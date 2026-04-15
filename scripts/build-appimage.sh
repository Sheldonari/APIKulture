#!/usr/bin/env bash
# Build a release AppImage for APIkulture (linuxdeploy + Qt plugin + appimagetool).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
RELEASE_DIR="${RELEASE_DIR:-$ROOT/release}"
APPDIR="$RELEASE_DIR/AppDir"

LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
APPIMAGETOOL_URL="https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"

die() {
	echo "error: $*" >&2
	exit 1
}

read_cmake_install_prefix() {
	local cache="$BUILD_DIR/CMakeCache.txt"
	if [[ -f "$cache" ]] && grep -q '^CMAKE_INSTALL_PREFIX:' "$cache"; then
		grep '^CMAKE_INSTALL_PREFIX:' "$cache" | head -1 | cut -d= -f2-
	else
		echo "/usr/local"
	fi
}

project_version_from_cmake() {
	grep -E '^[[:space:]]*project\(' "$ROOT/CMakeLists.txt" | head -1 |
		sed -n 's/.*VERSION[[:space:]]\{1,\}\([^[:space:];)]\{1,\}\).*/\1/p'
}

require_cmd() {
	command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

download_if_missing() {
	local url="$1" dest="$2"
	[[ -f "$dest" ]] && [[ -s "$dest" ]] && return 0
	echo "Downloading $(basename "$dest")..."
	curl -fsSL -o "$dest" "$url"
	chmod +x "$dest"
}

main() {
	require_cmd cmake
	require_cmd curl

	local arch
	arch="$(uname -m)"
	if [[ "$arch" != "x86_64" ]]; then
		die "this script downloads x86_64 linuxdeploy/appimagetool binaries; host is $arch"
	fi

	local version
	version="$(project_version_from_cmake)"
	[[ -n "$version" ]] || die "could not parse VERSION from CMakeLists.txt"

	local saved_prefix
	saved_prefix="$(read_cmake_install_prefix)"

	export ARCH="$arch"

	local qmake
	if command -v qmake6 >/dev/null 2>&1; then
		qmake="$(command -v qmake6)"
	elif command -v qmake >/dev/null 2>&1; then
		qmake="$(command -v qmake)"
	else
		die "qmake6 or qmake not found (Qt dev packages required for linuxdeploy-plugin-qt)"
	fi

	mkdir -p "$BUILD_DIR" "$RELEASE_DIR"
	rm -rf "$APPDIR"

	echo "Configuring and building (Release, install prefix /usr for staging)..."
	cmake -S "$ROOT" -B "$BUILD_DIR" \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=/usr
	cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)"

	echo "Installing into AppDir via DESTDIR..."
	DESTDIR="$APPDIR" cmake --install "$BUILD_DIR"

	echo "Removing FetchContent dev artifacts from AppDir..."
	rm -rf "$APPDIR/usr/include" \
		"$APPDIR/usr/share/cmake"
	shopt -s nullglob
	local d
	for d in "$APPDIR"/usr/lib/*/cmake; do
		[[ -e "$d" ]] || continue
		rm -rf "$d"
	done
	shopt -u nullglob

	local ld="$RELEASE_DIR/linuxdeploy-x86_64.AppImage"
	local ldqt="$RELEASE_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"
	local ait="$RELEASE_DIR/appimagetool-x86_64.AppImage"

	download_if_missing "$LINUXDEPLOY_URL" "$ld"
	download_if_missing "$LINUXDEPLOY_QT_URL" "$ldqt"
	download_if_missing "$APPIMAGETOOL_URL" "$ait"

	export APPIMAGE_EXTRACT_AND_RUN=1

	echo "Running linuxdeploy..."
	"$ld" --appdir "$APPDIR" \
		--executable "$APPDIR/usr/bin/apikulture" \
		--desktop-file "$APPDIR/usr/share/applications/apikulture.desktop" \
		--icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/apikulture.svg"

	echo "Running linuxdeploy-plugin-qt (qmake: $qmake)..."
	export QMAKE="$qmake"
	"$ldqt" --appdir "$APPDIR"

	local out="$RELEASE_DIR/APIkulture-${version}-${arch}.AppImage"
	rm -f "$out"

	echo "Running appimagetool..."
	export VERSION="$version"
	"$ait" "$APPDIR" "$out"

	chmod +x "$out"
	echo "Done: $out"

	echo "Restoring CMake install prefix to: $saved_prefix"
	cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_INSTALL_PREFIX="$saved_prefix"
}

main "$@"
