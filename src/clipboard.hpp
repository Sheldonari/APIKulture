#ifndef APIKULTURE_CLIPBOARD_H
#define APIKULTURE_CLIPBOARD_H

#include <optional>
#include <string>
#include <string_view>

namespace apikulture::clipboard {

/// Places UTF-8 text on the system clipboard (best effort; returns false if unsupported).
bool set_utf8(std::string_view text);

/// Reads plain text from the clipboard as UTF-8 (best effort; empty optional if unavailable).
std::optional<std::string> get_utf8();

}  // namespace apikulture::clipboard

#endif
