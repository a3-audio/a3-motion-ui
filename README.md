# Build and run a3-motion-ui

## Quick Start (Automated)

The easiest way to build a3-motion-ui is using the automated build script:

```bash
./build-pi5.sh
```

This script automatically:
- Installs all required system dependencies
- Builds and installs JUCE framework (if not already installed)
- Configures and builds a3-motion-ui with Release optimization
- Enables Hardware-Interface V2 support
- Validates the build

For detailed information, see [BUILD_PI5.md](BUILD_PI5.md).

## Manual Build (Advanced)

If you prefer to build manually:

### 1. Install JUCE
- Install development files: `apt-get install xorg-dev libasound2-dev libgsl-dev libserial-dev libgpiod-dev`
- Clone JUCE repo and checkout `develop` branch
  - `mkdir ~/src ; cd ~/src`
  - `git clone https://github.com/juce-framework/JUCE.git`
  - `git checkout develop`
- Create installation folder and build/install via cmake
  - `mkdir -p ~/local/juce`
  - `mkdir build ; cd build`
  - `cmake -DCMAKE_INSTALL_PREFIX=~/local/juce ..`
  - `make ; make install`

### 2. Build a3-motion-ui
- Tell cmake where to find JUCE (replace `X.Y.Z` with correct version)
  - `export JUCE_DIR=~/local/juce/lib/cmake/JUCE-X.Y.Z`
- `mkdir build ; cd build`
- Generate makefiles via cmake
  - `cmake -DCMAKE_BUILD_TYPE=Release -DHARDWARE_INTERFACE_ENABLED=ON -DHARDWARE_INTERFACE_VERSION=V2 ..`
- `make`

### 3. Run the application
```bash
./build/src/a3-motion-ui/a3-motion-ui_artefacts/Release/Standalone/a3-motion-ui
```
