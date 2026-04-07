#include "openapi_import.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <optional>

#include <nlohmann/json.hpp>

namespace apikulture::openapi {

namespace {

std::string trim(std::string s) {
	auto not_space = [](unsigned char c) { return !std::isspace(c); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
	s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
	return s;
}

bool looks_like_yaml(const std::string& content) {
	for (unsigned char uc : content) {
		char c = static_cast<char>(uc);
		if (!std::isspace(uc)) {
			return c != '{';
		}
	}
	return false;
}

std::string read_file_utf8(const std::string& path_utf8) {
	std::ifstream in(path_utf8, std::ios::binary);
	if (!in) return {};
	std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF
			&& static_cast<unsigned char>(content[1]) == 0xBB
			&& static_cast<unsigned char>(content[2]) == 0xBF) {
		content.erase(0, 3);
	}
	return content;
}

nlohmann::json resolve_ref(const nlohmann::json& root, const nlohmann::json& node) {
	if (!node.is_object() || !node.contains("$ref") || !node["$ref"].is_string()) {
		return node;
	}
	const std::string ref = node["$ref"].get<std::string>();
	if (ref.size() < 2 || ref[0] != '#') return node;
	// JSON Pointer–style: #/a/b/c
	nlohmann::json::json_pointer ptr(ref.substr(1));
	try {
		return root.at(ptr);
	} catch (...) {
		return node;
	}
}

std::string strip_trailing_slash(std::string s) {
	while (s.size() > 1 && s.back() == '/') s.pop_back();
	return s;
}

std::string http_origin_from_url(const std::string& url) {
	const size_t scheme = url.find("://");
	if (scheme == std::string::npos) return {};
	const size_t path_start = url.find('/', scheme + 3);
	if (path_start == std::string::npos) return url;
	return url.substr(0, path_start);
}

std::string resolve_primary_server_url(std::string server_url,
		const std::optional<std::string>& source_document_url) {
	server_url = trim(std::move(server_url));
	if (server_url.empty()) return server_url;
	if (server_url.size() >= 7 && server_url.compare(0, 7, "http://") == 0) {
		return strip_trailing_slash(std::move(server_url));
	}
	if (server_url.size() >= 8 && server_url.compare(0, 8, "https://") == 0) {
		return strip_trailing_slash(std::move(server_url));
	}
	if (!server_url.empty() && server_url[0] == '/' && source_document_url && !source_document_url->empty()) {
		const std::string origin = http_origin_from_url(*source_document_url);
		if (!origin.empty()) return strip_trailing_slash(origin + server_url);
	}
	return strip_trailing_slash(std::move(server_url));
}

std::string path_to_url_template(std::string path) {
	if (path.empty()) path = "/";
	if (path[0] != '/') path.insert(path.begin(), '/');
	std::string out;
	out.reserve(path.size() * 2);
	for (size_t i = 0; i < path.size(); ++i) {
		if (path[i] == '{') {
			size_t j = path.find('}', i);
			if (j != std::string::npos) {
				std::string name = path.substr(i + 1, j - i - 1);
				out += "{{";
				out += name;
				out += "}}";
				i = j;
				continue;
			}
		}
		out += path[i];
	}
	return out;
}

std::string http_method_upper(std::string_view m) {
	std::string s(m);
	for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	return s;
}

void collect_params(const nlohmann::json& root, const nlohmann::json& params_array,
		std::vector<nlohmann::json>& out) {
	if (!params_array.is_array()) return;
	for (const auto& p : params_array) {
		nlohmann::json r = resolve_ref(root, p);
		if (!r.is_object() || !r.contains("name") || !r.contains("in")) continue;
		out.push_back(std::move(r));
	}
}

void merge_params(std::vector<nlohmann::json>& base, const std::vector<nlohmann::json>& overlay) {
	for (const auto& p : overlay) {
		if (!p.is_object() || !p.contains("name") || !p.contains("in")) continue;
		const std::string name = p["name"].get<std::string>();
		const std::string in = p["in"].get<std::string>();
		bool replaced = false;
		for (auto& existing : base) {
			if (existing.is_object() && existing.value("name", "") == name
					&& existing.value("in", "") == in) {
				existing = p;
				replaced = true;
				break;
			}
		}
		if (!replaced) base.push_back(p);
	}
}

bool schema_includes_string_type(const nlohmann::json& sch) {
	if (!sch.is_object() || !sch.contains("type")) return false;
	const auto& t = sch["type"];
	if (t.is_string()) return t.get<std::string>() == "string";
	if (t.is_array()) {
		for (const auto& el : t) {
			if (el.is_string() && el.get<std::string>() == "string") return true;
		}
	}
	return false;
}

std::string param_example_value(const nlohmann::json& param) {
	if (param.contains("example")) {
		if (param["example"].is_string()) return param["example"].get<std::string>();
		return param["example"].dump();
	}
	if (param.contains("examples") && param["examples"].is_object()) {
		for (auto it = param["examples"].begin(); it != param["examples"].end(); ++it) {
			const auto& ex = it.value();
			if (ex.is_object() && ex.contains("value")) {
				if (ex["value"].is_string()) return ex["value"].get<std::string>();
				return ex["value"].dump();
			}
		}
	}
	if (param.contains("schema") && param["schema"].is_object()) {
		const auto& sch = param["schema"];
		if (sch.contains("default")) {
			if (sch["default"].is_string()) return sch["default"].get<std::string>();
			return sch["default"].dump();
		}
		if (schema_includes_string_type(sch) && sch.contains("enum") && sch["enum"].is_array()
				&& !sch["enum"].empty() && sch["enum"][0].is_string()) {
			return sch["enum"][0].get<std::string>();
		}
	}
	return {};
}

std::string body_from_media_object(const nlohmann::json& m) {
	if (!m.is_object()) return {};
	if (m.contains("example")) {
		if (m["example"].is_string()) return m["example"].get<std::string>();
		return m["example"].dump(2);
	}
	if (m.contains("examples") && m["examples"].is_object()) {
		for (auto e = m["examples"].begin(); e != m["examples"].end(); ++e) {
			if (e.value().is_object() && e.value().contains("value")) {
				const auto& v = e.value()["value"];
				if (v.is_string()) return v.get<std::string>();
				return v.dump(2);
			}
		}
	}
	return "{\n}";
}

std::string pick_json_body_media(const nlohmann::json& content_obj, std::string* out_content_type) {
	if (!content_obj.is_object()) return {};
	if (content_obj.contains("application/json")) {
		*out_content_type = "application/json";
		return body_from_media_object(content_obj["application/json"]);
	}
	for (auto it = content_obj.begin(); it != content_obj.end(); ++it) {
		if (it.key().find("json") != std::string::npos) {
			*out_content_type = it.key();
			return body_from_media_object(it.value());
		}
	}
	if (!content_obj.empty()) {
		auto it = content_obj.begin();
		*out_content_type = it.key();
		return body_from_media_object(it.value());
	}
	return {};
}

std::string build_body_from_request_body(const nlohmann::json& root, const nlohmann::json& rb_in) {
	const nlohmann::json rb = resolve_ref(root, rb_in);
	if (!rb.is_object() || !rb.contains("content")) return {};
	std::string ct;
	std::string body = pick_json_body_media(rb["content"], &ct);
	if (body.empty() && rb["content"].is_object() && !rb["content"].empty()) {
		body = "{\n}";
	}
	return body;
}

}  // namespace

ImportResult import_from_json_text(std::string raw, std::optional<std::string> source_url_for_relative_servers) {
	ImportResult out;
	if (raw.empty()) {
		out.error = "Document is empty.";
		return out;
	}
	if (looks_like_yaml(raw) && raw.find("openapi:") != std::string::npos) {
		out.error = "YAML OpenAPI files are not supported yet. Convert to JSON, e.g.: yq -o=json "
		            "openapi.yaml > openapi.json";
		return out;
	}

	nlohmann::json root;
	try {
		root = nlohmann::json::parse(raw);
	} catch (const std::exception& ex) {
		out.error = std::string("Invalid JSON: ") + ex.what();
		return out;
	}

	if (!root.contains("openapi") || !root["openapi"].is_string()) {
		out.error = "Not an OpenAPI document (missing \"openapi\" version string).";
		return out;
	}
	const std::string ver = root["openapi"].get<std::string>();
	if (ver.size() < 2 || ver[0] != '3') {
		out.error = "Only OpenAPI 3.x is supported (found openapi: \"" + ver + "\").";
		return out;
	}

	std::string title = "Imported API";
	if (root.contains("info") && root["info"].is_object()) {
		title = root["info"].value("title", title);
	}
	title = trim(title);
	if (title.empty()) title = "Imported API";
	out.collection.name = title;

	if (root.contains("servers") && root["servers"].is_array() && !root["servers"].empty()) {
		const auto& s0 = root["servers"][0];
		if (s0.is_object() && s0.contains("url") && s0["url"].is_string()) {
			const std::string resolved = resolve_primary_server_url(s0["url"].get<std::string>(),
					source_url_for_relative_servers);
			if (!resolved.empty()) out.primary_server_url = resolved;
		}
	}

	if (!root.contains("paths") || !root["paths"].is_object()) {
		out.error = "OpenAPI document has no \"paths\" object.";
		return out;
	}

	static const char* kOps[] = {"get", "post", "put", "patch", "delete", "head", "options"};
	std::unordered_set<std::string> used_names;

	for (auto pit = root["paths"].begin(); pit != root["paths"].end(); ++pit) {
		std::string path_key = pit.key();
		const nlohmann::json& path_item = pit.value();
		if (!path_item.is_object()) continue;

		std::vector<nlohmann::json> path_level;
		if (path_item.contains("parameters")) {
			collect_params(root, path_item["parameters"], path_level);
		}

		for (const char* op_key : kOps) {
			if (!path_item.contains(op_key) || !path_item[op_key].is_object()) continue;
			const nlohmann::json& op = path_item[op_key];

			std::vector<nlohmann::json> params = path_level;
			if (op.contains("parameters")) {
				std::vector<nlohmann::json> op_params;
				collect_params(root, op["parameters"], op_params);
				merge_params(params, op_params);
			}

			RequestItem item;
			item.method = http_method_upper(op_key);
			item.url = path_to_url_template(path_key);

			std::string op_id = op.value("operationId", std::string());
			std::string summary = op.value("summary", std::string());
			if (!op_id.empty()) {
				item.name = op_id;
			} else if (!summary.empty()) {
				item.name = summary;
			} else {
				item.name = item.method + " " + item.url;
			}
			// Unique display names for sidebar
			std::string base_name = item.name;
			int suffix = 2;
			while (used_names.count(item.name)) {
				item.name = base_name + " (" + std::to_string(suffix++) + ")";
			}
			used_names.insert(item.name);

			std::vector<std::pair<std::string, std::string>> header_lines;
			for (const auto& p : params) {
				if (!p.is_object()) continue;
				const std::string in = p.value("in", "");
				const std::string name = p.value("name", "");
				if (name.empty()) continue;
				if (in == "query") {
					item.query_params.push_back(QueryParam{name, param_example_value(p), true});
				} else if (in == "header") {
					header_lines.emplace_back(name, param_example_value(p));
				}
			}
			for (const auto& h : header_lines) {
				item.request_headers.push_back(apikulture::HeaderRow{h.first, h.second, true});
			}

			if (op.contains("requestBody")) {
				item.body = build_body_from_request_body(root, op["requestBody"]);
			}

			out.collection.items.push_back(std::move(item));
		}
	}

	if (out.collection.items.empty()) {
		out.error = "No HTTP operations were found under \"paths\".";
		return out;
	}

	return out;
}

ImportResult import_from_file(const std::string& path_utf8) {
	const std::string raw = read_file_utf8(path_utf8);
	if (raw.empty()) {
		ImportResult out;
		out.error = "Could not read file or file is empty.";
		return out;
	}
	return import_from_json_text(std::move(raw), std::nullopt);
}

}  // namespace apikulture::openapi
