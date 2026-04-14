#ifndef APIKULTURE_QUERY_STRING_H
#define APIKULTURE_QUERY_STRING_H

#include <map>
#include <string>
#include <vector>

#include "collections_io.hpp"

namespace apikulture {

/// RFC 3986-style percent-encoding for query keys and values (UTF-8 bytes).
std::string url_encode_query_component(std::string_view s);

/// Replace fullwidth `?` / `&` (U+FF1F / U+FF06) with ASCII so query strings parse reliably after paste.
void normalize_http_url_delimiters_inplace(std::string& s);

/// Append `params` as a query string to `url`. Substitutes `{{var}}` in each key/value using \a vars.
/// Merges with any existing `?…` on the URL (before `#fragment`). Skips params with an empty key after trim.
std::string append_query_params_to_url(const std::string& url,
		const std::vector<QueryParam>& params,
		const std::map<std::string, std::string>& vars);

/// Removes `?query` from \a url while keeping `#fragment`. No-op if there is no `?` before `#`.
std::string strip_url_query_preserving_fragment(std::string url);

/// True if at least one enabled param has a non-empty key after variable substitution and trim.
bool has_query_params_to_append(const std::vector<QueryParam>& params,
		const std::map<std::string, std::string>& vars);

/**
 * @brief Parses query parameters from a full URL string and populates the provided vector.
 * @param url The URL string to parse (e.g., "https://example.com/api?key1=val1&key2=val2").
 * @param params A reference to the vector to be populated with extracted QueryParam objects.
 * @param vars The environment variables (used to populate vars if needed, although usually not necessary for direct URL parsing).
 */
void populate_params_from_url(std::string_view url,
		std::vector<QueryParam>& params,
		const std::map<std::string, std::string>& vars = {});

}  // namespace apikulture

#endif
