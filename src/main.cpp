#include <slint.h>
#include "MainWindow.h"
#include "app_state.h"

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

	auto ui = MainWindow::create();
	AppState state(ui);

	// Initialize string properties so they are never in an undefined (null) state when read
	{
		auto& g = ui->global<AppLogic>();
		g.set_method(slint::SharedString("GET"));
		g.set_url(slint::SharedString(""));
		g.set_request_headers(slint::SharedString(""));
		g.set_request_body(slint::SharedString(""));
		g.set_response_status(slint::SharedString(""));
		g.set_response_headers(slint::SharedString(""));
		g.set_response_body(slint::SharedString(""));
	}

	ui->global<AppLogic>().on_send_request([&state]() { state.send_request(); });
	ui->global<AppLogic>().on_cancel_request([&state]() { state.cancel_request(); });

	ui->run();
	return 0;
}
