<!--
SPDX-FileCopyrightText: 2023 A3 Audio UG (haftungsbeschränkt) <contact@a3-audio.com>

SPDX-License-Identifier: CC0-1.0
-->

# Install JUCE
- install development files for xorg, googletest/libgtest/libgmock, alsa if necessary (e.g. Debian/Raspbian: `apt-get install xorg-dev googletest libgtest-dev libgmock-dev libasound2-dev`)
- clone JUCE repo and checkout `develop` branch
  - `mkdir ~/src ; cd ~/src`
  - `git clone https://github.com/juce-framework/JUCE.git`
  - `git checkout develop`
- create installation folder and build/install via cmake
  - `mkdir -p ~/local/juce`
  - `mkdir build ; cd build`
  - `cmake -DCMAKE_INSTALL_PREFIX=~/local/juce ..`
  - `make ; make install`

# Build and run a3-motion-ui
- tell cmake where to find JUCE (replace `X.Y.Z` with correct version)
  - `export JUCE_DIR=/home/aaa/local/juce/lib/cmake/JUCE-X.Y.Z`
- `mkdir build ; cd build`
- generate makefiles via cmake (to develop consider passing `Debug`)
- `cmake -DCMAKE_BUILD_TYPE=Release ..`
- `make`
- `cd ..`
- run the application: ``
