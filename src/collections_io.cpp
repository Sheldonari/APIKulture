#include "collections_io.hpp"

#include <map>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace {

void trim_header_line_inplace(std::string& s) {
	auto not_space = [](unsigned char c) { return !std::isspace(c); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
	s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

/// Legacy `headers` stored as one string: one "Key: value" per line.
void parse_legacy_headers_string(const std::string& text, std::vector<apikulture::HeaderRow>& out) {
	std::istringstream stream(text);
	std::string line;
	while (std::getline(stream, line)) {
		const size_t colon = line.find(':');
		if (colon == std::string::npos) continue;
		std::string key = line.substr(0, colon);
		std::string value = line.substr(colon + 1);
		trim_header_line_inplace(key);
		trim_header_line_inplace(value);
		if (!key.empty()) out.push_back(apikulture::HeaderRow{std::move(key), std::move(value), true});
	}
}

void parse_variables_json(const nlohmann::json& vars_j, std::vector<std::pair<std::string, std::string>>& out) {
	out.clear();
	if (!vars_j.is_object() && !vars_j.is_array()) return;
	if (vars_j.is_object()) {
		for (auto it = vars_j.begin(); it != vars_j.end(); ++it) {
			if (it.value().is_string()) {
				out.emplace_back(it.key(), it.value().get<std::string>());
			}
		}
		std::sort(out.begin(), out.end(),
				[](const auto& a, const auto& b) { return a.first < b.first; });
	} else {
		for (const auto& row : vars_j) {
			if (!row.is_object()) continue;
			const std::string k = row.value("key", std::string());
			const std::string v = row.value("value", std::string());
			if (!k.empty()) out.emplace_back(k, v);
		}
	}
}

}  // namespace

namespace apikulture {

void to_json(nlohmann::json& j, const Environment& e) {
	j = nlohmann::json{{"name", e.name}, {"base_url", e.base_url}};
}

void from_json(const nlohmann::json& j, Environment& e) {
	e.name = j.value("name", std::string("Default"));
	e.base_url = j.value("base_url", std::string());
}

void to_json(nlohmann::json& j, const RequestItem& r) {
	nlohmann::json qp = nlohmann::json::array();
	for (const auto& p : r.query_params) {
		qp.push_back(nlohmann::json{{"key", p.key}, {"value", p.value}, {"enabled", p.enabled}});
	}
	nlohmann::json hdr = nlohmann::json::array();
	for (const auto& h : r.request_headers) {
		hdr.push_back(nlohmann::json{{"key", h.key}, {"value", h.value}, {"enabled", h.enabled}});
	}
	j = nlohmann::json{{"name", r.name},
	                     {"method", r.method},
	                     {"url", r.url},
	                     {"query_params", qp},
	                     {"headers", std::move(hdr)},
	                     {"body", r.body},
	                     {"body_kind", r.body_kind},
	                     {"jsonpath", r.jsonpath},
	                     {"last_response_status", r.last_response_status},
	                     {"last_response_headers", r.last_response_headers},
	                     {"last_response_body_raw", r.last_response_body_raw},
	                     {"last_response_content_type", r.last_response_content_type},
	                     {"last_response_error", r.last_response_error}};
}

void from_json(const nlohmann::json& j, RequestItem& r) {
	r.name = j.value("name", std::string("New request"));
	r.method = j.value("method", std::string("GET"));
	r.url = j.value("url", std::string());
	r.query_params.clear();
	if (j.contains("query_params") && j["query_params"].is_array()) {
		for (const auto& row : j["query_params"]) {
			if (!row.is_object()) continue;
			QueryParam qp;
			qp.key = row.value("key", std::string());
			qp.value = row.value("value", std::string());
			qp.enabled = row.value("enabled", true);
			r.query_params.push_back(std::move(qp));
		}
	}
	r.request_headers.clear();
	if (j.contains("headers")) {
		const auto& hj = j["headers"];
		if (hj.is_array()) {
			for (const auto& row : hj) {
				if (!row.is_object()) continue;
				HeaderRow hr;
				hr.key = row.value("key", std::string());
				hr.value = row.value("value", std::string());
				hr.enabled = row.value("enabled", true);
				r.request_headers.push_back(std::move(hr));
			}
		} else if (hj.is_string()) {
			parse_legacy_headers_string(hj.get<std::string>(), r.request_headers);
		}
	}
	r.body = j.value("body", std::string());
	r.body_kind = j.value("body_kind", std::string("json"));
	if (r.body_kind != "json" && r.body_kind != "text" && r.body_kind != "form") r.body_kind = "json";
	r.jsonpath = j.value("jsonpath", std::string());
	r.last_response_status = j.value("last_response_status", std::string());
	r.last_response_headers = j.value("last_response_headers", std::string());
	r.last_response_body_raw = j.value("last_response_body_raw", std::string());
	r.last_response_content_type = j.value("last_response_content_type", std::string());
	r.last_response_error = j.value("last_response_error", std::string());
}

void to_json(nlohmann::json& j, const Collection& c) {
	nlohmann::json vars = nlohmann::json::object();
	for (const auto& kv : c.variables) {
		vars[kv.first] = kv.second;
	}
	j = nlohmann::json{{"name", c.name},
	                     {"variables", std::move(vars)},
	                     {"environments", c.environments},
	                     {"active_environment_index", c.active_environment_index},
	                     {"items", c.items},
	                     {"last_selected_request_index", c.last_selected_request_index}};
}

void from_json(const nlohmann::json& j, Collection& c) {
	c.name = j.value("name", std::string("Collection"));
	c.environments.clear();
	if (j.contains("environments") && j["environments"].is_array()) {
		for (const auto& je : j["environments"]) {
			Environment e;
			from_json(je, e);
			if (e.name.empty()) e.name = "Environment";
			c.environments.push_back(std::move(e));
		}
	}
	c.active_environment_index = j.value("active_environment_index", 0);
	c.items.clear();
	if (j.contains("items") && j["items"].is_array()) {
		for (const auto& it : j["items"]) {
			RequestItem r;
			from_json(it, r);
			c.items.push_back(std::move(r));
		}
	}
	if (c.items.empty()) {
		c.items.push_back(RequestItem{});
		c.items.back().name = "Request 1";
	}
	c.last_selected_request_index = j.value("last_selected_request_index", 0);
	if (!c.items.empty()) {
		if (c.last_selected_request_index < 0
				|| c.last_selected_request_index >= static_cast<int>(c.items.size())) {
			c.last_selected_request_index = 0;
		}
	} else {
		c.last_selected_request_index = 0;
	}
	if (!c.environments.empty()) {
		if (c.active_environment_index < 0
				|| c.active_environment_index >= static_cast<int>(c.environments.size())) {
			c.active_environment_index = 0;
		}
	}

	c.variables.clear();
	if (j.contains("variables")) {
		parse_variables_json(j["variables"], c.variables);
	} else if (j.contains("environments") && j["environments"].is_array()) {
		// Legacy: variables lived on each environment — merge into one collection-wide map (later env wins on key clash).
		std::map<std::string, std::string> merged;
		for (const auto& je : j["environments"]) {
			if (!je.contains("variables")) continue;
			std::vector<std::pair<std::string, std::string>> chunk;
			parse_variables_json(je["variables"], chunk);
			for (const auto& kv : chunk) merged[kv.first] = kv.second;
		}
		c.variables.assign(merged.begin(), merged.end());
	}
}

}  // namespace apikulture

namespace apikulture::collections_io {

using apikulture::Collection;
using apikulture::Environment;
using apikulture::RequestItem;
using apikulture::Workspace;

std::string default_config_path() {
	const char* xdg = std::getenv("XDG_CONFIG_HOME");
	if (xdg && xdg[0]) {
		return (fs::path(xdg) / "apikulture" / "collections.json").string();
	}
	const char* home = std::getenv("HOME");
	if (home && home[0]) {
		return (fs::path(home) / ".config" / "apikulture" / "collections.json").string();
	}
	return "collections.json";
}

std::string local_environments_path() {
	const std::string path = default_config_path();
	fs::path p(path);
	p.replace_filename("local-environments.json");
	return p.string();
}

static Workspace make_default_workspace() {
	Workspace w;
	Collection c;
	c.name = "Default";
	c.items.push_back(RequestItem{});
	c.items[0].name = "Request 1";
	w.collections = {std::move(c)};
	return w;
}

static void parse_collections_array(const nlohmann::json& root, Workspace& w) {
	w.collections.clear();
	if (!root.contains("collections") || !root["collections"].is_array()) {
		Collection c;
		c.name = "Default";
		c.items.push_back(RequestItem{});
		c.items[0].name = "Request 1";
		w.collections.push_back(std::move(c));
		return;
	}
	for (const auto& jc : root["collections"]) {
		Collection col;
		apikulture::from_json(jc, col);
		w.collections.push_back(std::move(col));
	}
	if (w.collections.empty()) {
		Collection c;
		c.name = "Default";
		c.items.push_back(RequestItem{});
		c.items[0].name = "Request 1";
		w.collections.push_back(std::move(c));
	}
}

/// Old files stored `environments` at workspace root; copy into any collection that has none (v1/v2 → v3).
static void migrate_legacy_workspace_environments_to_collections(const nlohmann::json& root, Workspace& w) {
	std::vector<Environment> legacy;
	int legacy_idx = 0;
	if (root.contains("environments") && root["environments"].is_array() && !root["environments"].empty()) {
		for (const auto& je : root["environments"]) {
			Environment e;
			apikulture::from_json(je, e);
			if (e.name.empty()) e.name = "Environment";
			legacy.push_back(std::move(e));
		}
		legacy_idx = root.value("active_environment_index", 0);
		if (legacy_idx < 0 || legacy_idx >= static_cast<int>(legacy.size())) legacy_idx = 0;
	}
	for (auto& col : w.collections) {
		if (col.environments.empty() && !legacy.empty()) {
			col.environments = legacy;
			col.active_environment_index = legacy_idx;
		}
		if (col.environments.empty()) {
			Environment def;
			def.name = "Default";
			col.environments.push_back(std::move(def));
			col.active_environment_index = 0;
		} else if (col.active_environment_index < 0
				|| col.active_environment_index >= static_cast<int>(col.environments.size())) {
			col.active_environment_index = 0;
		}
	}
}

Workspace load_workspace_or_default() {
	const std::string path = default_config_path();
	std::ifstream in(path);
	if (!in) return make_default_workspace();

	try {
		nlohmann::json root;
		in >> root;
		Workspace w;
		const int version = root.value("version", 1);
		(void)version;
		parse_collections_array(root, w);
		migrate_legacy_workspace_environments_to_collections(root, w);
		w.last_selected_collection_index = root.value("last_selected_collection_index", 0);
		if (w.last_selected_collection_index < 0
				|| w.last_selected_collection_index >= static_cast<int>(w.collections.size())) {
			w.last_selected_collection_index = 0;
		}
		return w;
	} catch (...) {
		return make_default_workspace();
	}
}

bool save_workspace(const Workspace& workspace) {
	const std::string path = default_config_path();
	try {
		fs::create_directories(fs::path(path).parent_path());
		nlohmann::json root;
		root["version"] = 3;
		root["last_selected_collection_index"] = workspace.last_selected_collection_index;
		root["collections"] = workspace.collections;
		std::ofstream out(path, std::ios::trunc);
		if (!out) return false;
		out << root.dump(2);
		return true;
	} catch (...) {
		return false;
	}
}

std::map<std::string, std::map<std::string, std::string>> load_local_overrides() {
	std::map<std::string, std::map<std::string, std::string>> out;
	const std::string path = local_environments_path();
	std::ifstream in(path);
	if (!in) return out;
	try {
		nlohmann::json root;
		in >> root;
		if (!root.contains("overrides") || !root["overrides"].is_object()) return out;
		for (auto env_it = root["overrides"].begin(); env_it != root["overrides"].end(); ++env_it) {
			const std::string env_name = env_it.key();
			if (!env_it.value().is_object()) continue;
			std::map<std::string, std::string> vars;
			for (auto vit = env_it.value().begin(); vit != env_it.value().end(); ++vit) {
				if (vit.value().is_string()) vars[vit.key()] = vit.value().get<std::string>();
			}
			if (!vars.empty()) out[env_name] = std::move(vars);
		}
	} catch (...) {
	}
	return out;
}

}  // namespace apikulture::collections_io
