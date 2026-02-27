#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JUCE_LOCAL_DIR="$HOME/local/juce"
JUCE_SRC_DIR="$HOME/src/JUCE"
BUILD_DIR="$SCRIPT_DIR/build"

echo "=========================================="
echo "a3-motion-ui Build Script for Raspberry Pi 5"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Function to print status
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

# Step 1: Check and install system dependencies
print_status "Step 1: Checking system dependencies..."

REQUIRED_PACKAGES=(
    "build-essential"
    "cmake"
    "git"
    "xorg-dev"
    "libasound2-dev"
    "libgsl-dev"
    "libserial-dev"
    "libgpiod-dev"
)

MISSING_PACKAGES=()
for pkg in "${REQUIRED_PACKAGES[@]}"; do
    if ! dpkg -l | grep -q "^ii  $pkg"; then
        MISSING_PACKAGES+=("$pkg")
    fi
done

if [ ${#MISSING_PACKAGES[@]} -gt 0 ]; then
    print_warning "Missing packages: ${MISSING_PACKAGES[*]}"
    print_status "Installing missing packages (requires sudo)..."
    sudo apt-get update
    sudo apt-get install -y "${MISSING_PACKAGES[@]}"
else
    print_status "All system dependencies are installed."
fi

echo ""

# Step 2: Build and install JUCE if not already installed
print_status "Step 2: Checking JUCE installation..."

if [ -d "$JUCE_LOCAL_DIR/lib/cmake/JUCE" ]; then
    print_status "JUCE is already installed at $JUCE_LOCAL_DIR"
    JUCE_VERSION=$(ls "$JUCE_LOCAL_DIR/lib/cmake" | grep "^JUCE-" | head -1)
    if [ -z "$JUCE_VERSION" ]; then
        print_error "Could not determine JUCE version"
        exit 1
    fi
else
    print_status "JUCE not found. Building and installing..."
    
    if [ ! -d "$JUCE_SRC_DIR" ]; then
        print_status "Cloning JUCE repository..."
        mkdir -p "$HOME/src"
        git clone https://github.com/juce-framework/JUCE.git "$JUCE_SRC_DIR"
    fi
    
    cd "$JUCE_SRC_DIR"
    print_status "Checking out develop branch..."
    git checkout develop
    
    print_status "Building JUCE..."
    mkdir -p build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX="$JUCE_LOCAL_DIR" ..
    make -j$(nproc)
    make install
    
    cd "$SCRIPT_DIR"
    JUCE_VERSION=$(ls "$JUCE_LOCAL_DIR/lib/cmake" | grep "^JUCE-" | head -1)
fi

print_status "JUCE version: $JUCE_VERSION"

echo ""

# Step 3: Configure and build a3-motion-ui
print_status "Step 3: Building a3-motion-ui..."

export JUCE_DIR="$JUCE_LOCAL_DIR/lib/cmake/$JUCE_VERSION"

if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

print_status "Running CMake configuration..."
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DHARDWARE_INTERFACE_ENABLED=ON \
    -DHARDWARE_INTERFACE_VERSION=V2 \
    -DTESTS_ENABLED=OFF \
    ..

print_status "Building application (this may take a while)..."
make -j$(nproc)

echo ""

# Step 4: Validate build
print_status "Step 4: Validating build..."

if [ -f "$BUILD_DIR/src/a3-motion-ui/a3-motion-ui_artefacts/Release/Standalone/a3-motion-ui" ]; then
    BINARY_PATH="$BUILD_DIR/src/a3-motion-ui/a3-motion-ui_artefacts/Release/Standalone/a3-motion-ui"
    print_status "Build successful!"
    echo ""
    echo -e "${GREEN}=========================================="
    echo "Build Complete"
    echo "=========================================="
    echo "Executable: $BINARY_PATH"
    echo "=========================================="
    echo ""
    echo "To run the application:"
    echo "  $BINARY_PATH"
    echo ""
else
    print_error "Build failed: executable not found"
    exit 1
fi
