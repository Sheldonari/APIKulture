#include "app_state.h"
#include "http_client.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <slint_models.h>

namespace {

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

void trim_in_place(std::string& s) {
	auto not_space = [](unsigned char c) { return !std::isspace(c); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
	s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

}  // namespace

AppState::AppState(const MainWindowHandle& ui) : ui_(ui) {
	collections_ = apikulture::collections_io::load_or_default();
	if (collection_index_ >= static_cast<int>(collections_.size())) {
		collection_index_ = 0;
	}
	if (!collections_.empty()) {
		auto& col = collections_[collection_index_];
		if (request_index_ >= static_cast<int>(col.items.size())) {
			request_index_ = 0;
		}
	}
}

AppState::~AppState() {
	commit_form_to_current_item();
	apikulture::collections_io::save(collections_);
	cancel_request();
	{
		std::lock_guard<std::mutex> lock(mutex_);
		shutdown_ = true;
	}
	cv_.notify_one();
	if (worker_.joinable()) worker_.join();
}

std::shared_ptr<slint::VectorModel<slint::SharedString>> AppState::make_name_model(
		const std::vector<std::string>& names) {
	auto m = std::make_shared<slint::VectorModel<slint::SharedString>>();
	for (const auto& n : names) {
		m->push_back(slint::SharedString(n));
	}
	return m;
}

void AppState::refresh_collection_names_model() {
	std::vector<std::string> names;
	names.reserve(collections_.size());
	for (const auto& c : collections_) {
		names.push_back(c.name);
	}
	auto& g = ui_->global<AppLogic>();
	g.set_collection_names(make_name_model(names));
}

void AppState::refresh_request_names_model() {
	std::vector<std::string> names;
	if (collection_index_ >= 0 && collection_index_ < static_cast<int>(collections_.size())) {
		for (const auto& r : collections_[collection_index_].items) {
			names.push_back(r.name);
		}
	}
	auto& g = ui_->global<AppLogic>();
	g.set_request_names(make_name_model(names));
}

void AppState::push_name_edits_to_ui() {
	auto& g = ui_->global<AppLogic>();
	if (collections_.empty()) {
		g.set_collection_name_edit(slint::SharedString(""));
		g.set_request_name_edit(slint::SharedString(""));
		return;
	}
	if (collection_index_ >= 0 && collection_index_ < static_cast<int>(collections_.size())) {
		g.set_collection_name_edit(slint::SharedString(collections_[static_cast<size_t>(collection_index_)].name));
		const auto& col = collections_[static_cast<size_t>(collection_index_)];
		if (!col.items.empty() && request_index_ >= 0
				&& request_index_ < static_cast<int>(col.items.size())) {
			g.set_request_name_edit(slint::SharedString(col.items[static_cast<size_t>(request_index_)].name));
		} else {
			g.set_request_name_edit(slint::SharedString(""));
		}
	}
}

void AppState::push_selection_to_ui() {
	auto& g = ui_->global<AppLogic>();
	g.set_selected_collection_index(collection_index_);
	g.set_selected_request_index(request_index_);
	push_name_edits_to_ui();
}

void AppState::commit_form_to_current_item() {
	if (collections_.empty()) return;
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(collections_.size())) return;
	auto& col = collections_[collection_index_];
	if (col.items.empty()) return;
	if (request_index_ < 0 || request_index_ >= static_cast<int>(col.items.size())) return;

	auto& g = ui_->global<AppLogic>();
	auto& item = col.items[static_cast<size_t>(request_index_)];
	item.method = to_std_string(g.get_method());
	item.url = to_std_string(g.get_url());
	item.headers = to_std_string(g.get_request_headers());
	item.body = to_std_string(g.get_request_body());
	// Keep list label in sync if user only edited URL (optional: derive name from URL — skip for MVP)
}

void AppState::apply_form_from_current_item() {
	if (collections_.empty()) return;
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(collections_.size())) return;
	auto& col = collections_[collection_index_];
	if (col.items.empty()) return;
	if (request_index_ < 0 || request_index_ >= static_cast<int>(col.items.size())) return;

	const auto& item = col.items[static_cast<size_t>(request_index_)];
	auto& g = ui_->global<AppLogic>();
	g.set_method(slint::SharedString(item.method));
	g.set_url(slint::SharedString(item.url));
	g.set_request_headers(slint::SharedString(item.headers));
	g.set_request_body(slint::SharedString(item.body));
	g.set_response_status(slint::SharedString(""));
	g.set_response_headers(slint::SharedString(""));
	g.set_response_body(slint::SharedString(""));
}

void AppState::init_collections_ui() {
	refresh_collection_names_model();
	refresh_request_names_model();
	push_selection_to_ui();
	apply_form_from_current_item();
}

void AppState::select_collection(int index) {
	if (index < 0 || index >= static_cast<int>(collections_.size())) return;
	commit_form_to_current_item();
	collection_index_ = index;
	request_index_ = 0;
	if (!collections_[static_cast<size_t>(collection_index_)].items.empty()) {
		if (request_index_ >= static_cast<int>(collections_[static_cast<size_t>(collection_index_)].items.size())) {
			request_index_ = 0;
		}
	}
	refresh_request_names_model();
	push_selection_to_ui();
	apply_form_from_current_item();
}

void AppState::select_request(int index) {
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(collections_.size())) return;
	auto& col = collections_[static_cast<size_t>(collection_index_)];
	if (index < 0 || index >= static_cast<int>(col.items.size())) return;
	commit_form_to_current_item();
	request_index_ = index;
	push_selection_to_ui();
	apply_form_from_current_item();
}

void AppState::new_collection() {
	commit_form_to_current_item();
	apikulture::Collection c;
	c.name = "Collection " + std::to_string(collections_.size() + 1);
	apikulture::RequestItem r;
	r.name = "Request 1";
	c.items.push_back(std::move(r));
	collections_.push_back(std::move(c));
	collection_index_ = static_cast<int>(collections_.size()) - 1;
	request_index_ = 0;
	refresh_collection_names_model();
	refresh_request_names_model();
	push_selection_to_ui();
	apply_form_from_current_item();
}

void AppState::new_request() {
	if (collections_.empty()) return;
	commit_form_to_current_item();
	auto& col = collections_[static_cast<size_t>(collection_index_)];
	apikulture::RequestItem r;
	r.name = std::string("Request ") + std::to_string(col.items.size() + 1);
	col.items.push_back(std::move(r));
	request_index_ = static_cast<int>(col.items.size()) - 1;
	refresh_request_names_model();
	push_selection_to_ui();
	apply_form_from_current_item();
}

void AppState::delete_request() {
	if (collections_.empty()) return;
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(collections_.size())) return;
	auto& col = collections_[static_cast<size_t>(collection_index_)];
	if (col.items.empty()) return;
	if (col.items.size() <= 1) {
		col.items[0] = apikulture::RequestItem{};
		col.items[0].name = "Request 1";
		request_index_ = 0;
	} else {
		col.items.erase(col.items.begin() + request_index_);
		if (request_index_ >= static_cast<int>(col.items.size())) {
			request_index_ = static_cast<int>(col.items.size()) - 1;
		}
	}
	refresh_request_names_model();
	push_selection_to_ui();
	apply_form_from_current_item();
}

void AppState::duplicate_request() {
	if (collections_.empty()) return;
	commit_form_to_current_item();
	auto& col = collections_[static_cast<size_t>(collection_index_)];
	if (request_index_ < 0 || request_index_ >= static_cast<int>(col.items.size())) return;
	apikulture::RequestItem copy = col.items[static_cast<size_t>(request_index_)];
	copy.name = copy.name + " copy";
	col.items.insert(col.items.begin() + request_index_ + 1, std::move(copy));
	request_index_++;
	refresh_request_names_model();
	push_selection_to_ui();
	apply_form_from_current_item();
}

void AppState::save_collections() {
	commit_form_to_current_item();
	(void)apikulture::collections_io::save(collections_);
}

void AppState::commit_collection_name() {
	if (collections_.empty()) return;
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(collections_.size())) return;
	auto& g = ui_->global<AppLogic>();
	std::string name = to_std_string(g.get_collection_name_edit());
	trim_in_place(name);
	if (name.empty()) {
		push_name_edits_to_ui();
		return;
	}
	collections_[static_cast<size_t>(collection_index_)].name = std::move(name);
	refresh_collection_names_model();
	push_selection_to_ui();
}

void AppState::commit_request_name() {
	if (collections_.empty()) return;
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(collections_.size())) return;
	auto& col = collections_[static_cast<size_t>(collection_index_)];
	if (col.items.empty()) return;
	if (request_index_ < 0 || request_index_ >= static_cast<int>(col.items.size())) return;
	auto& g = ui_->global<AppLogic>();
	std::string name = to_std_string(g.get_request_name_edit());
	trim_in_place(name);
	if (name.empty()) {
		push_name_edits_to_ui();
		return;
	}
	col.items[static_cast<size_t>(request_index_)].name = std::move(name);
	refresh_request_names_model();
	push_selection_to_ui();
}

void AppState::send_request() {
	if (worker_busy_) return;
	commit_form_to_current_item();

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
