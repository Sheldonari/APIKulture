#include "response_json_highlighter.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <optional>
#include <string_view>

namespace apikulture::response_highlight {
namespace {

constexpr int kPunct = 0;
constexpr int kString = 1;
constexpr int kNumber = 2;
constexpr int kKeyword = 3;
constexpr int kKey = 4;

struct Frame {
	bool is_object;
	bool expect_key;
};

bool is_hspace(char c) {
	return c == ' ' || c == '\t';
}

bool is_digit(char c) {
	return c >= '0' && c <= '9';
}

std::optional<std::vector<Line>> tokenize_json(std::string_view s) {
	std::vector<Line> lines;
	Line current;
	std::vector<Frame> stack;

	auto flush_line = [&]() {
		lines.push_back(std::move(current));
		current = Line{};
	};

	auto emit = [&](std::string_view piece, int kind) {
		if (piece.empty()) return;
		current.push_back(Span{std::string(piece), kind});
	};

	auto emit_hspace = [&](size_t& i) {
		const size_t start = i;
		while (i < s.size() && is_hspace(static_cast<char>(s[i]))) ++i;
		if (i > start) emit(s.substr(start, i - start), kPunct);
	};

	auto skip_newlines = [&](size_t& i) {
		while (i < s.size()) {
			if (s[i] == '\r') {
				if (i + 1 < s.size() && s[i + 1] == '\n') {
					i += 2;
				} else {
					++i;
				}
				flush_line();
			} else if (s[i] == '\n') {
				++i;
				flush_line();
			} else {
				break;
			}
		}
	};

	auto read_string_emit = [&](size_t& i, bool as_key) -> bool {
		if (i >= s.size() || s[i] != '"') return false;
		const size_t start = i;
		++i;
		while (i < s.size()) {
			const char c = static_cast<char>(s[i]);
			if (c == '"') {
				++i;
				emit(s.substr(start, i - start), as_key ? kKey : kString);
				return true;
			}
			if (c == '\\') {
				++i;
				if (i >= s.size()) return false;
				const char e = static_cast<char>(s[i]);
				if (e == 'u') {
					++i;
					for (int k = 0; k < 4; ++k) {
						if (i >= s.size()) return false;
						if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
						++i;
					}
				} else {
					++i;
				}
				continue;
			}
			if (c == '\n' || c == '\r') return false;
			++i;
		}
		return false;
	};

	auto read_number = [&](size_t& i) {
		const size_t start = i;
		if (i < s.size() && s[i] == '-') ++i;
		while (i < s.size() && is_digit(static_cast<char>(s[i]))) ++i;
		if (i < s.size() && s[i] == '.') {
			++i;
			while (i < s.size() && is_digit(static_cast<char>(s[i]))) ++i;
		}
		if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
			++i;
			if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
			while (i < s.size() && is_digit(static_cast<char>(s[i]))) ++i;
		}
		emit(s.substr(start, i - start), kNumber);
	};

	auto read_keyword = [&](size_t& i, std::string_view word) -> bool {
		if (i + word.size() > s.size()) return false;
		if (s.substr(i, word.size()) != word) return false;
		const size_t after = i + word.size();
		if (after < s.size()) {
			const unsigned char uc = static_cast<unsigned char>(s[after]);
			if (std::isalnum(uc) || s[after] == '_') return false;
		}
		emit(s.substr(i, word.size()), kKeyword);
		i += word.size();
		return true;
	};

	size_t i = 0;
	while (i < s.size()) {
		emit_hspace(i);
		skip_newlines(i);
		if (i >= s.size()) break;
		emit_hspace(i);
		if (i >= s.size()) break;

		const char c = static_cast<char>(s[i]);
		if (c == '\r' || c == '\n') {
			skip_newlines(i);
			continue;
		}

		if (c == '"') {
			bool key = false;
			if (!stack.empty() && stack.back().is_object && stack.back().expect_key) {
				key = true;
				stack.back().expect_key = false;
			}
			if (!read_string_emit(i, key)) return std::nullopt;
			continue;
		}

		if (c == '{') {
			emit(s.substr(i, 1), kPunct);
			++i;
			stack.push_back(Frame{true, true});
			continue;
		}
		if (c == '}') {
			if (stack.empty() || !stack.back().is_object) return std::nullopt;
			emit(s.substr(i, 1), kPunct);
			++i;
			stack.pop_back();
			continue;
		}
		if (c == '[') {
			emit(s.substr(i, 1), kPunct);
			++i;
			stack.push_back(Frame{false, false});
			continue;
		}
		if (c == ']') {
			if (stack.empty() || stack.back().is_object) return std::nullopt;
			emit(s.substr(i, 1), kPunct);
			++i;
			stack.pop_back();
			continue;
		}
		if (c == ':') {
			emit(s.substr(i, 1), kPunct);
			++i;
			continue;
		}
		if (c == ',') {
			emit(s.substr(i, 1), kPunct);
			++i;
			if (!stack.empty() && stack.back().is_object) {
				stack.back().expect_key = true;
			}
			continue;
		}

		if (c == '-' || is_digit(c)) {
			read_number(i);
			continue;
		}
		if (c == 't') {
			if (!read_keyword(i, "true")) return std::nullopt;
			continue;
		}
		if (c == 'f') {
			if (!read_keyword(i, "false")) return std::nullopt;
			continue;
		}
		if (c == 'n') {
			if (!read_keyword(i, "null")) return std::nullopt;
			continue;
		}

		return std::nullopt;
	}

	if (!stack.empty()) return std::nullopt;

	if (!current.empty()) {
		lines.push_back(std::move(current));
	}
	return lines;
}

}  // namespace

std::optional<std::vector<Line>> highlight_json(const std::string& text) {
	if (!nlohmann::json::accept(text)) return std::nullopt;
	return tokenize_json(std::string_view(text));
}

std::vector<Line> highlight_plain_lines(const std::string& text) {
	std::vector<Line> out;
	if (text.empty()) return out;
	size_t start = 0;
	for (size_t i = 0; i < text.size(); ++i) {
		if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
			out.push_back(Line{Span{text.substr(start, i - start), kPunct}});
			++i;
			start = i + 1;
		} else if (text[i] == '\n') {
			out.push_back(Line{Span{text.substr(start, i - start), kPunct}});
			start = i + 1;
		}
	}
	out.push_back(Line{Span{text.substr(start), kPunct}});
	return out;
}

}  // namespace apikulture::response_highlight
