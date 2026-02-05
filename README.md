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
  - `cmake -S . -B build -DHARDWARE_INTERFACE_ENABLED=ON -DHARDWARE_INTERFACE_VERSION=V2 -DCMAKE_BUILD_TYPE=Debug` for hardwaresupport
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

# OSC Communication Protocol

The motion controller communicates via OSC (Open Sound Control) with external applications.

## Configuration (config.json)

```json
{
  "oscSender": {
    "host": "192.168.43.57",
    "port": 9000,
    "beatclockPort": 9001
  },
  "oscReceiver": {
    "host": "0.0.0.0",
    "port": 7771
  }
}
```

## OSC Messages Sent (Output)

### Motion Data (sent to `oscSender.host:oscSender.port`)

| Address | Arguments | Description |
|---------|-----------|-------------|
| `/channel/<n>/azimuth` | `f` (float) | Azimuth angle for channel n (1-4) |
| `/channel/<n>/elevation` | `f` (float) | Elevation angle for channel n (1-4) |
| `/channel/<n>/pot_1` | `f` (float 0-1) | Pot 1 value for channel n |
| `/channel/<n>/pot_2` | `f` (float 0-1) | Pot 2 value for channel n |

### Clock Data (sent to `oscSender.host:oscSender.beatclockPort`)

| Address | Arguments | Description |
|---------|-----------|-------------|
| `/beatclock` | `i i f` (beat, bar, bpm) | Internal clock beat (1-4), bar (1-indexed), current BPM |
| `/clockmode` | `i` (0 or 1) | Clock mode status: 0=internal, 1=external |

## OSC Messages Received (Input on `oscReceiver.port`)

| Address | Arguments | Description |
|---------|-----------|-------------|
| `/vu/<n>` | `f f` (peak, rms) | VU meter data for channel n (1-4). Used for corona visualization around channel blobs. |
| `/beat/1` | `f` (bpm) | External BPM. Displayed in status bar. |
| `/beatclock/1` | `i i i i` (timestamp, bpm, beat, bar) | External beat clock. Displayed in status bar. |

## Clock Mode Behavior

- **INT (Internal)**: Motion controller runs its own clock. Tap tempo sets BPM. First tap resets beat counter to 1.
- **EXT (External)**: Motion controller receives external clock via OSC. Tap tempo is disabled. Clock reset via Tap+ClockMode combo.

Press the ClockMode button to toggle between internal and external mode. The status bar shows "INT" (green) or "EXT" (orange).

## VU Corona Visualization

The corona around each channel blob is controlled by incoming VU data:
- **Size**: Controlled by RMS level
- **Brightness**: Controlled by Peak level
- **White blend**: Added at high peak levels for "hot" visual effect

Corona parameters can be configured in `config.json` under `"corona"`:
```json
{
  "corona": {
    "vuMax": 0.24,
    "sizeMin": 0,
    "sizeMax": 2.5,
    "sizeGrabbed": 0.6,
    "alphaMin": 0.15,
    "alphaMax": 0.75,
    "whiteBlend": 0.3
  }
}
```
