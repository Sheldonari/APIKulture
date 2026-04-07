cmake_minimum_required(VERSION 3.21)
project(apikulture VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
include(GNUInstallDirs)

include(FetchContent)

# Slint: `qt` style mirrors QApplication's palette only — setting Palette.color-scheme from code/env
# does not change appearance. `material` / `fluent` use Slint's palette and respect color-scheme.
if(NOT SLINT_STYLE)
	set(SLINT_STYLE material CACHE STRING "The Slint widget style")
endif()

# Slint: prefer package, else FetchContent (requires Rust to build from source)
if(DEFINED ENV{SLINT_DIR})
	set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};$ENV{SLINT_DIR}")
endif()
find_package(Slint 1.0 QUIET)
if(NOT Slint_FOUND)
	FetchContent_Declare(
			Slint
			GIT_REPOSITORY https://github.com/slint-ui/slint.git
			GIT_TAG release/1
			SOURCE_SUBDIR api/cpp
	)
	FetchContent_MakeAvailable(Slint)
endif()

# cpp-httplib (header-only)
FetchContent_Declare(
		cpp-httplib
		GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
		GIT_TAG v0.14.3
)
FetchContent_MakeAvailable(cpp-httplib)

# nlohmann/json (header-only)
FetchContent_Declare(
		nlohmann_json
		GIT_REPOSITORY https://github.com/nlohmann/json.git
		GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# jsoncons (JSONPath via jsoncons_ext)
FetchContent_Declare(
		jsoncons
		GIT_REPOSITORY https://github.com/danielaparker/jsoncons.git
		GIT_TAG v1.6.0
)
FetchContent_MakeAvailable(jsoncons)

# OpenSSL for HTTPS (optional but recommended)
find_package(OpenSSL)
if(OpenSSL_FOUND)
	add_compile_definitions(CPPHTTPLIB_OPENSSL_SUPPORT)
endif()

add_executable(apikulture
	src/main.cpp
	src/platform_color_scheme.cpp
	src/theme_watcher.cpp
	src/app_state.cpp
	src/http_client.cpp
	src/collections_io.cpp
	src/environment.cpp
	src/query_string.cpp
	src/window_state.cpp
	src/openapi_import.cpp
	src/file_dialog.cpp
	src/response_json_highlighter.cpp
	src/clipboard.cpp
	src/url_import.cpp
)

target_include_directories(apikulture PRIVATE
	${cpp-httplib_SOURCE_DIR}
	${nlohmann_json_SOURCE_DIR}/include
	${jsoncons_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(OpenSSL_FOUND)
	target_link_libraries(apikulture PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

if(WIN32)
	target_link_libraries(apikulture PRIVATE comdlg32)
endif()

slint_target_sources(apikulture ui/MainWindow.slint)
target_link_libraries(apikulture PRIVATE Slint::Slint)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	find_package(X11 QUIET)
	if(X11_FOUND)
		target_sources(apikulture PRIVATE src/clipboard_linux_x11.cpp)
		target_link_libraries(apikulture PRIVATE X11::X11)
		target_compile_definitions(apikulture PRIVATE APIKULTURE_HAVE_X11_CLIPBOARD=1)
	endif()
endif()

if(WIN32)
	add_custom_command(TARGET apikulture POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_RUNTIME_DLLS:apikulture>
			$<TARGET_FILE_DIR:apikulture>
		COMMAND_EXPAND_LISTS
	)
endif()

# Freedesktop icon + launcher (GNOME dock / app menu resolve Icon= by name from hicolor theme)
if(UNIX AND NOT APPLE)
	install(TARGETS apikulture RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/apikulture.desktop"
		DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/ui/icons/apikulture.svg"
		DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
		RENAME apikulture.svg)
endif()
