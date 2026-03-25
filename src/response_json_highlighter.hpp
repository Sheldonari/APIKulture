#ifndef APIKULTURE_RESPONSE_JSON_HIGHLIGHTER_H
#define APIKULTURE_RESPONSE_JSON_HIGHLIGHTER_H

#include <optional>
#include <string>
#include <vector>

namespace apikulture::response_highlight {

/// 0 = punctuation / whitespace, 1 = string value, 2 = number, 3 = keyword, 4 = object key
struct Span {
	std::string text;
	int kind{0};
};

using Line = std::vector<Span>;

/// Tokenize pretty-printed JSON for syntax highlighting. Returns nullopt if the text is not valid JSON.
std::optional<std::vector<Line>> highlight_json(const std::string& text);

/// One span per line, kind 0 (plain).
std::vector<Line> highlight_plain_lines(const std::string& text);

}  // namespace apikulture::response_highlight

#endif
