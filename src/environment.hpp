#ifndef APIKULTURE_ENVIRONMENT_H
#define APIKULTURE_ENVIRONMENT_H

#include <map>
#include <string>
#include <vector>

namespace apikulture {

struct Environment {
	std::string name{"Default"};
	/// Optional origin for this environment; relative request paths are joined here (after `{{var}}` substitution).
	std::string base_url;
	std::vector<std::pair<std::string, std::string>> variables;
};

/// Parse "KEY=value" lines (first '=' separates; '#' starts a comment line).
std::vector<std::pair<std::string, std::string>> parse_environment_lines(const std::string& text);

/// Serialize variables to KEY=value lines (sorted by key for stable files).
std::string format_environment_lines(const std::vector<std::pair<std::string, std::string>>& variables);

/// Single pass `{{NAME}}` replacement; unknown names left unchanged. Values are not re-substituted.
std::string substitute_variables(const std::string& text,
		const std::map<std::string, std::string>& vars);

/// Merge env.variables with optional local overrides for this environment name (local wins on key clash).
std::map<std::string, std::string> effective_variable_map(const Environment& env,
		const std::map<std::string, std::map<std::string, std::string>>& local_overrides_by_env_name);

/// Shared `base_url` field, optionally overridden by `local_overrides[env.name]["base_url"]`.
std::string effective_base_url_field(const Environment& env,
		const std::map<std::string, std::map<std::string, std::string>>& local_overrides_by_env_name);

/// True if `s` begins with `http://` or `https://` (after leading ASCII whitespace).
bool is_absolute_http_url(std::string_view s);

/// Join environment base to a path that is not an absolute URL. Handles `?query` and `#fragment` on the path.
std::string resolve_url_with_base(std::string_view base_substituted, const std::string& path_after_substitution);

}  // namespace apikulture

#endif
