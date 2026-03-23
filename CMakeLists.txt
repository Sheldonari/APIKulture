cmake_minimum_required(VERSION 3.21)
project(apikulture VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

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

# OpenSSL for HTTPS (optional but recommended)
find_package(OpenSSL)
if(OpenSSL_FOUND)
	add_compile_definitions(CPPHTTPLIB_OPENSSL_SUPPORT)
endif()

add_executable(apikulture
	src/main.cpp
	src/app_state.cpp
	src/http_client.cpp
	src/collections_io.cpp
)

target_include_directories(apikulture PRIVATE
	${cpp-httplib_SOURCE_DIR}
	${nlohmann_json_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(OpenSSL_FOUND)
	target_link_libraries(apikulture PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

slint_target_sources(apikulture ui/MainWindow.slint)
target_link_libraries(apikulture PRIVATE Slint::Slint)

if(WIN32)
	add_custom_command(TARGET apikulture POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_RUNTIME_DLLS:apikulture>
			$<TARGET_FILE_DIR:apikulture>
		COMMAND_EXPAND_LISTS
	)
endif()
