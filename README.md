# A3 Motion Controller

A spatial audio motion controller for recording and playing back movement trajectories.

---

# User Guide

## Hardware Buttons

### ClockMode Button (ehemals Shift)
- **Press**: Toggle zwischen **INT** (internal clock) und **EXT** (external clock) Modus
- LED zeigt aktuellen Modus (an = EXT, aus = INT)

### Record Button (REC)
- **Halten + Pad**: Recording starten auf dem gewählten Channel/Pad

### Tap Button (TAP)
- **INT Mode**: Mehrfach tippen um internes Tempo (BPM) zu setzen. Erster Tap nach Pause setzt Beat Counter auf 1.
- **EXT Mode**: Drücken um Beat Counter auf 1 zu setzen (Sync-Punkt)
- **Halten + Pad**: Trajectory Preview des Patterns anzeigen

### Pads (pro Channel)
- **Drücken**: Play/Stop des aufgenommenen Patterns auf diesem Pad
- **REC halten + Pad**: Recording auf diesem Pad starten

## Display (Status Bar)

The top status bar shows:

| Position | INT Mode (green) | EXT Mode (orange) |
|----------|------------------|-------------------|
| **Left** | Internal BPM (from tap tempo) | External BPM (from OSC) |
| **Center** | Beat indicator (visual tick) | External beat indicator |
| **Right** | Beat counter "n/4" | External beat counter "n/4" |
| **Far Right** | "INT" | "EXT" |

## Channel Blobs (Motion Area)

Each channel is represented by a colored blob on the motion area:
- **Drag** the blob to record/control spatial position (azimuth/elevation)
- **Corona glow** around the blob shows VU meter level (received via OSC)
  - Size = RMS level
  - Brightness = Peak level

## Clock Modes

### INT (Internal Clock) – Green
- The motion controller runs its own tempo clock
- Use **TAP** button to set BPM (first tap resets beat to 1)
- Status bar shows internal BPM, beat indicator, and beat counter
- **Internal beatclock is always sent via OSC** (`/beatclock`)

### EXT (External Clock) – Orange
- The motion controller receives tempo/beat from an external source via OSC
- **TAP** button resets beat counter to 1 (sync point)
- Status bar shows external BPM and beat counter from OSC input
- Use for synchronization with DAWs or other clock sources
- **Internal beatclock is still sent via OSC** (for downstream applications)

## General Behavior

- The internal beatclock (`/beatclock`) is **always sent via OSC**, regardless of clock mode
- In **EXT mode**, the display shows incoming OSC data, but the internal clock continues to run and send

## Recording & Playback

1. Set the desired recording length using the encoder (per channel)
2. Hold **REC** + press a **Pad** to start recording
3. Move the channel blob during recording
4. Recording starts on the next downbeat
5. Press the **Pad** again to play/stop the recorded pattern

---

# Installation

## Install JUCE
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
