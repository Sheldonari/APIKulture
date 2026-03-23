# APIkulture

A multiplatform REST API tester (Yaak/Insomnia-style) built with C++ and Slint. Supports Windows, Linux, and macOS.

## Features

- **Collections & requests (sidebar)**: multiple named collections, each with multiple saved requests; select a row to load method, URL, headers, and body into the editor
- **Persistence**: collections saved as JSON under `$XDG_CONFIG_HOME/apikulture/collections.json` or `~/.config/apikulture/collections.json` (auto-save on exit; use **Save all** anytime)
- Actions: **+ Collection**, **New** / **Dup** / **Del** request, **Save all**
- HTTP methods: GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS
- URL and custom headers (one per line: `Key: value`)
- Request body (raw/JSON)
- Response: status, headers, body with JSON pretty-print
- Send / Cancel; non-blocking UI (requests run on a worker thread)
- HTTPS (when built with OpenSSL)

## Build

### Requirements

- CMake 3.21+
- C++17 compiler
- **Slint** (one of):
	- Pre-built Slint C++ SDK: set `SLINT_DIR` to the extracted directory (e.g. from [Slint releases](https://github.com/slint-ui/slint/releases), use `slint-cpp-*-Linux-x86_64.tar.gz` or the matching archive for your OS), or
	- Build from source via FetchContent: **Rust** toolchain (for Slint’s build)
- OpenSSL (optional but recommended for HTTPS)
- nlohmann/json and cpp-httplib are fetched by CMake

### Configure and build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

If you use a pre-built Slint:

```bash
export SLINT_DIR=/path/to/slint-cpp-XXX-Linux-x86_64  # or your platform
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Run:

```bash
./apikulture   # or apikulture.exe on Windows
```

### Theme (light / dark)

The project defaults to Slint’s **`material`** style (`SLINT_STYLE` in CMake). Material/fluent widgets use Slint’s `Palette` and **`Palette.color-scheme` actually changes** light vs dark.

The **`qt`** style is different: it copies colors from Qt’s `QApplication` palette and effectively **ignores** `Palette.color-scheme` for drawing, so env vars and `set_window_color_scheme` have almost no visible effect. If you still have an old build with `SLINT_STYLE=qt`, reconfigure with `-DSLINT_STYLE=material` or delete `CMakeCache.txt` and run CMake again.

**Overrides** (material style):

- Environment: `APIKULTURE_COLOR_SCHEME=light` or `=dark` (also `system` / `auto` to detect on Linux).
- CLI: `--color-scheme=light` or `--color-scheme dark`.

On **Linux**, when unset or `system`/`auto`, detection uses **GNOME `gsettings` first** (including `color-scheme` `'default'` + `gtk-application-prefer-dark-theme`), then **XDG portal** via `ReadOne` (same as Slint’s winit backend), then KDE. If auto-detection fails, use `APIKULTURE_COLOR_SCHEME` or `--color-scheme`.

While the app is running, the theme **updates automatically** when you follow the system (no `light`/`dark` forced): a background thread re-checks the desktop preference about every **750 ms** and applies it on the Slint event loop. Forced `APIKULTURE_COLOR_SCHEME=light|dark` or `--color-scheme light|dark` disables live switching so the choice stays fixed.

### Multiplatform notes

- **Windows**: A console window is shown by default; to hide it, link with `/SUBSYSTEM:WINDOWS` (see Slint docs).
- **Linux / macOS**: No extra steps.

## License

Use and modify as you like.
