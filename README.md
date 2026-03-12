# APIkulture

A multiplatform REST API tester (Yaak/Insomnia-style) built with C++ and Slint. Supports Windows, Linux, and macOS.

## Features

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

### Multiplatform notes

- **Windows**: A console window is shown by default; to hide it, link with `/SUBSYSTEM:WINDOWS` (see Slint docs).
- **Linux / macOS**: No extra steps.

## License

Use and modify as you like.
