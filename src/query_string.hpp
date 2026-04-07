#ifndef APIKULTURE_QUERY_STRING_H
#define APIKULTURE_QUERY_STRING_H

#include <map>
#include <string>
#include <vector>

#include "collections_io.hpp"

namespace apikulture {

/// RFC 3986-style percent-encoding for query keys and values (UTF-8 bytes).
std::string url_encode_query_component(std::string_view s);

/// Append `params` as a query string to `url`. Substitutes `{{var}}` in each key/value using \a vars.
/// Merges with any existing `?…` on the URL (before `#fragment`). Skips params with an empty key after trim.
std::string append_query_params_to_url(const std::string& url,
		const std::vector<QueryParam>& params,
		const std::map<std::string, std::string>& vars);

}  // namespace apikulture

#endif
