#include "environment.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string_view>

namespace apikulture {

namespace {

void trim_inplace(std::string& s) {
	auto not_space = [](unsigned char c) { return !std::isspace(c); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
	s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

}  // namespace

std::vector<std::pair<std::string, std::string>> parse_environment_lines(const std::string& text) {
	std::vector<std::pair<std::string, std::string>> out;
	std::istringstream stream(text);
	std::string line;
	while (std::getline(stream, line)) {
		trim_inplace(line);
		if (line.empty() || line[0] == '#') continue;
		const size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string key = line.substr(0, eq);
		std::string value = line.substr(eq + 1);
		trim_inplace(key);
		trim_inplace(value);
		if (!key.empty()) out.emplace_back(std::move(key), std::move(value));
	}
	return out;
}

std::string format_environment_lines(const std::vector<std::pair<std::string, std::string>>& variables) {
	std::vector<std::pair<std::string, std::string>> sorted = variables;
	std::sort(sorted.begin(), sorted.end(),
			[](const auto& a, const auto& b) { return a.first < b.first; });
	std::ostringstream o;
	for (size_t i = 0; i < sorted.size(); ++i) {
		if (i) o << '\n';
		o << sorted[i].first << '=' << sorted[i].second;
	}
	return o.str();
}

std::string substitute_variables(const std::string& text,
		const std::map<std::string, std::string>& vars) {
	std::string result;
	result.reserve(text.size() + 64);
	size_t i = 0;
	while (i < text.size()) {
		const size_t open = text.find("{{", i);
		if (open == std::string::npos) {
			result.append(text, i, text.size() - i);
			break;
		}
		result.append(text, i, open - i);
		const size_t close = text.find("}}", open + 2);
		if (close == std::string::npos) {
			result.append(text, open, text.size() - open);
			break;
		}
		std::string key = text.substr(open + 2, close - (open + 2));
		trim_inplace(key);
		auto it = vars.find(key);
		if (it != vars.end()) {
			result += it->second;
		} else {
			result.append(text, open, close + 2 - open);
		}
		i = close + 2;
	}
	return result;
}

std::map<std::string, std::string> effective_variable_map(const Environment& env,
		const std::map<std::string, std::map<std::string, std::string>>& local_overrides_by_env_name) {
	std::map<std::string, std::string> out;
	for (const auto& kv : env.variables) {
		out[kv.first] = kv.second;
	}
	auto oit = local_overrides_by_env_name.find(env.name);
	if (oit != local_overrides_by_env_name.end()) {
		for (const auto& kv : oit->second) {
			out[kv.first] = kv.second;
		}
	}
	return out;
}

std::string effective_base_url_field(const Environment& env,
		const std::map<std::string, std::map<std::string, std::string>>& local_overrides_by_env_name) {
	std::string b = env.base_url;
	auto oit = local_overrides_by_env_name.find(env.name);
	if (oit != local_overrides_by_env_name.end()) {
		auto jt = oit->second.find("base_url");
		if (jt != oit->second.end()) b = jt->second;
	}
	return b;
}

namespace {

void trim_ascii_inplace(std::string& s) {
	auto not_space = [](unsigned char c) { return !std::isspace(c); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
	s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

std::string_view trim_view(std::string_view s) {
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
	return s;
}

}  // namespace

bool is_absolute_http_url(std::string_view s) {
	s = trim_view(s);
	if (s.size() >= 7) {
		static const char http_pref[] = "http://";
		bool ok = true;
		for (size_t i = 0; i < 7; ++i) {
			if (std::tolower(static_cast<unsigned char>(s[i])) != http_pref[i]) {
				ok = false;
				break;
			}
		}
		if (ok) return true;
	}
	if (s.size() >= 8) {
		static const char https_pref[] = "https://";
		bool ok = true;
		for (size_t i = 0; i < 8; ++i) {
			if (std::tolower(static_cast<unsigned char>(s[i])) != https_pref[i]) {
				ok = false;
				break;
			}
		}
		if (ok) return true;
	}
	return false;
}

std::string resolve_url_with_base(std::string_view base_substituted, const std::string& path_after_substitution) {
	std::string path = path_after_substitution;
	trim_ascii_inplace(path);
	if (path.empty()) {
		std::string b(base_substituted);
		trim_ascii_inplace(b);
		return b;
	}
	if (is_absolute_http_url(path)) return path;

	std::string base(base_substituted);
	trim_ascii_inplace(base);

	if (!path.empty() && path[0] == '?') {
		if (base.empty()) return path;
		return base + path;
	}
	if (!path.empty() && path[0] == '#') {
		if (base.empty()) return path;
		return base + path;
	}

	while (!base.empty() && base.back() == '/') base.pop_back();
	while (!path.empty() && path[0] == '/') path.erase(0, 1);

	if (base.empty()) return path;
	if (path.empty()) return base;
	return base + "/" + path;
}

}  // namespace apikulture
