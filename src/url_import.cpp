#include "url_import.hpp"
#include "environment.hpp"
#include "query_string.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace apikulture::url_import {

namespace {

void trim_inplace(std::string& s) {
	auto not_space = [](unsigned char c) { return !std::isspace(c); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
	s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

bool hex_digit(char c, unsigned& out) {
	if (c >= '0' && c <= '9') {
		out = static_cast<unsigned>(c - '0');
		return true;
	}
	if (c >= 'a' && c <= 'f') {
		out = 10u + static_cast<unsigned>(c - 'a');
		return true;
	}
	if (c >= 'A' && c <= 'F') {
		out = 10u + static_cast<unsigned>(c - 'A');
		return true;
	}
	return false;
}

/// Decode `application/x-www-form-urlencoded` component (percent + plus).
bool url_decode_query_component(std::string_view in, std::string& out) {
	out.clear();
	out.reserve(in.size());
	for (size_t i = 0; i < in.size(); ++i) {
		const char c = in[i];
		if (c == '+') {
			out += ' ';
			continue;
		}
		if (c == '%' && i + 2 < in.size()) {
			unsigned hi = 0, lo = 0;
			if (hex_digit(in[i + 1], hi) && hex_digit(in[i + 2], lo)) {
				out += static_cast<char>((hi << 4) | lo);
				i += 2;
				continue;
			}
		}
		out += c;
	}
	return true;
}

void parse_query_string(std::string_view qs, std::vector<QueryParam>& params) {
	params.clear();
	if (qs.empty()) return;
	std::string chunk;
	while (!qs.empty()) {
		size_t amp = qs.find('&');
		std::string_view part = qs.substr(0, amp);
		if (amp == std::string_view::npos) {
			qs = {};
		} else {
			qs.remove_prefix(amp + 1);
		}
		chunk.assign(part.begin(), part.end());
		trim_inplace(chunk);
		if (chunk.empty()) continue;
		std::string key_raw;
		std::string val_raw;
		const size_t eq = chunk.find('=');
		if (eq == std::string::npos) {
			key_raw = std::move(chunk);
		} else {
			key_raw = chunk.substr(0, eq);
			val_raw = chunk.substr(eq + 1);
		}
		std::string key_dec;
		std::string val_dec;
		url_decode_query_component(key_raw, key_dec);
		url_decode_query_component(val_raw, val_dec);
		QueryParam p;
		p.key = std::move(key_dec);
		p.value = std::move(val_dec);
		p.enabled = true;
		params.push_back(std::move(p));
	}
}

void strip_trailing_slashes_inplace(std::string& s) {
	while (s.size() > 1 && s.back() == '/') s.pop_back();
}

/// If the query was not parsed earlier but remains on the path (e.g. odd delimiters), split into \a params_out.
void extract_inline_query_from_url_inplace(std::string& url_inout, std::vector<QueryParam>& params_out) {
	apikulture::normalize_http_url_delimiters_inplace(url_inout);
	const size_t hash = url_inout.find('#');
	const std::string before_frag = hash == std::string::npos ? url_inout : url_inout.substr(0, hash);
	const std::string frag = hash == std::string::npos ? std::string() : url_inout.substr(hash);
	const size_t qpos = before_frag.find('?');
	if (qpos == std::string::npos) return;
	const std::string qs = before_frag.substr(qpos + 1);
	url_inout = before_frag.substr(0, qpos) + frag;
	std::vector<QueryParam> parsed;
	parse_query_string(qs, parsed);
	if (!parsed.empty()) params_out = std::move(parsed);
}

/// True if \a full begins with \a prefix (ASCII case-insensitive) and either ends there or continues with `/`.
bool has_prefix_path_ci(std::string_view full, std::string_view prefix) {
	if (full.size() < prefix.size()) return false;
	for (size_t i = 0; i < prefix.size(); ++i) {
		if (std::tolower(static_cast<unsigned char>(full[i]))
				!= std::tolower(static_cast<unsigned char>(prefix[i]))) {
			return false;
		}
	}
	return full.size() == prefix.size() || full[prefix.size()] == '/';
}

}  // namespace

bool parse_pasted_url_into_request(std::string raw,
		std::string_view base_substituted,
		std::string& url_out,
		std::vector<QueryParam>& params_out) {
	url_out.clear();
	params_out.clear();

	trim_inplace(raw);
	if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF && static_cast<unsigned char>(raw[1]) == 0xBB
			&& static_cast<unsigned char>(raw[2]) == 0xBF) {
		raw.erase(0, 3);
	}
	if (raw.size() >= 2 && raw.front() == '<' && raw.back() == '>') {
		raw = raw.substr(1, raw.size() - 2);
		trim_inplace(raw);
	}
	if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
		raw = raw.substr(1, raw.size() - 2);
		trim_inplace(raw);
	}
	if (raw.empty()) return false;

	apikulture::normalize_http_url_delimiters_inplace(raw);

	std::string fragment;
	const size_t hash = raw.find('#');
	if (hash != std::string::npos) {
		fragment = raw.substr(hash);
		raw.resize(hash);
		trim_inplace(raw);
	}

	std::string query_part;
	const size_t qmark = raw.find('?');
	if (qmark != std::string::npos) {
		query_part = raw.substr(qmark + 1);
		raw.resize(qmark);
		trim_inplace(raw);
	}
	parse_query_string(query_part, params_out);

	std::string path_core = raw;
	trim_inplace(path_core);

	if (path_core.empty() && params_out.empty() && fragment.empty()) return false;

	if (apikulture::is_absolute_http_url(path_core)) {
		std::string base(base_substituted);
		trim_inplace(base);
		strip_trailing_slashes_inplace(base);

		if (!base.empty() && has_prefix_path_ci(path_core, base)) {
			std::string rel = path_core.substr(base.size());
			trim_inplace(rel);
			while (!rel.empty() && rel.front() == '/') rel.erase(0, 1);
			url_out = rel + fragment;
		} else {
			url_out = path_core + fragment;
		}
	} else {
		url_out = path_core + fragment;
	}

	if (params_out.empty()) {
		extract_inline_query_from_url_inplace(url_out, params_out);
	}

	if (url_out.empty() && !params_out.empty()) return true;
	if (!url_out.empty()) return true;
	return false;
}

}  // namespace apikulture::url_import
