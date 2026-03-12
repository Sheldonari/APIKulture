#include "MainWindow.h"
#include "app_state.h"
#include <slint.h>

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  auto ui = MainWindow::create();
  apikulture::AppState state(ui);

  ui->global<AppLogic>().on_send_request([&state]() { state.send_request(); });
  ui->global<AppLogic>().on_cancel_request([&state]() { state.cancel_request(); });

  ui->run();
  return 0;
}
