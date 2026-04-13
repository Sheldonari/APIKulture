#include "http_client.h"
#include "httplib.h"
#include <cctype>
#include <sstream>
#include <algorithm>

#define CA_CERT_FILE "./ca-bundle.crt"

std::string trim(const std::string& s) {
	auto start = s.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) return "";
	auto end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end == std::string::npos ? std::string::npos : end - start + 1);
}

namespace {

bool headers_have_content_type(const std::vector<std::pair<std::string, std::string>>& headers) {
	for (const auto& h : headers) {
		std::string k = trim(h.first);
		for (auto& c : k) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		if (k == "content-type") return true;
	}
	return false;
}

// cpp-httplib always calls req.set_header("Content-Type", content_type) when content_type is non-empty,
// which overwrites any Content-Type the user supplied in `headers`. Pass "" so only user headers apply.
std::string httplib_content_type_arg(const std::vector<std::pair<std::string, std::string>>& headers,
		const std::string& body,
		const char* default_for_non_empty_body) {
	if (headers_have_content_type(headers)) return {};
	if (body.empty()) return {};
	return std::string(default_for_non_empty_body);
}

}  // namespace

void parse_url(const std::string& url, std::string& scheme, std::string& host, int& port, std::string& path) {
	scheme = "https";
	host.clear();
	port = 443;
	path = "/";

	size_t i = 0;
	if (url.size() >= 8 && (url.substr(0, 7) == "http://" || url.substr(0, 8) == "https://")) {
		if (url[4] == 's') {
			scheme = "https";
			port = 443;
			i = 8;
		} else {
			scheme = "http";
			port = 80;
			i = 7;
		}
	}

	size_t host_end = url.find('/', i);
	if (host_end == std::string::npos) {
		host_end = url.size();
	} else {
		path = url.substr(host_end);
		if (path.empty()) path = "/";
	}

	std::string host_port = url.substr(i, host_end - i);
	size_t colon = host_port.find(':');
	if (colon != std::string::npos) {
		host = host_port.substr(0, colon);
		try {
			port = std::stoi(host_port.substr(colon + 1));
		} catch (...) {}
	} else {
		host = host_port;
	}
}

httplib::Headers to_httplib_headers(const std::vector<std::pair<std::string, std::string>>& headers) {
	httplib::Headers out;
	for (const auto& h : headers) {
		out.emplace(trim(h.first), trim(h.second));
	}
	return out;
}

HttpResponse execute(const std::string& method,
										 const std::string& url,
										 const std::vector<std::pair<std::string, std::string>>& headers,
										 const std::string& body,
										 std::atomic<bool>* cancelled) {
	HttpResponse result;

	std::string scheme, host, path;
	int port;
	parse_url(url, scheme, host, port, path);
	if (host.empty()) {
		result.error = "Invalid URL";
		return result;
	}
	// Ensure host has valid storage so httplib (SSLClient) never sees null from &host_[0]
	std::string safe_host;
	if (host.data() != nullptr && host.size() > 0) {
		safe_host.assign(host.data(), host.size());
	}
	if (safe_host.empty()) {
		result.error = "Invalid URL";
		return result;
	}

	if (cancelled && cancelled->load()) {
		result.error = "Cancelled";
		return result;
	}

	httplib::Headers req_headers = to_httplib_headers(headers);
	std::string method_upper = method;
	std::transform(method_upper.begin(), method_upper.end(), method_upper.begin(), ::toupper);

	auto run_client = [&](auto& client) -> bool {
		client.set_connection_timeout(10, 0);
		client.set_read_timeout(30, 0);
		client.set_write_timeout(30, 0);

		const std::string json_ct = httplib_content_type_arg(headers, body, "application/json");
		const std::string octet_ct = httplib_content_type_arg(headers, body, "application/octet-stream");

		auto do_req = [&]() -> decltype(client.Get(path, req_headers)) {
			if (method_upper == "GET") return client.Get(path, req_headers);
			if (method_upper == "POST") return client.Post(path, req_headers, body, json_ct);
			if (method_upper == "PUT") return client.Put(path, req_headers, body, json_ct);
			if (method_upper == "PATCH") return client.Patch(path, req_headers, body, json_ct);
			if (method_upper == "DELETE") return client.Delete(path, req_headers);
			if (method_upper == "HEAD") return client.Head(path, req_headers);
			if (method_upper == "OPTIONS") return client.Options(path, req_headers);
			return client.Post(path, req_headers, body, octet_ct);
		};
		auto res = do_req();

		if (!res) {
			result.error = "Request failed";
			return false;
		}
		result.status = res->status;
		result.status_line = std::to_string(res->status);
		if (res->reason.data() != nullptr && res->reason.size() > 0) {
			result.status_line += " ";
			result.status_line.append(res->reason.data(), res->reason.size());
		}
		for (const auto& h : res->headers) {
			const char* kp = h.first.data();
			const char* vp = h.second.data();
			result.headers.emplace_back(
					(kp && h.first.size()) ? std::string(kp, h.first.size()) : std::string(),
					(vp && h.second.size()) ? std::string(vp, h.second.size()) : std::string());
		}
		if (res->body.data() != nullptr) {
			result.body.assign(res->body.data(), res->body.size());
		} else {
			result.body.clear();
		}
		return true;
	};

	if (scheme == "https") {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
		httplib::SSLClient client(safe_host, port);
		client.set_ca_cert_path(CA_CERT_FILE);
		client.enable_server_certificate_verification(false);  // allow self-signed for dev
		if (!run_client(client)) return result;
#else
		result.error = "HTTPS not supported (build with OpenSSL)";
		return result;
#endif
	} else {
		httplib::Client client(safe_host, port);
		if (!run_client(client)) return result;
	}

	return result;
}
