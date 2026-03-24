#include "window_state.hpp"

#include "collections_io.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace apikulture::window_state {

namespace {

static fs::path window_state_file_path() {
	fs::path p(collections_io::default_config_path());
	p.replace_filename("window-state.json");
	return p;
}

static nlohmann::json read_json_or_default() {
	const fs::path path = window_state_file_path();
	std::ifstream in(path);
	if (!in) {
		return nlohmann::json::object({{"version", 1},
		                               {"maximized", false},
		                               {"response_body_font_family", "monospace"},
		                               {"response_body_font_size_px", 12.0},
		                               {"collection_panel_width_px", 240.0},
		                               {"request_panel_width_px", 420.0},
		                               {"sidebar_collections_height_px", 220.0},
		                               {"request_query_panel_height_px", 200.0},
		                               {"response_headers_panel_height_px", 120.0}});
	}
	try {
		nlohmann::json j;
		in >> j;
		return j;
	} catch (...) {
		return nlohmann::json::object({{"version", 1},
		                               {"maximized", false},
		                               {"response_body_font_family", "monospace"},
		                               {"response_body_font_size_px", 12.0},
		                               {"collection_panel_width_px", 240.0},
		                               {"request_panel_width_px", 420.0},
		                               {"sidebar_collections_height_px", 220.0},
		                               {"request_query_panel_height_px", 200.0},
		                               {"response_headers_panel_height_px", 120.0}});
	}
}

static void write_json(const nlohmann::json& j) {
	const fs::path path = window_state_file_path();
	fs::create_directories(path.parent_path());
	std::ofstream out(path, std::ios::trunc);
	if (!out) return;
	out << j.dump(2);
}

}  // namespace

std::string normalize_response_font(std::string_view name) {
	if (name.empty()) return "monospace";
	for (const char* choice : k_response_font_choices) {
		if (name == choice) return std::string(name);
	}
	return "monospace";
}

int response_font_choice_index(std::string_view name) {
	const std::string n = normalize_response_font(name);
	for (size_t i = 0; i < k_response_font_choices.size(); ++i) {
		if (n == k_response_font_choices[i]) return static_cast<int>(i);
	}
	return 0;
}

namespace {

float clamp_response_font_size_px(float v) {
	if (v < 8.f) return 8.f;
	if (v > 24.f) return 24.f;
	return v;
}

float clamp_collection_panel_width_px(float v) {
	return std::clamp(v, 180.f, 560.f);
}

float clamp_request_panel_width_px(float v) {
	return std::clamp(v, 200.f, 10000.f);
}

float clamp_sidebar_collections_height_px(float v) {
	return std::clamp(v, 120.f, 10000.f);
}

float clamp_request_query_panel_height_px(float v) {
	return std::clamp(v, 80.f, 10000.f);
}

float clamp_response_headers_panel_height_px(float v) {
	return std::clamp(v, 72.f, 10000.f);
}

}  // namespace

PersistedWindowState load_window_session() {
	nlohmann::json j = read_json_or_default();
	PersistedWindowState s;
	s.maximized = j.value("maximized", false);
	s.response_body_font_family = normalize_response_font(
			j.value("response_body_font_family", std::string("monospace")));
	s.response_body_font_size_px = clamp_response_font_size_px(
			j.value("response_body_font_size_px", 12.0));
	s.collection_panel_width_px = clamp_collection_panel_width_px(
			j.value("collection_panel_width_px", 240.0));
	s.request_panel_width_px = clamp_request_panel_width_px(
			j.value("request_panel_width_px", 420.0));
	s.sidebar_collections_height_px = clamp_sidebar_collections_height_px(
			j.value("sidebar_collections_height_px", 220.0));
	s.request_query_panel_height_px = clamp_request_query_panel_height_px(
			j.value("request_query_panel_height_px", 200.0));
	s.response_headers_panel_height_px = clamp_response_headers_panel_height_px(
			j.value("response_headers_panel_height_px", 120.0));
	return s;
}

void save_window_session(const PersistedWindowState& state) {
	nlohmann::json j = read_json_or_default();
	j["version"] = 1;
	j["maximized"] = state.maximized;
	j["response_body_font_family"] = std::string(normalize_response_font(state.response_body_font_family));
	j["response_body_font_size_px"] = clamp_response_font_size_px(state.response_body_font_size_px);
	j["collection_panel_width_px"] = clamp_collection_panel_width_px(state.collection_panel_width_px);
	j["request_panel_width_px"] = clamp_request_panel_width_px(state.request_panel_width_px);
	j["sidebar_collections_height_px"] = clamp_sidebar_collections_height_px(
			state.sidebar_collections_height_px);
	j["request_query_panel_height_px"] = clamp_request_query_panel_height_px(
			state.request_query_panel_height_px);
	j["response_headers_panel_height_px"] = clamp_response_headers_panel_height_px(
			state.response_headers_panel_height_px);
	write_json(j);
}

void persist_response_body_font_family(std::string_view family) {
	nlohmann::json j = read_json_or_default();
	j["version"] = 1;
	j["response_body_font_family"] = std::string(normalize_response_font(family));
	write_json(j);
}

void persist_response_body_font_size(float size_px) {
	nlohmann::json j = read_json_or_default();
	j["version"] = 1;
	j["response_body_font_size_px"] = clamp_response_font_size_px(size_px);
	write_json(j);
}

}  // namespace apikulture::window_state
