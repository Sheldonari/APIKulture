#include "platform_color_scheme.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>

#if defined(__linux__)

static std::string read_command_output(const char* cmd)
{
	std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(cmd, "r"), pclose);
	if (!pipe)
		return {};
	std::string out;
	char buf[512];
	while (fgets(buf, sizeof buf, pipe.get()) != nullptr)
		out += buf;
	return out;
}

static void to_lower_ascii(std::string& s)
{
	for (char& c : s)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

static std::string trim(std::string s)
{
	while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
		s.pop_back();
	size_t i = 0;
	while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
		++i;
	return s.substr(i);
}

// Capture stderr too — gsettings often prints errors only there (wrong session, etc.).
static std::string read_shell_merged(const char* cmd)
{
	return read_command_output(cmd);
}

// gdbus output varies: "uint32 2", "<uint32 2>", nested variants — scan for uint32 tokens.
static std::optional<unsigned> parse_uint32_color_scheme(const std::string& out)
{
	for (size_t pos = 0;;) {
		pos = out.find("uint32", pos);
		if (pos == std::string::npos)
			break;
		pos += 6;
		while (pos < out.size() && (out[pos] == ' ' || out[pos] == '\t' || out[pos] == '\n'))
			++pos;
		if (pos >= out.size())
			break;
		char* end = nullptr;
		unsigned long v = std::strtoul(out.c_str() + pos, &end, 10);
		if (end == out.c_str() + pos) {
			++pos;
			continue;
		}
		if (v <= 2)
			return static_cast<unsigned>(v);
		pos = static_cast<size_t>(end - out.c_str());
	}
	return std::nullopt;
}

static std::optional<unsigned> xdg_portal_color_scheme_u32()
{
	const char* commands[] = {
		"gdbus call --session --dest org.freedesktop.portal.Desktop "
		"--object-path /org/freedesktop/portal/desktop "
		"--method org.freedesktop.portal.Settings.ReadOne "
		"org.freedesktop.appearance color-scheme 2>&1",
		"gdbus call --session --dest org.freedesktop.portal.Desktop "
		"--object-path /org/freedesktop/portal/desktop "
		"--method org.freedesktop.portal.Settings.Read "
		"org.freedesktop.appearance color-scheme 2>&1",
	};
	for (const char* cmd : commands) {
		std::string out = read_shell_merged(cmd);
		if (auto v = parse_uint32_color_scheme(out))
			return v;
	}
	return std::nullopt;
}

static std::optional<bool> parse_bool_line(std::string line)
{
	line = trim(std::move(line));
	to_lower_ascii(line);
	if (line == "true" || line == "1")
		return true;
	if (line == "false" || line == "0")
		return false;
	return std::nullopt;
}

// ~/.config/gtk-4.0/settings.ini and gtk-3.0 — used when gsettings is unavailable (PATH, snap, etc.)
static std::optional<bool> gtk_ini_prefer_dark()
{
	const char* home = std::getenv("HOME");
	if (!home || !*home)
		return std::nullopt;
	const char* paths[] = {
		"/.config/gtk-4.0/settings.ini",
		"/.config/gtk-3.0/settings.ini",
	};
	for (const char* rel : paths) {
		std::string path = std::string(home) + rel;
		std::ifstream f(path);
		if (!f)
			continue;
		std::string line;
		while (std::getline(f, line)) {
			auto t = trim(line);
			if (t.empty() || t[0] == '#' || t[0] == ';')
				continue;
			if (t.rfind("gtk-application-prefer-dark-theme", 0) != 0)
				continue;
			size_t eq = t.find('=');
			if (eq == std::string::npos)
				continue;
			std::string val = trim(t.substr(eq + 1));
			if (auto b = parse_bool_line(val))
				return b;
		}
	}
	return std::nullopt;
}

static std::optional<slint::cbindgen_private::ColorScheme> scheme_from_prefer_dark(bool prefer_dark)
{
	return prefer_dark ? slint::cbindgen_private::ColorScheme::Dark
			   : slint::cbindgen_private::ColorScheme::Light;
}

static std::optional<slint::cbindgen_private::ColorScheme> interpret_color_scheme_string(std::string cs)
{
	cs = trim(std::move(cs));
	to_lower_ascii(cs);
	if (cs.find("no such key") != std::string::npos || cs.find("not installed") != std::string::npos)
		return std::nullopt;
	if (cs.empty())
		return std::nullopt;

	if (cs.find("prefer-light") != std::string::npos)
		return slint::cbindgen_private::ColorScheme::Light;
	if (cs.find("prefer-dark") != std::string::npos)
		return slint::cbindgen_private::ColorScheme::Dark;
	if (cs.find("default") != std::string::npos) {
		std::string pdt = read_shell_merged(
			"gsettings get org.gnome.desktop.interface gtk-application-prefer-dark-theme "
			"2>&1");
		if (auto b = parse_bool_line(pdt))
			return scheme_from_prefer_dark(*b);
		pdt = read_shell_merged(
			"dconf read /org/gnome/desktop/interface/gtk-application-prefer-dark-theme 2>&1");
		if (auto b = parse_bool_line(pdt))
			return scheme_from_prefer_dark(*b);
		if (auto b = gtk_ini_prefer_dark())
			return scheme_from_prefer_dark(*b);

		std::string gtk = read_shell_merged(
			"gsettings get org.gnome.desktop.interface gtk-theme 2>&1");
		to_lower_ascii(gtk);
		if (gtk.find("dark") != std::string::npos)
			return slint::cbindgen_private::ColorScheme::Dark;
		return slint::cbindgen_private::ColorScheme::Light;
	}
	return std::nullopt;
}

static std::optional<slint::cbindgen_private::ColorScheme> gnome_detect_color_scheme()
{
	// stderr merged so we see failures; dconf often works when gsettings is not on PATH
	std::string cs = read_shell_merged(
		"gsettings get org.gnome.desktop.interface color-scheme 2>&1");
	if (auto s = interpret_color_scheme_string(cs))
		return s;

	cs = read_shell_merged("dconf read /org/gnome/desktop/interface/color-scheme 2>&1");
	if (auto s = interpret_color_scheme_string(cs))
		return s;

	// Only gtk-application-prefer-dark-theme (older GNOME / minimal schemas)
	std::string pdt = read_shell_merged(
		"gsettings get org.gnome.desktop.interface gtk-application-prefer-dark-theme "
		"2>&1");
	if (auto b = parse_bool_line(pdt))
		return scheme_from_prefer_dark(*b);
	pdt = read_shell_merged(
		"dconf read /org/gnome/desktop/interface/gtk-application-prefer-dark-theme 2>&1");
	if (auto b = parse_bool_line(pdt))
		return scheme_from_prefer_dark(*b);
	if (auto b = gtk_ini_prefer_dark())
		return scheme_from_prefer_dark(*b);

	return std::nullopt;
}

static bool looks_like_gnome_session()
{
	const char* desk = std::getenv("XDG_CURRENT_DESKTOP");
	if (desk && std::strstr(desk, "GNOME"))
		return true;
	const char* sess = std::getenv("DESKTOP_SESSION");
	if (sess) {
		std::string l = sess;
		to_lower_ascii(l);
		if (l.find("gnome") != std::string::npos)
			return true;
	}
	return false;
}

static bool looks_like_plasma_session()
{
	const char* kde = std::getenv("KDE_SESSION_VERSION");
	if (kde && *kde)
		return true;
	const char* desk = std::getenv("XDG_CURRENT_DESKTOP");
	if (!desk || !*desk)
		return false;
	return std::strstr(desk, "KDE") != nullptr;
}

static std::optional<slint::cbindgen_private::ColorScheme> kde_color_scheme()
{
	std::string out = read_shell_merged(
		"kreadconfig6 --file kdeglobals --group General --key ColorScheme 2>/dev/null");
	if (out.empty())
		out = read_shell_merged(
			"kreadconfig5 --file kdeglobals --group General --key ColorScheme 2>/dev/null");
	if (out.empty())
		return std::nullopt;
	std::string lower = out;
	to_lower_ascii(lower);
	if (lower.find("dark") != std::string::npos)
		return slint::cbindgen_private::ColorScheme::Dark;
	return slint::cbindgen_private::ColorScheme::Light;
}

#endif // __linux__

std::optional<slint::cbindgen_private::ColorScheme> platform_detect_desktop_color_scheme()
{
#if defined(__linux__)
	if (auto g = gnome_detect_color_scheme())
		return g;

	auto u = xdg_portal_color_scheme_u32();
	if (u.has_value()) {
		if (*u == 2u)
			return slint::cbindgen_private::ColorScheme::Light;
		if (*u == 1u) {
			// On GNOME, portal "prefer-dark" often disagrees with Settings / libadwaita.
			// Do not force dark from portal alone — prefer GTK/dconf/ini (same as Settings app).
			if (looks_like_gnome_session()) {
				if (auto b = gtk_ini_prefer_dark())
					return scheme_from_prefer_dark(*b);
				std::string pdt = read_shell_merged(
					"dconf read /org/gnome/desktop/interface/gtk-application-prefer-dark-theme "
					"2>&1");
				if (auto b = parse_bool_line(pdt))
					return scheme_from_prefer_dark(*b);
				return std::nullopt;
			}
			return slint::cbindgen_private::ColorScheme::Dark;
		}
	}

	if (looks_like_plasma_session())
		return kde_color_scheme();
	return std::nullopt;
#else
	(void)0;
	return std::nullopt;
#endif
}
