#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_TYPE="${A3_BUILD_TYPE:-Debug}"
BINARY="$SCRIPT_DIR/build/src/a3-motion-ui/a3-motion-ui_artefacts/$BUILD_TYPE/Standalone/a3-motion-ui"

if [ ! -x "$BINARY" ]; then
	ALT_TYPE="Release"
	if [ "$BUILD_TYPE" = "Release" ]; then
		ALT_TYPE="Debug"
	fi
	ALT_BINARY="$SCRIPT_DIR/build/src/a3-motion-ui/a3-motion-ui_artefacts/$ALT_TYPE/Standalone/a3-motion-ui"

	if [ -x "$ALT_BINARY" ]; then
		BINARY="$ALT_BINARY"
	else
		echo "Binary not found: $BINARY"
		echo "Build first, e.g.: ./build.sh -d"
		exit 127
	fi
fi

exec "$BINARY" "$@"
