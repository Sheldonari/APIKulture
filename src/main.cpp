#include <slint.h>
#include "MainWindow.h"
#include "app_state.h"

#include <cstdlib>
#include <cstring>

namespace {

slint::cbindgen_private::ColorScheme color_scheme_from_env()
{
	const char* raw = std::getenv("APIKULTURE_COLOR_SCHEME");
	if (!raw || !*raw)
		return slint::cbindgen_private::ColorScheme::Unknown;
	// Slint naming: Dark = light chrome (bright bg), Light = dark chrome (dark bg)
	if (std::strcmp(raw, "system") == 0 || std::strcmp(raw, "auto") == 0)
		return slint::cbindgen_private::ColorScheme::Unknown;
	if (std::strcmp(raw, "light") == 0)
		return slint::cbindgen_private::ColorScheme::Dark;
	if (std::strcmp(raw, "dark") == 0)
		return slint::cbindgen_private::ColorScheme::Light;
	return slint::cbindgen_private::ColorScheme::Unknown;
}

} // namespace

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

	auto ui = MainWindow::create();
	AppState state(ui);

	// Initialize string properties so they are never in an undefined (null) state when read
	{
		auto& g = ui->global<AppLogic>();
		ui->set_window_color_scheme(color_scheme_from_env());
		g.set_method(slint::SharedString("GET"));
		g.set_url(slint::SharedString(""));
		g.set_request_headers(slint::SharedString(""));
		g.set_request_body(slint::SharedString(""));
		g.set_response_status(slint::SharedString(""));
		g.set_response_headers(slint::SharedString(""));
		g.set_response_body(slint::SharedString(""));
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

	ui->run();
	return 0;
}
