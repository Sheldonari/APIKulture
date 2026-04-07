#include "clipboard.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace apikulture::clipboard {

#if defined(__linux__) && defined(APIKULTURE_HAVE_X11_CLIPBOARD)
namespace detail {
bool set_utf8_x11(std::string_view text);
}
#endif

namespace {

std::optional<std::string> read_pipe_stdout(const char* cmd) {
	FILE* p = ::popen(cmd, "r");
	if (!p) return std::nullopt;
	std::string out;
	std::array<char, 4096> buf{};
	while (true) {
		const size_t n = std::fread(buf.data(), 1, buf.size(), p);
		if (n == 0) break;
		out.append(buf.data(), n);
	}
	const int st = ::pclose(p);
	if (st != 0) return std::nullopt;
	while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
	return out;
}

}  // namespace

std::optional<std::string> get_utf8() {
#if defined(_WIN32)
	if (!OpenClipboard(nullptr)) return std::nullopt;
	HANDLE h = GetClipboardData(CF_UNICODETEXT);
	if (!h) {
		CloseClipboard();
		return std::nullopt;
	}
	const wchar_t* w = static_cast<const wchar_t*>(GlobalLock(h));
	if (!w) {
		CloseClipboard();
		return std::nullopt;
	}
	const int need = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
	if (need < 1) {
		GlobalUnlock(h);
		CloseClipboard();
		return std::nullopt;
	}
	std::string out(static_cast<size_t>(std::max(0, need - 1)), '\0');
	if (need > 1) {
		WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), need, nullptr, nullptr);
	}
	GlobalUnlock(h);
	CloseClipboard();
	return out;
#elif defined(__APPLE__)
	if (auto r = read_pipe_stdout("pbpaste")) return r;
	return std::nullopt;
#elif defined(__linux__)
	(void)0;
	return std::nullopt;
#else
	return std::nullopt;
#endif
}

bool set_utf8(std::string_view text) {
#if defined(_WIN32)
	if (!OpenClipboard(nullptr)) return false;
	EmptyClipboard();
	const int wlen = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	if (wlen <= 0) {
		CloseClipboard();
		return false;
	}
	const size_t bytes = static_cast<size_t>(wlen + 1) * sizeof(wchar_t);
	HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
	if (!h) {
		CloseClipboard();
		return false;
	}
	auto* w = static_cast<wchar_t*>(GlobalLock(h));
	if (!w) {
		GlobalFree(h);
		CloseClipboard();
		return false;
	}
	MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), w, wlen);
	w[wlen] = L'\0';
	GlobalUnlock(h);
	if (!SetClipboardData(CF_UNICODETEXT, h)) {
		GlobalFree(h);
		CloseClipboard();
		return false;
	}
	CloseClipboard();
	return true;
#elif defined(__APPLE__)
	FILE* p = ::popen("pbcopy", "w");
	if (!p) return false;
	const size_t n = std::fwrite(text.data(), 1, text.size(), p);
	const int st = ::pclose(p);
	return st == 0 && n == text.size();
#elif defined(__linux__)
#	if defined(APIKULTURE_HAVE_X11_CLIPBOARD)
	return apikulture::clipboard::detail::set_utf8_x11(text);
#	else
	(void)text;
	return false;
#	endif
#else
	(void)text;
	return false;
#endif
}

}  // namespace apikulture::clipboard
