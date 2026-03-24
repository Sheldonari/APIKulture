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
	j = nlohmann::json{{"name", r.name},
	                     {"method", r.method},
	                     {"url", r.url},
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
	j = nlohmann::json{{"name", c.name}, {"items", c.items}};
}

void from_json(const nlohmann::json& j, Collection& c) {
	c.name = j.value("name", std::string("Collection"));
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
	w.environments.clear();
	Environment def;
	def.name = "Default";
	w.environments.push_back(std::move(def));
	w.active_environment_index = 0;
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
		Collection c{"Default", {}};
		c.items.push_back(RequestItem{});
		c.items[0].name = "Request 1";
		w.collections.push_back(std::move(c));
		return;
	}
	for (const auto& jc : root["collections"]) {
		Collection c;
		apikulture::from_json(jc, c);
		w.collections.push_back(std::move(c));
	}
	if (w.collections.empty()) {
		Collection c;
		c.name = "Default";
		c.items.push_back(RequestItem{});
		c.items[0].name = "Request 1";
		w.collections.push_back(std::move(c));
	}
}

static void migrate_environments(const nlohmann::json& root, Workspace& w) {
	w.environments.clear();
	if (root.contains("environments") && root["environments"].is_array() && !root["environments"].empty()) {
		for (const auto& je : root["environments"]) {
			Environment e;
			apikulture::from_json(je, e);
			if (e.name.empty()) e.name = "Environment";
			w.environments.push_back(std::move(e));
		}
	} else {
		Environment e;
		e.name = "Default";
		w.environments.push_back(std::move(e));
	}
	w.active_environment_index = root.value("active_environment_index", 0);
	if (w.active_environment_index < 0
			|| w.active_environment_index >= static_cast<int>(w.environments.size())) {
		w.active_environment_index = 0;
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
		migrate_environments(root, w);
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
		root["version"] = 2;
		root["active_environment_index"] = workspace.active_environment_index;
		root["environments"] = workspace.environments;
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
