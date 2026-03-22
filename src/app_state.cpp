#include "app_state.h"
#include "http_client.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <string_view>
#include <algorithm>

std::vector<std::pair<std::string, std::string>> parse_headers(const std::string& text) {
	std::vector<std::pair<std::string, std::string>> out;
	std::istringstream stream(text);
	std::string line;
	while (std::getline(stream, line)) {
		size_t colon = line.find(':');
		if (colon != std::string::npos) {
			std::string key = line.substr(0, colon);
			std::string value = line.substr(colon + 1);
			auto trim = [](std::string& s) {
				s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); }));
				s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), s.end());
			};
			trim(key);
			trim(value);
			if (!key.empty()) out.emplace_back(std::move(key), std::move(value));
		}
	}
	return out;
}

std::string format_headers(const std::vector<std::pair<std::string, std::string>>& headers) {
	std::ostringstream out;
	for (const auto& h : headers) {
		out << h.first << ": " << h.second << "\n";
	}
	return out.str();
}

std::string try_pretty_print_json(const std::string& body) {
	try {
		auto j = nlohmann::json::parse(body);
		return j.dump(2);
	} catch (...) {
		return body;
	}
}

// Safe SharedString -> std::string (avoids std::string(nullptr) which throws)
std::string to_std_string(const slint::SharedString& ss) {
	std::string_view v;
	try {
		v = static_cast<std::string_view>(ss);
	} catch (...) {
		return std::string();
	}
	if (v.data() == nullptr || v.size() == 0) return std::string();
	return std::string(v.data(), v.size());
}

AppState::AppState(const MainWindowHandle& ui) : ui_(ui) {}

AppState::~AppState() {
	cancel_request();
	{
		std::lock_guard<std::mutex> lock(mutex_);
		shutdown_ = true;
	}
	cv_.notify_one();
	if (worker_.joinable()) worker_.join();
}

void AppState::send_request() {
	if (worker_busy_) return;

	auto& g = ui_->global<AppLogic>();
	pending_method_ = to_std_string(g.get_method());
	pending_url_ = to_std_string(g.get_url());
	pending_headers_ = to_std_string(g.get_request_headers());
	pending_body_ = to_std_string(g.get_request_body());

	if (pending_url_.empty()) {
		g.set_response_status(slint::SharedString(""));
		g.set_response_headers(slint::SharedString(""));
		g.set_response_body(slint::SharedString("Enter a URL"));
		return;
	}

	cancelled_ = false;
	g.set_loading(true);
	g.set_response_status(slint::SharedString("Loading..."));
	g.set_response_headers(slint::SharedString(""));
	g.set_response_body(slint::SharedString(""));

	{
		std::lock_guard<std::mutex> lock(mutex_);
		pending_work_ = true;
	}
	cv_.notify_one();

	if (!worker_.joinable()) {
		worker_ = std::thread(&AppState::worker_run, this);
	}
}

void AppState::cancel_request() {
	cancelled_ = true;
}

void AppState::worker_run() {
	while (true) {
		std::string method, url, headers_text, body;
		{
			std::unique_lock<std::mutex> lock(mutex_);
			cv_.wait(lock, [this] { return pending_work_ || shutdown_; });
			if (shutdown_) break;
			if (!pending_work_) continue;
			worker_busy_ = true;
			method = pending_method_;
			url = pending_url_;
			headers_text = pending_headers_;
			body = pending_body_;
			pending_work_ = false;
		}

		auto headers = parse_headers(headers_text);
		HttpResponse res = execute(method, url, headers, body, &cancelled_);

		slint::invoke_from_event_loop([this, res]() {
			auto& g = ui_->global<AppLogic>();
			g.set_loading(false);
			if (!res.error.empty()) {
				g.set_response_status(slint::SharedString("Error: " + res.error));
				g.set_response_headers(slint::SharedString(""));
				g.set_response_body(slint::SharedString(res.error));
			} else {
				g.set_response_status(slint::SharedString(res.status_line));
				g.set_response_headers(slint::SharedString(format_headers(res.headers)));
				g.set_response_body(slint::SharedString(try_pretty_print_json(res.body)));
			}
		});

		worker_busy_ = false;
	}
}

