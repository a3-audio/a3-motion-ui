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
- `/beat` is sent via OSC

### EXT (External Clock) – Orange
- The motion controller receives tempo/beat from an external source via OSC
- **TAP** button resets beat counter to 1 (sync point)
- Status bar shows external BPM and beat counter from OSC input
- **Pattern playback synchronizes to external clock** (BPM and beat phase)
- `/beat` is NOT sent (to avoid feedback)
- Use for synchronization with DAWs or other clock sources

## General Behavior

- In **INT mode**, `/beat` is sent via OSC to the beatclockPort
- In **EXT mode**, `/beat` is received via OSC and drives display + pattern playback

## Recording & Playback

1. Set the desired recording length using the encoder (per channel)
2. Hold **REC** + press a **Pad** to start recording
3. Move the channel blob during recording
4. Recording starts on the next downbeat
5. Press the **Pad** again to play/stop the recorded pattern

---

# Installation

## Install system packages

On a fresh Debian/Raspbian system, install the following before building anything:

- Toolchain: `build-essential cmake pkg-config git`
- JUCE dependencies: `xorg-dev libasound2-dev libfreetype6-dev libcurl4-openssl-dev libegl-dev`
  (on newer Debian releases the freetype package was renamed to `libfreetype-dev`; install
  whichever exists). `libegl-dev` is new with JUCE 9 — its OpenGL module includes `EGL/egl.h`
  unconditionally on Linux. Install it **before** configuring: if `egl.pc` is missing at configure
  time, JUCE silently drops its whole `egl;gl` package group and the build fails much later at
  link time on `glLineWidth` and friends. A rebuild does not recover from that; the build
  directory has to be configured again.
- a3-motion-engine dependency (GSL, checked via `pkg_check_modules`): `libgsl-dev`
- Hardware interface (`HARDWARE_INTERFACE_ENABLED=ON`, V2 or V3): `libserial-dev libgpiod-dev`
- Unit tests (`TESTS_ENABLED`, on by default): `googletest libgtest-dev libgmock-dev`
- On-screen keyboard, for entering names and addresses on the touchscreen:
  `onboard dbus-bin`. The UI does not draw a keyboard of its own — it asks
  Onboard to show and hide over D-Bus (`org.onboard.Onboard`), and Onboard
  types into the focused window. Without it, the keyboard icon in the status
  bar does nothing and every field is still reachable with the encoder.

```
apt-get install build-essential cmake pkg-config git \
    xorg-dev libasound2-dev libfreetype6-dev libcurl4-openssl-dev libegl-dev \
    libgsl-dev libserial-dev libgpiod-dev \
    googletest libgtest-dev libgmock-dev \
    onboard dbus-bin
```

Onboard docks at the top of the screen by default, where it would cover the
status bar — including the icon that hides it again. Move it to the bottom
once per machine:

```
python3 -c "from gi.repository import Gio; s = Gio.Settings.new('org.onboard.window'); \
    s.set_string('docking-edge','bottom'); s.set_boolean('docking-enabled', True); \
    s.set_boolean('docking-shrink-workarea', False)"
```

Onboard follows the system theme by default, which on this rig is a light beige
that fights the dark UI. `Blackboard` is the one that matches; `Nightshade` and
`DarkRoom` are the other dark ones it ships. Set it the same way:

```
python3 -c "from gi.repository import Gio; s = Gio.Settings.new('org.onboard'); \
    s.set_boolean('system-theme-tracking-enabled', False); \
    s.set_string('theme','/usr/share/onboard/themes/Blackboard.theme')"
```

(`gsettings` does the same thing if it is installed; both settings live in the
user's dconf database and survive restarts.)

## Install JUCE

This project builds against **JUCE 9.0.1**. A released tag, not `develop`: the
point of pinning is that a build here fails for reasons in this repository.

- clone JUCE repo and check out the release tag
  - `mkdir ~/src ; cd ~/src`
  - `git clone https://github.com/juce-framework/JUCE.git`
  - `git checkout 9.0.1`
- create installation folder and build/install via cmake
  - `mkdir -p ~/local/juce`
  - `mkdir build ; cd build`
  - `cmake -DCMAKE_INSTALL_PREFIX=~/local/juce ..`
  - `cmake -S . -B build -DHARDWARE_INTERFACE_ENABLED=ON -DHARDWARE_INTERFACE_VERSION=V2 -DCMAKE_BUILD_TYPE=Debug` for hardwaresupport
  - `make ; make install`

# Build and run a3-motion-ui
- tell cmake where to find JUCE (replace `X.Y.Z` with correct version)
  - `export JUCE_DIR=$HOME/local/juce/lib/cmake/JUCE-9.0.1`
  - An older JUCE may be installed side by side under its own prefix; point `JUCE_DIR` at
    whichever one you mean. Note that `build.sh` falls back to `$HOME/local/juce` when
    `JUCE_DIR` is unset, so a stale install at that path is what you get without a word —
    export it, or keep that prefix on the version this project builds against.
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
| `/beat` | `i i i` (beat, bar, bpm) | Internal clock beat (1-4), bar (1-indexed), current BPM (rounded) |
| `/tap` | `i` (1) | Sent when beat 1 is detected (first tap in INT mode, tap press in EXT mode) |
| `/clockmode` | `i` (0 or 1) | Clock mode status: 0=internal, 1=external |

## OSC Messages Received (Input on `oscReceiver.port`)

| Address | Arguments | Description |
|---------|-----------|-------------|
| `/vu/<n>` | `f f` (peak, rms) | VU meter data for channel n (1-4). Used for corona visualization around channel blobs. |
| `/beat` | `i i i` (beat, bar, bpm) | External beat clock. Beat (1-4), bar (1-indexed), BPM. Displayed in status bar (EXT mode). |

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
