#include <slint.h>
#include "MainWindow.h"
#include "app_state.h"
#include "platform_color_scheme.h"
#include "theme_watcher.h"
#include "window_state.hpp"

#include <cstdlib>
#include <cstring>
#include <optional>

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

} // namespace

int main(int argc, char** argv) {
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
		g.set_method(slint::SharedString("GET"));
		g.set_url(slint::SharedString(""));
		g.set_request_headers(slint::SharedString(""));
		g.set_request_body(slint::SharedString(""));
		g.set_response_status(slint::SharedString(""));
		g.set_response_headers(slint::SharedString(""));
		g.set_response_jsonpath(slint::SharedString(""));
		g.set_response_body(slint::SharedString(""));
	}

	if (apikulture::window_state::load_main_window_maximized()) {
		ui->window().set_maximized(true);
	}

	state.init_collections_ui();

	auto& logic = ui->global<AppLogic>();
	logic.on_send_request([&state]() { state.send_request(); });
	logic.on_cancel_request([&state]() { state.cancel_request(); });
	logic.on_select_collection([&state](int i) { state.select_collection(i); });
	logic.on_select_request([&state](int i) { state.select_request(i); });
	logic.on_new_collection([&state]() { state.new_collection(); });
	logic.on_new_request([&state]() { state.new_request(); });
	logic.on_delete_request([&state]() { state.delete_request(); });
	logic.on_duplicate_request([&state]() { state.duplicate_request(); });
	logic.on_save_collections([&state]() { state.save_collections(); });
	logic.on_commit_collection_name([&state]() { state.commit_collection_name(); });
	logic.on_commit_request_name([&state]() { state.commit_request_name(); });
	logic.on_response_jsonpath_changed([&state]() { state.response_jsonpath_changed(); });

	ui->run();
	apikulture::window_state::save_main_window_maximized(ui->window().is_maximized());
	stop_theme_watcher();
	return 0;
}
