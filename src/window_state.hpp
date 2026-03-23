#pragma once

namespace apikulture::window_state {

/// Read persisted main-window maximized flag; default false if missing or invalid.
bool load_main_window_maximized();

/// Persist whether the main window was maximized (best-effort; ignores I/O errors).
void save_main_window_maximized(bool maximized);

}  // namespace apikulture::window_state
