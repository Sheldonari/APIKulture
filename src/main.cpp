#include <slint.h>
#include "MainWindow.h"
#include "app_state.h"
#include "platform_color_scheme.h"
#include "theme_watcher.h"
#include "window_state.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <slint_models.h>

namespace {

const char* color_scheme_argv_override(int argc, char** argv)
{
	for (int i = 1; i < argc; ++i) {
		const char* a = argv[i];
		if (std::strncmp(a, "--color-scheme=", 15) == 0)
			return a + 15;
		if (std::strcmp(a, "--color-scheme") == 0 && i + 1 < argc)
			return argv[i + 1];
	}
	return nullptr;
}

const char* import_openapi_argv_path(int argc, char** argv)
{
	for (int i = 1; i < argc; ++i) {
		const char* a = argv[i];
		if (std::strncmp(a, "--import-openapi=", 19) == 0)
			return a + 19;
	}
	return nullptr;
}

const char* import_openapi_argv_url(int argc, char** argv)
{
	for (int i = 1; i < argc; ++i) {
		const char* a = argv[i];
		if (std::strncmp(a, "--import-openapi-url=", 23) == 0)
			return a + 23;
	}
	return nullptr;
}

// For material/fluent styles: Light = light UI, Dark = dark UI (matches Slint MaterialPalette).
// Returns nullopt = do not call set_window_color_scheme (unknown → follow SlintInternal / OS).
std::optional<slint::cbindgen_private::ColorScheme> resolve_startup_color_scheme(const char* argv_override)
{
	const char* raw = argv_override;
	if (!raw || !*raw)
		raw = std::getenv("APIKULTURE_COLOR_SCHEME");
	if (raw && *raw) {
		if (std::strcmp(raw, "light") == 0)
			return slint::cbindgen_private::ColorScheme::Light;
		if (std::strcmp(raw, "dark") == 0)
			return slint::cbindgen_private::ColorScheme::Dark;
		if (std::strcmp(raw, "system") == 0 || std::strcmp(raw, "auto") == 0)
			; // fall through to platform detection below
		else
			return std::nullopt;
	}
	return platform_detect_desktop_color_scheme();
}

bool follows_system_theme(int argc, char** argv)
{
	const char* a = color_scheme_argv_override(argc, argv);
	const char* raw = a ? a : std::getenv("APIKULTURE_COLOR_SCHEME");
	if (!raw || !*raw)
		return true;
	if (std::strcmp(raw, "light") == 0 || std::strcmp(raw, "dark") == 0)
		return false;
	return true;
}

static std::string shared_string_to_std(const slint::SharedString& ss)
{
	try {
		std::string_view v = static_cast<std::string_view>(ss);
		if (v.data() == nullptr || v.empty()) return std::string();
		return std::string(v.data(), v.size());
	} catch (...) {
		return std::string();
	}
}

} // namespace

int main(int argc, char** argv) {
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
			std::fprintf(stderr,
					"Usage: apikulture [options]\n"
					"  --color-scheme=light|dark|system|auto\n"
					"  --import-openapi=PATH   Import OpenAPI 3.x JSON file after startup\n"
					"  --import-openapi-url=URL   Fetch OpenAPI 3.x JSON over HTTP(S) after startup\n");
			return 0;
		}
	}

	auto ui = MainWindow::create();
	AppState state(ui);

	// Initialize string properties so they are never in an undefined (null) state when read
	{
		auto& g = ui->global<AppLogic>();
		std::optional<slint::cbindgen_private::ColorScheme> initial_scheme;
		if (auto scheme = resolve_startup_color_scheme(color_scheme_argv_override(argc, argv))) {
			initial_scheme = *scheme;
			ui->set_window_color_scheme(*scheme);
		}
		start_theme_watcher(ui, follows_system_theme(argc, argv), initial_scheme);
		g.set_import_status(slint::SharedString(""));
		g.set_openapi_import_url(slint::SharedString(""));
		g.set_method(slint::SharedString("GET"));
		g.set_url(slint::SharedString(""));
		g.set_resolved_request_url(slint::SharedString(""));
		g.set_query_params_text(slint::SharedString(""));
		g.set_query_params_text_mode(false);
		g.set_request_body(slint::SharedString(""));
		g.set_response_status(slint::SharedString(""));
		g.set_response_headers(slint::SharedString(""));
		g.set_response_jsonpath(slint::SharedString(""));
		g.set_response_body(slint::SharedString(""));
		g.set_response_body_lines(std::make_shared<slint::VectorModel<ResponseLine>>());
		g.set_active_environment_name(slint::SharedString("Default"));
		g.set_environment_name_edit(slint::SharedString(""));
		g.set_environment_base_url(slint::SharedString(""));
		g.set_environment_variables(slint::SharedString(""));
		{
			auto font_model = std::make_shared<slint::VectorModel<slint::SharedString>>();
			for (const char* f : apikulture::window_state::k_response_font_choices) {
				font_model->push_back(slint::SharedString(f));
			}
			g.set_response_font_names(font_model);
		}
		const auto ws = apikulture::window_state::load_window_session();
		g.set_response_font_index(apikulture::window_state::response_font_choice_index(
				ws.response_body_font_family));
		g.set_response_body_font_family(
				slint::SharedString(ws.response_body_font_family));
		g.set_response_body_font_size(ws.response_body_font_size_px);
		ui->set_collection_panel_width(ws.collection_panel_width_px);
		ui->set_request_panel_width(ws.request_panel_width_px);
		ui->set_sidebar_collections_height(ws.sidebar_collections_height_px);
		ui->set_sidebar_requests_section_height(ws.sidebar_requests_section_height_px);
		ui->set_request_query_panel_height(ws.request_query_panel_height_px);
		ui->set_request_headers_panel_height(ws.request_headers_panel_height_px);
		ui->set_response_headers_panel_height(ws.response_headers_panel_height_px);
		if (ws.maximized) {
			ui->window().set_maximized(true);
		}
	}

	state.init_collections_ui();

	if (const char* oa = import_openapi_argv_path(argc, argv)) {
		if (*oa)
			state.import_openapi_from_path(std::string(oa));
	}
	if (const char* ou = import_openapi_argv_url(argc, argv)) {
		if (*ou)
			state.import_openapi_from_url_string(std::string(ou));
	}

	auto& logic = ui->global<AppLogic>();
	logic.on_send_request([&state]() { state.send_request(); });
	logic.on_cancel_request([&state]() { state.cancel_request(); });
	logic.on_request_url_accepted([&state]() { state.request_url_accepted(); });
	logic.on_request_url_edited([&state]() { state.request_url_edited(); });
	logic.on_copy_request_url([&state]() { state.copy_request_url(); });
	logic.on_copy_response_body([&state]() { state.copy_response_body(); });
	logic.on_select_collection([&state](int i) { state.select_collection(i); });
	logic.on_select_request([&state](int i) { state.select_request(i); });
	logic.on_new_collection([&state]() { state.new_collection(); });
	logic.on_delete_collection([&state]() { state.delete_collection(); });
	logic.on_collection_delete_dialog_cancel([&state]() { state.collection_delete_dialog_cancel(); });
	logic.on_collection_delete_dialog_confirm([&state]() { state.collection_delete_dialog_confirm(); });
	logic.on_new_request([&state]() { state.new_request(); });
	logic.on_delete_request([&state]() { state.delete_request(); });
	logic.on_duplicate_request([&state]() { state.duplicate_request(); });
	logic.on_save_collections([&state]() { state.save_collections(); });
	logic.on_import_openapi([&state]() { state.import_openapi_dialog(); });
	logic.on_import_openapi_url([&state]() { state.import_openapi_from_url_ui(); });
	logic.on_environment_changed([&state](slint::SharedString name) {
		state.environment_changed(shared_string_to_std(name));
	});
	logic.on_new_environment([&state]() { state.new_environment(); });
	logic.on_delete_environment([&state]() { state.delete_environment(); });
	logic.on_commit_environment_name([&state]() { state.commit_environment_name(); });
	logic.on_commit_environment_variables([&state]() { state.commit_environment_variables(); });
	logic.on_request_body_edited([&state]() { state.commit_request_body(); });
	logic.on_request_method_changed([&state]() { state.persist_request_editor_to_item(); });
	logic.on_request_body_kind_changed([&state]() { state.persist_request_editor_to_item(); });
	logic.on_query_param_key_edited([&state](int idx, slint::SharedString text) {
		state.query_param_key_edited(idx, std::move(text));
	});
	logic.on_query_param_value_edited([&state](int idx, slint::SharedString text) {
		state.query_param_value_edited(idx, std::move(text));
	});
	logic.on_query_param_enabled_changed([&state](int idx, bool on) { state.query_param_enabled_changed(idx, on); });
	logic.on_query_params_text_edited([&state]() { state.query_params_text_edited(); });
	logic.on_request_elapsed_tick([&state]() { state.tick_request_elapsed(); });
	logic.on_add_query_param([&state]() { state.add_query_param(); });
	logic.on_remove_query_param([&state](int idx) { state.remove_query_param(idx); });
	logic.on_request_header_key_edited([&state](int idx, slint::SharedString text) {
		state.request_header_key_edited(idx, std::move(text));
	});
	logic.on_request_header_value_edited([&state](int idx, slint::SharedString text) {
		state.request_header_value_edited(idx, std::move(text));
	});
	logic.on_request_header_enabled_changed([&state](int idx, bool on) { state.request_header_enabled_changed(idx, on); });
	logic.on_add_request_header([&state]() { state.add_request_header(); });
	logic.on_remove_request_header([&state](int idx) { state.remove_request_header(idx); });
	logic.on_commit_collection_name([&state]() { state.commit_collection_name(); });
	logic.on_commit_request_name([&state]() { state.commit_request_name(); });
	logic.on_response_jsonpath_changed([&state]() { state.response_jsonpath_changed(); });
	logic.on_response_font_selected([&state](int i) { state.apply_response_font_index(i); });
	logic.on_adjust_response_font_size([&state](int d) { state.adjust_response_font_size(d); });
	logic.on_commit_response_font_size([&state](float px) { state.commit_response_font_size(px); });

	ui->run();
	// Window geometry first (splitter positions, sizes) while the UI is still in its last layout state.
	{
		auto& g = ui->global<AppLogic>();
		apikulture::window_state::PersistedWindowState snapshot;
		snapshot.maximized = ui->window().is_maximized();
		snapshot.response_body_font_family = shared_string_to_std(g.get_response_body_font_family());
		snapshot.response_body_font_size_px = g.get_response_body_font_size();
		snapshot.collection_panel_width_px = ui->get_collection_panel_width();
		snapshot.request_panel_width_px = ui->get_request_panel_width();
		snapshot.sidebar_collections_height_px = ui->get_sidebar_collections_height();
		snapshot.sidebar_requests_section_height_px = ui->get_sidebar_requests_section_height();
		snapshot.request_query_panel_height_px = ui->get_request_query_panel_height();
		snapshot.request_headers_panel_height_px = ui->get_request_headers_panel_height();
		snapshot.response_headers_panel_height_px = ui->get_response_headers_panel_height();
		apikulture::window_state::save_window_session(snapshot);
	}
	// Collections JSON after window-state.json (may touch globals; keep layout snapshot above).
	state.save_collections_state();
	stop_theme_watcher();
	return 0;
}
