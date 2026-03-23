#ifndef APIKULTURE_COLLECTIONS_IO_H
#define APIKULTURE_COLLECTIONS_IO_H

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace apikulture {

struct RequestItem {
	std::string name{"New request"};
	std::string method{"GET"};
	std::string url;
	std::string headers;
	std::string body;
	/// Optional JSONPath applied to the response body when displaying (e.g. "$.data.items[*]").
	std::string jsonpath;
	/// Last completed HTTP response for this request (persisted across sessions).
	std::string last_response_status;
	std::string last_response_headers;
	/// Raw response body from the last successful request; used with `jsonpath` to rebuild the body view.
	std::string last_response_body_raw;
	/// If non-empty, last response was an error; message shown as response body (raw body unused).
	std::string last_response_error;
};

struct Collection {
	std::string name{"Collection"};
	std::vector<RequestItem> items;
};

namespace collections_io {

std::string default_config_path();

/// Load collections from disk; on failure returns a single default collection.
std::vector<Collection> load_or_default();

/// Save all collections; returns false on I/O error.
bool save(const std::vector<Collection>& collections);

}  // namespace collections_io

void to_json(nlohmann::json& j, const RequestItem& r);
void from_json(const nlohmann::json& j, RequestItem& r);
void to_json(nlohmann::json& j, const Collection& c);
void from_json(const nlohmann::json& j, Collection& c);

}  // namespace apikulture

#endif
