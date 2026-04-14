#pragma once

#include <array>
#include <string>
#include <string_view>

namespace apikulture::window_state {

/// Response panel font choices (display name = CSS `font-family` value). Order is UI list order.
inline constexpr std::array<const char*, 8> k_response_font_choices = {
		"monospace",
		"Courier New",
		"Consolas",
		"JetBrains Mono",
		"Fira Code",
		"Source Code Pro",
		"Noto Sans Mono",
		"Lucida Console",
};

struct PersistedWindowState {
	bool maximized = false;
	/// Font family for the response column (body, headers, JSONPath).
	std::string response_body_font_family = "monospace";
	/// Font size in logical pixels for the response column (body, headers, JSONPath).
	float response_body_font_size_px = 12.f;
	/// Main splitter: collections sidebar width (matches `VerticalSplitter` min/max in UI).
	float collection_panel_width_px = 240.f;
	/// Request column width before the response splitter.
	float request_panel_width_px = 420.f;
	/// Horizontal splitter between collections list and requests list.
	float sidebar_collections_height_px = 220.f;
	/// Horizontal splitter between requests block (list + New/Dup/Del) and Save / import / environment.
	float sidebar_requests_section_height_px = 260.f;
	/// Horizontal splitter between query-parameters block and headers/body in the request column.
	float request_query_panel_height_px = 200.f;
	/// Horizontal splitter between request headers block and body in the request column.
	float request_headers_panel_height_px = 200.f;
	/// Horizontal splitter between response headers block and JSONPath/body.
	float response_headers_panel_height_px = 120.f;
};

/// Load `window-state.json` or defaults when missing / invalid.
PersistedWindowState load_window_session();

/// Write full session (used on exit).
void save_window_session(const PersistedWindowState& state);

/// Update only the response font in the JSON file; leaves `maximized` as stored on disk.
void persist_response_body_font_family(std::string_view family);

/// Update only the response font size; leaves other keys as stored on disk.
void persist_response_body_font_size(float size_px);

/// If \a name is not a known UI choice, returns `"monospace"`.
std::string normalize_response_font(std::string_view name);

/// Index into `k_response_font_choices` after normalization; 0 if unknown.
int response_font_choice_index(std::string_view stored_or_ui_name);

}  // namespace apikulture::window_state
