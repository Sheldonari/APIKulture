#include "collections_io.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace apikulture {

void to_json(nlohmann::json& j, const Environment& e) {
	nlohmann::json vars = nlohmann::json::object();
	for (const auto& kv : e.variables) {
		vars[kv.first] = kv.second;
	}
	j = nlohmann::json{{"name", e.name},
	                     {"base_url", e.base_url},
	                     {"variables", std::move(vars)}};
}

void from_json(const nlohmann::json& j, Environment& e) {
	e.name = j.value("name", std::string("Default"));
	e.base_url = j.value("base_url", std::string());
	e.variables.clear();
	if (j.contains("variables")) {
		if (j["variables"].is_object()) {
			for (auto it = j["variables"].begin(); it != j["variables"].end(); ++it) {
				if (it.value().is_string()) {
					e.variables.emplace_back(it.key(), it.value().get<std::string>());
				}
			}
			std::sort(e.variables.begin(), e.variables.end(),
					[](const auto& a, const auto& b) { return a.first < b.first; });
		} else if (j["variables"].is_array()) {
			for (const auto& row : j["variables"]) {
				if (!row.is_object()) continue;
				const std::string k = row.value("key", std::string());
				const std::string v = row.value("value", std::string());
				if (!k.empty()) e.variables.emplace_back(k, v);
			}
		}
	}
}

void to_json(nlohmann::json& j, const RequestItem& r) {
	nlohmann::json qp = nlohmann::json::array();
	for (const auto& p : r.query_params) {
		qp.push_back(nlohmann::json{{"key", p.first}, {"value", p.second}});
	}
	j = nlohmann::json{{"name", r.name},
	                     {"method", r.method},
	                     {"url", r.url},
	                     {"query_params", qp},
	                     {"headers", r.headers},
	                     {"body", r.body},
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
			r.query_params.emplace_back(row.value("key", std::string()), row.value("value", std::string()));
		}
	}
	r.headers = j.value("headers", std::string());
	r.body = j.value("body", std::string());
	r.jsonpath = j.value("jsonpath", std::string());
	r.last_response_status = j.value("last_response_status", std::string());
	r.last_response_headers = j.value("last_response_headers", std::string());
	r.last_response_body_raw = j.value("last_response_body_raw", std::string());
	r.last_response_content_type = j.value("last_response_content_type", std::string());
	r.last_response_error = j.value("last_response_error", std::string());
}

void to_json(nlohmann::json& j, const Collection& c) {
	j = nlohmann::json{{"name", c.name},
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
