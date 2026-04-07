#ifndef APIKULTURE_URL_IMPORT_H
#define APIKULTURE_URL_IMPORT_H

#include "collections_io.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace apikulture::url_import {

/// Trim, strip BOM/quotes/brackets, split `?query` and `#fragment` off the path.
/// Fills \a params from the query string (URL-decoded keys/values, `+` as space).
/// \a base_substituted is the resolved environment base URL (may be empty): when the pasted
/// absolute URL shares that prefix, \a url_out is the relative remainder so it works with Base URL.
/// Returns false if there is no usable path and no query parameters.
bool parse_pasted_url_into_request(std::string raw,
		std::string_view base_substituted,
		std::string& url_out,
		std::vector<QueryParam>& params_out);

}  // namespace apikulture::url_import

#endif
