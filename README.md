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
- run the application: `./build/src/a3-motion-ui/a3-motion-ui_artefacts/Release/Standalone/a3-motion-ui`


# Toplavel Overview

```mermaid
graph TD
  subgraph "UI Layer"
    A[MainWindow] --> B[A3MotionEditor]
    B --> C[A3MotionUIComponent]
    C --> D[MotionComponent]
    C --> E[ChannelStrip]
    C --> F[StatusBar]
    C --> G[DirectivityComponent]
    C --> CH[ChannelHeader]
  end

  subgraph "IO Layer"
    H[InputOutputAdapter] -.-> I[InputOutputAdapterV2]
    I --> K[LibSerial::SerialPort]
  end

  subgraph "Engine Layer"
    M[MotionEngine] --> N[Channel]
    M --> O[Pattern]
    M --> P[TempoClock]
    M --> Q[AsyncCommandQueue]
    M --> ME[Measure]
    N --> POS[Position]
    O --> POS
  end

  subgraph "Backend Layer"
    Q --> R[SpatBackendA3]
    Q --> S[SpatBackendIEM]
    R --> T[OSC Messages]
    S --> T
  end

  subgraph "Test Layer"
    TA[UnitTests] --> M
    TA --> N
    TA --> O
    TA --> POS
  end

  %% Connections between layers
  C <--> H
  B --> M

  classDef ui fill:#d4f1f9,stroke:#333,stroke-width:1px;
  classDef io fill:#ffeaa7,stroke:#333,stroke-width:1px;
  classDef engine fill:#d5f5e3,stroke:#333,stroke-width:1px;
  classDef backend fill:#fadbd8,stroke:#333,stroke-width:1px;
  classDef test fill:#f9e79f,stroke:#333,stroke-width:1px;

  class A,B,C,D,E,F,G,CH ui;
  class H,I,K io;
  class M,N,O,P,Q,ME,POS engine;
  class R,S,T backend;
  class TA test;
  ```