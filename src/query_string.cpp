#include "query_string.hpp"
#include "environment.hpp"

#include <cctype>
#include <sstream>

namespace apikulture {

namespace {

void trim_ascii(std::string& s) {
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(0, 1);
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
}

static const char kHex[] = "0123456789ABCDEF";

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
			out += kHex[c >> 4];
			out += kHex[c & 0xF];
		}
	}
	return out;
}

std::string append_query_params_to_url(const std::string& url,
		const std::vector<std::pair<std::string, std::string>>& params,
		const std::map<std::string, std::string>& vars) {
	std::ostringstream new_qs_stream;
	bool first_pair = true;
	for (const auto& p : params) {
		std::string k = substitute_variables(p.first, vars);
		std::string v = substitute_variables(p.second, vars);
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

}  // namespace apikulture
