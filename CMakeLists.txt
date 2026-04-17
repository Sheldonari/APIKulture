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

# jsoncons (JSONPath via jsoncons_ext) — tests off: default ON pulls in CTest, which needs a
# complete CMake install (some ~/.local CMake trees lack Templates/CTestScript.cmake.in).
set(JSONCONS_BUILD_TESTS OFF CACHE BOOL "jsoncons test suite" FORCE)
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

# Linux: Slint is usually linked to libslint_cpp.so; `make install` must ship that library and
# set RPATH so the installed binary finds it under prefix (without LD_LIBRARY_PATH).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(_apikulture_install_libdir "${CMAKE_INSTALL_LIBDIR}")
	if(_apikulture_install_libdir MATCHES "^/")
		set(_apikulture_rpath "${_apikulture_install_libdir}")
	else()
		set(_apikulture_rpath "\$ORIGIN/../${_apikulture_install_libdir}")
	endif()
	set_target_properties(apikulture PROPERTIES INSTALL_RPATH "${_apikulture_rpath}")
	if(TARGET slint_cpp-shared)
		set_target_properties(apikulture PROPERTIES
			BUILD_RPATH_USE_ORIGIN ON
			BUILD_RPATH "$<TARGET_FILE_DIR:slint_cpp-shared>"
		)
	endif()
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
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		if(TARGET slint_cpp-shared)
			install(FILES $<TARGET_FILE:slint_cpp-shared> DESTINATION ${CMAKE_INSTALL_LIBDIR})
		elseif(DEFINED ENV{SLINT_DIR} AND EXISTS "$ENV{SLINT_DIR}/lib")
			file(GLOB _apikulture_slint_so LIST_DIRECTORIES false "$ENV{SLINT_DIR}/lib/libslint_cpp.so*")
			if(_apikulture_slint_so)
				install(FILES ${_apikulture_slint_so} DESTINATION ${CMAKE_INSTALL_LIBDIR})
			endif()
		endif()
	endif()
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/apikulture.desktop"
		DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/ui/icons/apikulture.svg"
		DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
		RENAME apikulture.svg)
endif()

# -----------------------------------------------------------------------------
# CPack: native .deb / .rpm (and TGZ). Build a Release tree with prefix /usr, then:
#   cd build && cpack -G DEB    # Debian / Ubuntu (dpkg-dev for shlibdeps)
#   cpack -G RPM                # Fedora / openSUSE (rpm-build)
#   cpack -G TGZ                # plain archive
# -----------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "Sheldonari")
set(CPACK_PACKAGE_CONTACT "Sheldonari <https://codeberg.org/Sheldonari/APIKulture>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native REST API client (collections, OpenAPI, HTTPS)")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
	set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
endif()
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}")
set(CPACK_PACKAGE_CHECKSUM "SHA256")

if(UNIX AND NOT APPLE AND NOT WIN32)
	set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://www.apikulture.com")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	# Resolves libc, libssl, X11, etc. from apikulture + bundled libslint_cpp.so (needs dpkg-dev).
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_URL "https://www.apikulture.com")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")

include(CPack)
