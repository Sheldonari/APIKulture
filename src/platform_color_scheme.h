#pragma once

#include <optional>
#include <slint.h>

/// Best-effort desktop light/dark preference (Linux: portal + GNOME settings).
/// Returns nullopt when unknown — do not call Slint setters in that case.
std::optional<slint::cbindgen_private::ColorScheme> platform_detect_desktop_color_scheme();
