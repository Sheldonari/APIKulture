#include "file_dialog.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif

namespace apikulture::file_dialog {

namespace {

std::string trim_trailing_newline(std::string s) {
	while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
	return s;
}

#if defined(_WIN32)

std::string wide_to_utf8(const wchar_t* w, int len) {
	if (len <= 0) return {};
	int need = WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
	if (need <= 0) return {};
	std::string out(static_cast<size_t>(need), '\0');
	WideCharToMultiByte(CP_UTF8, 0, w, len, out.data(), need, nullptr, nullptr);
	return out;
}

std::string pick_win32() {
	std::array<wchar_t, 4096> buf{};
	OPENFILENAMEW ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = buf.data();
	ofn.nMaxFile = static_cast<DWORD>(buf.size());
	ofn.lpstrFilter = L"OpenAPI JSON\0*.json\0All files\0*.*\0\0";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	ofn.lpstrTitle = L"Import OpenAPI";
	if (GetOpenFileNameW(&ofn)) {
		return wide_to_utf8(buf.data(), static_cast<int>(wcslen(buf.data())));
	}
	return {};
}

#elif defined(__APPLE__)

std::string pick_macos() {
	std::array<char, 8192> out{};
	FILE* pipe = popen(
			"osascript -e 'return POSIX path of (choose file with prompt \"Import OpenAPI\")' "
			"2>/dev/null",
			"r");
	if (!pipe) return {};
	const size_t n = fread(out.data(), 1, out.size() - 1, pipe);
	const int st = pclose(pipe);
	if (st != 0) return {};
	if (n == 0) return {};
	out[n] = '\0';
	return trim_trailing_newline(std::string(out.data()));
}

#else

std::string pick_zenity() {
	std::array<char, 8192> out{};
	// zenity prints the path or nothing on cancel.
	FILE* pipe = popen(
			"zenity --file-selection --title='Import OpenAPI' "
			"--file-filter='OpenAPI JSON (*.json)|*.json|All files|*.*' 2>/dev/null",
			"r");
	if (!pipe) return {};
	const size_t n = fread(out.data(), 1, out.size() - 1, pipe);
	const int st = pclose(pipe);
	if (st != 0) return {};
	if (n == 0) return {};
	out[n] = '\0';
	return trim_trailing_newline(std::string(out.data()));
}

std::string pick_kdialog() {
	std::array<char, 8192> out{};
	FILE* pipe = popen("kdialog --getopenfilename . '*.json|OpenAPI JSON' --title 'Import OpenAPI' 2>/dev/null",
			"r");
	if (!pipe) return {};
	const size_t n = fread(out.data(), 1, out.size() - 1, pipe);
	const int st = pclose(pipe);
	if (st != 0) return {};
	if (n == 0) return {};
	out[n] = '\0';
	return trim_trailing_newline(std::string(out.data()));
}

#endif

}  // namespace

std::string pick_openapi_json_file() {
#if defined(_WIN32)
	return pick_win32();
#elif defined(__APPLE__)
	return pick_macos();
#else
	std::string z = pick_zenity();
	if (!z.empty()) return z;
	return pick_kdialog();
#endif
}

}  // namespace apikulture::file_dialog
