#include "query_string.hpp"
#include "environment.hpp"

#include <cctype>
#include <sstream>

namespace apikulture {

// Used by url_encode_query_component (must not live only inside an anonymous namespace below).
static const char k_hex_encode[] = "0123456789ABCDEF";

namespace {

void trim_ascii(std::string& s) {
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(0, 1);
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
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

/// Decode `application/x-www-form-urlencoded` component (percent + plus); same rules as url_import.cpp.
void url_decode_query_component(std::string_view in, std::string& out) {
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
}

}  // namespace

std::string url_encode_query_component(std::string_view s) {
	std::string out;
	out.reserve(s.size() + s.size() / 4);
	for (unsigned char c : s) {
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'
				|| c == '_' || c == '.' || c == '~') {
			out += static_cast<char>(c);
		} else if (c == ' ') {
			out += '%';
			out += '2';
			out += '0';
		} else {
			out += '%';
			out += k_hex_encode[c >> 4];
			out += k_hex_encode[c & 0xF];
		}
	}
	return out;
}

void normalize_http_url_delimiters_inplace(std::string& s) {
	static const char fw_q[] = "\xEF\xBC\x9F";    // U+FF1F FULLWIDTH QUESTION MARK
	static const char fw_amp[] = "\xEF\xBC\x86";  // U+FF06 FULLWIDTH AMPERSAND
	const size_t fw_q_len = sizeof(fw_q) - 1;
	const size_t fw_amp_len = sizeof(fw_amp) - 1;
	for (;;) {
		const size_t p = s.find(fw_q);
		if (p == std::string::npos) break;
		s.replace(p, fw_q_len, "?");
	}
	for (;;) {
		const size_t p = s.find(fw_amp);
		if (p == std::string::npos) break;
		s.replace(p, fw_amp_len, "&");
	}
}

std::string append_query_params_to_url(const std::string& url,
		const std::vector<QueryParam>& params,
		const std::map<std::string, std::string>& vars) {
	std::ostringstream new_qs_stream;
	bool first_pair = true;
	for (const auto& p : params) {
		if (!p.enabled) continue;
		std::string k = substitute_variables(p.key, vars);
		std::string v = substitute_variables(p.value, vars);
		trim_ascii(k);
		trim_ascii(v);
		if (k.empty()) continue;
		if (!first_pair) new_qs_stream << '&';
		first_pair = false;
		new_qs_stream << url_encode_query_component(k) << '=' << url_encode_query_component(v);
	}
	const std::string new_qs = new_qs_stream.str();
	if (new_qs.empty()) return url;

	const size_t hash_pos = url.find('#');
	const std::string before_frag = url.substr(0, hash_pos);
	const std::string frag = hash_pos == std::string::npos ? std::string() : url.substr(hash_pos);

	const size_t qpos = before_frag.find('?');
	const std::string path_part = qpos == std::string::npos ? before_frag : before_frag.substr(0, qpos);
	const std::string exist_qs = qpos == std::string::npos ? std::string() : before_frag.substr(qpos + 1);

	std::string merged_qs;
	if (exist_qs.empty()) merged_qs = new_qs;
	else if (new_qs.empty()) merged_qs = exist_qs;
	else merged_qs = exist_qs + '&' + new_qs;

	std::string mid = path_part;
	if (!merged_qs.empty()) mid += '?' + merged_qs;
	return mid + frag;
}

std::string strip_url_query_preserving_fragment(std::string url) {
	const size_t hash_pos = url.find('#');
	const std::string before_frag = hash_pos == std::string::npos ? url : url.substr(0, hash_pos);
	const std::string frag = hash_pos == std::string::npos ? std::string() : url.substr(hash_pos);
	const size_t qpos = before_frag.find('?');
	if (qpos == std::string::npos) return url;
	return before_frag.substr(0, qpos) + frag;
}

bool has_query_params_to_append(const std::vector<QueryParam>& params,
		const std::map<std::string, std::string>& vars) {
	for (const auto& p : params) {
		if (!p.enabled) continue;
		std::string k = substitute_variables(p.key, vars);
		trim_ascii(k);
		if (!k.empty()) return true;
	}
	return false;
}

void populate_params_from_url(std::string_view url,
		std::vector<QueryParam>& params,
		const std::map<std::string, std::string>& /*vars*/) {
	params.clear();

	const size_t qpos = url.find('?');
	const size_t hash_pos = url.find('#');

	if (qpos == std::string::npos) {
		return;
	}

	const size_t end_pos = (hash_pos == std::string::npos) ? url.size() : hash_pos;
	const std::string query_string(url.substr(qpos + 1, end_pos - (qpos + 1)));

	if (query_string.empty()) {
		return;
	}

	std::stringstream ss(query_string);
	std::string item;
	while (std::getline(ss, item, '&')) {
		const size_t eq_pos = item.find('=');
		std::string key_encoded;
		std::string value_encoded;

		if (eq_pos == std::string::npos) {
			key_encoded = item;
			value_encoded = "";
		} else {
			key_encoded = item.substr(0, eq_pos);
			value_encoded = item.substr(eq_pos + 1);
		}

		std::string key;
		std::string value;
		url_decode_query_component(key_encoded, key);
		url_decode_query_component(value_encoded, value);

		QueryParam param;
		param.key = std::move(key);
		param.value = std::move(value);
		param.enabled = true;
		params.push_back(std::move(param));
	}
}

}  // namespace apikulture
