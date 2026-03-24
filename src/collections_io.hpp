#ifndef APIKULTURE_COLLECTIONS_IO_H
#define APIKULTURE_COLLECTIONS_IO_H

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>

#include "environment.hpp"

namespace apikulture {

struct RequestItem {
	std::string name{"New request"};
	std::string method{"GET"};
	std::string url;
	/// GET-style query parameters (applied to the URL on send; supports {{var}} in keys/values).
	std::vector<std::pair<std::string, std::string>> query_params;
	std::string headers;
	std::string body;
	/// Optional JSONPath applied to the response body when displaying (e.g. "$.data.items[*]").
	std::string jsonpath;
	/// Last completed HTTP response for this request (persisted across sessions).
	std::string last_response_status;
	std::string last_response_headers;
	/// Raw response body from the last successful request; used with `jsonpath` to rebuild the body view.
	std::string last_response_body_raw;
	/// Value of the last successful response `Content-Type` header (for syntax highlighting after reload).
	std::string last_response_content_type;
	/// If non-empty, last response was an error; message shown as response body (raw body unused).
	std::string last_response_error;
};

struct Collection {
	std::string name{"Collection"};
	std::vector<RequestItem> items;
};

/// All persisted workspace data (collections + environments). Root JSON file is `collections.json`.
struct Workspace {
	std::vector<Environment> environments{{Environment{}}};
	int active_environment_index{0};
	std::vector<Collection> collections;
};

namespace collections_io {

std::string default_config_path();
/// Same directory as `collections.json`; gitignore-friendly optional secrets (`local-environments.json`).
std::string local_environments_path();

/// Load workspace from disk; on failure returns defaults (one collection, one "Default" env).
Workspace load_workspace_or_default();

/// Saves workspace (does not write `local-environments.json`).
bool save_workspace(const Workspace& workspace);

/// Optional per-environment variable overrides; merge wins over shared `collections.json` values.
std::map<std::string, std::map<std::string, std::string>> load_local_overrides();

}  // namespace collections_io

void to_json(nlohmann::json& j, const RequestItem& r);
void from_json(const nlohmann::json& j, RequestItem& r);
void to_json(nlohmann::json& j, const Collection& c);
void from_json(const nlohmann::json& j, Collection& c);
void to_json(nlohmann::json& j, const Environment& e);
void from_json(const nlohmann::json& j, Environment& e);

}  // namespace apikulture

#endif
