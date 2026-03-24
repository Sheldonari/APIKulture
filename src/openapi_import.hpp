#ifndef APIKULTURE_OPENAPI_IMPORT_H
#define APIKULTURE_OPENAPI_IMPORT_H

#include "collections_io.hpp"

#include <optional>
#include <string>

namespace apikulture::openapi {

struct ImportResult {
	/// Empty on success.
	std::string error;
	apikulture::Collection collection;
	/// First `servers[].url` (trimmed); caller may apply to environment base URL.
	std::optional<std::string> primary_server_url;
};

/// Parse OpenAPI **3.x** document (JSON). YAML payloads are rejected with a hint to convert.
/// If \a source_url_for_relative_servers is set (e.g. fetch URL), relative `servers[0].url` like `/api/v3` is resolved against that origin.
ImportResult import_from_json_text(std::string raw_utf8,
		std::optional<std::string> source_url_for_relative_servers = std::nullopt);

/// Read a UTF-8 file and parse as OpenAPI 3.x JSON.
ImportResult import_from_file(const std::string& path_utf8);

}  // namespace apikulture::openapi

#endif
