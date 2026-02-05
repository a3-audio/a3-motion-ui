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

BUILD_DIR="/home/aaa/a3-motion/ui/build"
SRC_DIR="/home/aaa/a3-motion/ui"
BUILD_TYPE="Release"
DO_CLEAN=false
DO_RESTART=false

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

cd "$BUILD_DIR"

# Clean if requested
if [ "$DO_CLEAN" = true ]; then
    echo "=== Cleaning ==="
    rm -rf CMakeCache.txt CMakeFiles
fi

# Configure if needed
if [ ! -f CMakeCache.txt ] || ! grep -q "CMAKE_BUILD_TYPE:STRING=$BUILD_TYPE" CMakeCache.txt; then
    echo "=== Configuring for $BUILD_TYPE ==="
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DHARDWARE_INTERFACE_ENABLED=ON "$SRC_DIR"
fi

echo "=== Building a3-motion-ui ($BUILD_TYPE) ==="
cmake --build . --target a3-motion-ui_Standalone -j3

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

# Update wrapper script to point to correct build type
cat > /home/aaa/bin/a3-motion-ui << EOF
#!/bin/bash
cd $BUILD_DIR/src/a3-motion-ui/a3-motion-ui_artefacts/$BUILD_TYPE/Standalone/
./a3-motion-ui
EOF
chmod +x /home/aaa/bin/a3-motion-ui
echo "Updated: /home/aaa/bin/a3-motion-ui -> $BUILD_TYPE"

# Restart service if requested
if [ "$DO_RESTART" = true ]; then
    echo ""
    echo "=== Restarting service ==="
    systemctl --user restart a3-motion.service
    sleep 1
    systemctl --user status a3-motion.service --no-pager
fi
