#include "collections_io.hpp"

#include <cstdlib>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace apikulture {

void to_json(nlohmann::json& j, const RequestItem& r) {
	j = nlohmann::json{{"name", r.name},
	                     {"method", r.method},
	                     {"url", r.url},
	                     {"headers", r.headers},
	                     {"body", r.body},
	                     {"jsonpath", r.jsonpath}};
}

void from_json(const nlohmann::json& j, RequestItem& r) {
	r.name = j.value("name", std::string("New request"));
	r.method = j.value("method", std::string("GET"));
	r.url = j.value("url", std::string());
	r.headers = j.value("headers", std::string());
	r.body = j.value("body", std::string());
	r.jsonpath = j.value("jsonpath", std::string());
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
using apikulture::RequestItem;

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

static std::vector<Collection> make_default() {
	Collection c;
	c.name = "Default";
	c.items.push_back(RequestItem{});
	c.items[0].name = "Request 1";
	return {std::move(c)};
}

std::vector<Collection> load_or_default() {
	const std::string path = default_config_path();
	std::ifstream in(path);
	if (!in) return make_default();

	try {
		nlohmann::json root;
		in >> root;
		if (!root.contains("collections") || !root["collections"].is_array()) {
			return make_default();
		}
		std::vector<Collection> out;
		for (const auto& jc : root["collections"]) {
			Collection c;
			apikulture::from_json(jc, c);
			out.push_back(std::move(c));
		}
		if (out.empty()) return make_default();
		return out;
	} catch (...) {
		return make_default();
	}
}

bool save(const std::vector<Collection>& collections) {
	const std::string path = default_config_path();
	try {
		fs::create_directories(fs::path(path).parent_path());
		nlohmann::json root;
		root["version"] = 1;
		root["collections"] = collections;
		std::ofstream out(path, std::ios::trunc);
		if (!out) return false;
		out << root.dump(2);
		return true;
	} catch (...) {
		return false;
	}
}

}  // namespace apikulture::collections_io
