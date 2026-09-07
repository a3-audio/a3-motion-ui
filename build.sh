#!/bin/bash
# A3 Motion UI Build Script
#
# Usage: ./build.sh [OPTIONS]
#
# Options:
#   -d, --debug     Build Debug (slow, with symbols)
#   -r, --release   Build Release (fast, optimized) [default]
#   -c, --clean     Clean build directory first
#   -s, --restart   Restart systemd service after build
#   -h, --help      Show this help
#
# Examples:
#   ./build.sh              # Release build
#   ./build.sh -d           # Debug build
#   ./build.sh -r -s        # Release build + restart service
#   ./build.sh -c -r -s     # Clean + Release + restart

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
SRC_DIR="$SCRIPT_DIR"
BUILD_TYPE="Release"
DO_CLEAN=false
DO_RESTART=false

# Prefer explicit JUCE_DIR (cmake package dir), then JUCE_PREFIX_PATH, then CMAKE_PREFIX_PATH,
# finally fall back to ~/local/juce.
JUCE_PREFIX_PATH="${JUCE_DIR:-${JUCE_PREFIX_PATH:-${CMAKE_PREFIX_PATH:-$HOME/local/juce}}}"

# An override that points at nothing is worse than no override: a shell profile
# outlives the JUCE it was written for, and configuring against a path that is
# not there fails somewhere far from the cause. Say so and use the default.
if [ ! -d "$JUCE_PREFIX_PATH" ]; then
    echo "!!! JUCE_DIR/JUCE_PREFIX_PATH points at $JUCE_PREFIX_PATH, which does not exist."
    echo "!!! Using $HOME/local/juce instead. Check your shell profile."
    JUCE_PREFIX_PATH="$HOME/local/juce"
fi

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -c|--clean)
            DO_CLEAN=true
            shift
            ;;
        -s|--restart)
            DO_RESTART=true
            shift
            ;;
        -h|--help)
            head -16 "$0" | tail -14
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

mkdir -p "$BUILD_DIR"

# Clean if requested
if [ "$DO_CLEAN" = true ]; then
    echo "=== Cleaning ==="
    rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles"
fi

# Configure if needed
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ] || ! grep -q "CMAKE_BUILD_TYPE:STRING=$BUILD_TYPE" "$BUILD_DIR/CMakeCache.txt"; then
    echo "=== Configuring for $BUILD_TYPE ==="
    cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DHARDWARE_INTERFACE_ENABLED=ON \
        -DCMAKE_PREFIX_PATH="$JUCE_PREFIX_PATH"
fi

# Which JUCE this build is against. Resolved from several places and defaulted
# silently, so a build that picked up the wrong one used to look exactly like a
# build that picked up the right one. Read back out of the cache once there is
# one, because that -- not this variable -- is what an existing build dir uses.
JUCE_IN_USE="$(grep -m1 "^JUCE_DIR:PATH=" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2-)"
[ -n "$JUCE_IN_USE" ] || JUCE_IN_USE="$JUCE_PREFIX_PATH"
echo "=== JUCE: $JUCE_IN_USE ==="

echo "=== Building a3-motion-ui ($BUILD_TYPE) ==="
cmake --build "$BUILD_DIR" --target a3-motion-ui_Standalone -j4

BINARY="$BUILD_DIR/src/a3-motion-ui/a3-motion-ui_artefacts/$BUILD_TYPE/Standalone/a3-motion-ui"
BINARY_DIR="$BUILD_DIR/src/a3-motion-ui/a3-motion-ui_artefacts/$BUILD_TYPE/Standalone"

# Create symlinks to resources and config if not exists
if [ ! -e "$BINARY_DIR/resources" ]; then
    echo "=== Creating resources symlink ==="
    ln -s "$SRC_DIR/resources" "$BINARY_DIR/resources"
fi
if [ ! -e "$BINARY_DIR/config" ]; then
    echo "=== Creating config symlink ==="
    ln -s "$SRC_DIR/config" "$BINARY_DIR/config"
fi

echo ""
echo "=== Build complete ==="
echo "Binary: $BINARY"
echo "Run: $BINARY"

# Restart service if requested
if [ "$DO_RESTART" = true ]; then
    echo ""
    echo "=== Restarting service ==="
    systemctl --user restart a3-motion.service
    sleep 1
    systemctl --user status a3-motion.service --no-pager
fi
