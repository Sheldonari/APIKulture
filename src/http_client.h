#ifndef APIKULTURE_HTTP_CLIENT_H
#define APIKULTURE_HTTP_CLIENT_H

#include <atomic>
#include <string>
#include <vector>
#include <utility>

struct HttpResponse {
	int status = 0;
	std::string status_line;
	std::vector<std::pair<std::string, std::string>> headers;
	std::string body;
	std::string error;  // non-empty if request failed
};

// Execute HTTP request. Blocking. Run from worker thread.
HttpResponse execute(const std::string& method,
	const std::string& url,
	const std::vector<std::pair<std::string, std::string>>& headers,
	const std::string& body,
	std::atomic<bool>* cancelled = nullptr);

#endif  // APIKULTURE_HTTP_CLIENT_H
