# Building a3-motion-ui for Raspberry Pi 5

## Quick Start

```bash
./build-pi5.sh
```

This script automates the entire build process for Raspberry Pi 5.

## What the script does

1. **Install system dependencies**
   - Checks for required packages (xorg-dev, libasound2-dev, libgsl-dev, etc.)
   - Installs missing packages via apt-get (requires sudo)

2. **Build JUCE Framework** (if not already installed)
   - Clones JUCE repository (develop branch)
   - Builds and installs to `~/local/juce`

3. **Build a3-motion-ui**
   - Release build (optimized for performance)
   - Hardware-Interface V2 enabled (serial + gpiod)
   - Unit tests disabled

4. **Validation**
   - Checks for successful binary creation
   - Outputs path to executable

## Requirements

- Raspberry Pi 5 (ARM64)
- Raspbian/Debian Linux
- Internet connection (for JUCE download)
- ~2GB free disk space (JUCE + build)

## Execution

```bash
cd /home/aaa/a3-motion-ui
./build-pi5.sh
```

The script will prompt for `sudo` if needed to install system dependencies.

## Output

After a successful build, the path to the executable is displayed:

```
Executable: /home/aaa/a3-motion-ui/build/src/a3-motion-ui/a3-motion-ui_artefacts/Release/Standalone/a3-motion-ui
```

## Running the application

```bash
/home/aaa/a3-motion-ui/build/src/a3-motion-ui/a3-motion-ui_artefacts/Release/Standalone/a3-motion-ui
```

## Troubleshooting

- **Missing dependencies**: The script attempts to install these automatically
- **JUCE build fails**: Check internet connection and disk space
- **Permission errors**: Ensure `build-pi5.sh` is executable (`chmod +x build-pi5.sh`)

## Configuration

The script uses the following settings:

- **Build type**: Release (optimized)
- **Hardware-Interface**: V2 enabled (serial + gpiod)
- **Tests**: Disabled
- **Parallelization**: Uses all available CPU cores

To change these settings, edit the CMake flags in `build-pi5.sh`.

## Build Status

✅ **Successfully tested on Raspberry Pi 5**

The build script has been successfully executed and the a3-motion-ui application with Hardware-Interface V2 has been compiled.
