# APIkulture

A multiplatform REST API tester (Yaak/Insomnia-style) built with C++ and Slint. Supports Windows, Linux, and macOS.

## Features

- **Collections & requests (sidebar)**: multiple named collections, each with multiple saved requests; select a row to load method, URL, headers, and body into the editor
- **Persistence**: collections saved as JSON under `$XDG_CONFIG_HOME/apikulture/collections.json` or `~/.config/apikulture/collections.json` (auto-save on exit; use **Save all** anytime). Each collection has its own **environments** (names, base URL, variables); switching collections updates the environment UI accordingly. Older configs (workspace-level environments) are migrated into each collection on load (`version` 3).
- Actions: **+ Collection**, **New** / **Dup** / **Del** request, **Save all**, **Import OpenAPI**
- **OpenAPI 3.x (JSON)**: import a spec into a new collection (one saved request per operation). Path parameters `{id}` become `{{id}}` for your environment variables. Query/header parameters and JSON `requestBody` examples are filled when present. The **Default** environment’s **Base URL** is set from the first `servers[0].url` (relative URLs like `/api/v3` are resolved against the document URL when you import via **Fetch**). Import from a **local file** (button or `--import-openapi=PATH`) or from a **URL** (sidebar field + **Fetch**, or `--import-openapi-url=https://…`). URLs without `http://` / `https://` get `https://` prepended. YAML specs are not read directly—convert with e.g. `yq -o=json openapi.yaml > openapi.json`. **HTTPS** needs OpenSSL when building (same as sending HTTPS requests).
- HTTP methods: GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS
- URL and custom headers (**Name** / **Value** columns; each row must be **checked** to be sent). Example for DeepL: name `Authorization`, value `DeepL-Auth-Key YOUR_KEY` (same as curl `--header`). Use `{{var}}` in header key/value for environment variables.
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

### Runtime: Slint shared library (`libslint_cpp.so` / `slint_cpp.dll`)

APIkulture is normally **linked dynamically** against Slint. The system must then load **`libslint_cpp.so`** on Linux or **`slint_cpp.dll`** on Windows (plus any other Slint/runtime DLLs the SDK ships). If that library is missing from the loader search path, the app exits immediately with an error such as `error while loading shared libraries: libslint_cpp.so`.

This applies when you use the **pre-built Slint C++ SDK** or a **default shared build** of Slint (including typical **FetchContent** builds). **Static** Slint builds embed the runtime instead; they are not covered here.

**`cmake --install` on Linux** — The project sets **`INSTALL_RPATH`** to `$ORIGIN/../lib` and installs **`libslint_cpp.so`** into `<prefix>/lib` next to a normal layout (`bin/apikulture`, `lib/libslint_cpp.so`), so you do not need **`LD_LIBRARY_PATH`** after a prefix install. Re-run CMake from a clean build directory if install rules were added after your first configure.

#### Linux — Ubuntu

Ubuntu’s default apt repositories do **not** ship `libslint_cpp` as a standalone package. Use one of these:

1. **Official Slint SDK (tarball)** — Download `slint-cpp-*-Linux-x86_64.tar.gz` from [Slint releases](https://github.com/slint-ui/slint/releases), extract it, then either:
	- **Per session:**  
	  `export LD_LIBRARY_PATH="/path/to/slint-cpp-…/lib:$LD_LIBRARY_PATH"`  
	  before starting `./apikulture`, or
	- **System-wide:** copy the `.so` files from the archive’s `lib/` directory into `/usr/local/lib` (or another directory on the loader path) and run `sudo ldconfig`.

2. **Same tree as CMake** — If you built with `SLINT_DIR` pointing at an extracted SDK, use that SDK’s `lib` directory in `LD_LIBRARY_PATH` when you run the binary from another folder.

#### Linux — Arch

Use the **AUR** so the libraries are installed where the dynamic linker expects them, for example:

- **`slint-cpp-bin`** — pre-built Slint C++ binaries (faster install), or  
- **`slint-cpp`** — builds Slint from source.

Install with your AUR helper (e.g. `yay -S slint-cpp-bin`). Package names and file paths may change; check the PKGBUILD if linking still fails.

#### Windows

Install or unpack the **Slint C++ SDK** for Windows and ensure **`slint_cpp.dll`** and the other runtime DLLs from the SDK’s `lib` folder are **next to `apikulture.exe`**, or add that `lib` directory to **`PATH`**.

This repo’s `CMakeLists.txt` runs a **post-build copy** of runtime DLLs into the executable directory on Windows; if you move only `apikulture.exe` elsewhere, copy those DLLs too.

### Theme (light / dark)

The project defaults to Slint’s **`material`** style (`SLINT_STYLE` in CMake). Material/fluent widgets use Slint’s `Palette` and **`Palette.color-scheme` actually changes** light vs dark.

The **`qt`** style is different: it copies colors from Qt’s `QApplication` palette and effectively **ignores** `Palette.color-scheme` for drawing, so env vars and `set_window_color_scheme` have almost no visible effect. If you still have an old build with `SLINT_STYLE=qt`, reconfigure with `-DSLINT_STYLE=material` or delete `CMakeCache.txt` and run CMake again.

**Overrides** (material style):

- Environment: `APIKULTURE_COLOR_SCHEME=light` or `=dark` (also `system` / `auto` to detect on Linux).
- CLI: `--color-scheme=light` or `--color-scheme dark`.

On **Linux**, when unset or `system`/`auto`, detection uses **GNOME `gsettings` first** (including `color-scheme` `'default'` + `gtk-application-prefer-dark-theme`), then **XDG portal** via `ReadOne` (same as Slint’s winit backend), then KDE. If auto-detection fails, use `APIKULTURE_COLOR_SCHEME` or `--color-scheme`.

While the app is running, the theme **updates automatically** when you follow the system (no `light`/`dark` forced): a background thread re-checks the desktop preference about every **750 ms** and applies it on the Slint event loop. Forced `APIKULTURE_COLOR_SCHEME=light|dark` or `--color-scheme light|dark` disables live switching so the choice stays fixed.

### Multiplatform notes

- **Windows**: A console window is shown by default; to hide it, link with `/SUBSYSTEM:WINDOWS` (see Slint docs). See **Runtime: Slint shared library** for DLL layout.
- **Linux**: See **Runtime: Slint shared library** for `libslint_cpp.so` (Ubuntu / Arch instructions above).
- **macOS**: Dynamic Slint builds need the SDK’s shared libraries on the loader path at runtime (install Slint to a prefix, set rpath, or follow [Slint’s C++ setup](https://docs.slint.dev/latest/docs/cpp/cmake/) for your layout).

## License

[MIT](LICENSE) — Copyright (c) 2026 Sheldonari
