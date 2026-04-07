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

}  // namespace apikulture

#endif
