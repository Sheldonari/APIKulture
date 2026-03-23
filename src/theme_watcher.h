#pragma once

#include "MainWindow.h"
#include <optional>
#include <slint.h>

/// When \a follow_system is true (no forced light/dark from env/CLI), a background thread
/// polls the desktop theme (Linux: GNOME/portal) and updates the Slint palette while the app runs.
void start_theme_watcher(
	const slint::ComponentHandle<MainWindow>& ui,
	bool follow_system,
	std::optional<slint::cbindgen_private::ColorScheme> initial_scheme);

/// Stop the watcher thread; call after \c ui->run() returns.
void stop_theme_watcher();
