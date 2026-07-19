# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A3 Motion UI is a JUCE-based (C++17) standalone application that drives a spatial-audio motion
controller: recording and playing back movement trajectories for up to 4 audio channels, driven by
a dedicated hardware controller (buttons, pads, encoders, pots) and communicating with external
spatialization software via OSC.

## Build

Requires JUCE (`develop` branch) built/installed separately; requires `pkg-config`/`gsl` dev
packages, and (when hardware support is on) `libserial`/`libgpiod` dev packages, plus GoogleTest
(`libgtest-dev libgmock-dev`) for the test target.

```bash
export JUCE_DIR=/home/aaa/local/juce/lib/cmake/JUCE-X.Y.Z   # match installed JUCE version

./build.sh              # Release build (default)
./build.sh -d           # Debug build
./build.sh -c -r        # Clean + Release build
./build.sh -r -s        # Release build + restart the a3-motion.service systemd unit
```

`build.sh` configures CMake into `build/` with `-DHARDWARE_INTERFACE_ENABLED=ON` and builds only
the `a3-motion-ui_Standalone` target. It also symlinks `resources/` and `config/` into the build's
artefact directory so the binary can find them at runtime.

Manual CMake invocation (equivalent, useful for other targets like the test runner or pattern
generator):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHARDWARE_INTERFACE_ENABLED=ON -DCMAKE_PREFIX_PATH="$JUCE_DIR"
cmake --build build -j4
```

Key CMake options (see `src/a3-motion-ui/CMakeLists.txt`):
- `HARDWARE_INTERFACE_ENABLED` (bool) — compile in real hardware I/O vs. UI-only.
- `HARDWARE_INTERFACE_VERSION` — `V2` or `V3`, selects `InputOutputAdapterV2`/`V3`.
- `TESTS_ENABLED` (top-level `CMakeLists.txt`) — builds `src/a3-motion-tests`.
- `MOTION_NUM_CHANNELS` (in `a3-motion-engine`) — number of channels compiled in, default 4.

`Config.hh` is generated from `Config.hh.in` for both `a3-motion-engine` and `a3-motion-ui` at
configure time and is gitignored — don't hand-edit the generated header, edit the `.in` template.

## Run

```bash
./run.sh                # runs Debug binary by default (A3_BUILD_TYPE=Debug), falls back to Release
A3_BUILD_TYPE=Release ./run.sh
```

Runtime config lives in `config/config.json` (OSC hosts/ports, LED colours, corona/glow visual
tuning, per-channel colours). The build symlinks this directory next to the binary.

## Tests

Unit tests use GoogleTest via `src/a3-motion-tests` (built when `TESTS_ENABLED=ON`, the default).

```bash
cmake --build build --target a3-motion-tests -j4
cd build && ctest                                  # run all tests
cd build && ctest -R <TestSuiteName>                # run a single test/suite
```

Tests are registered via `gtest_discover_tests`; test sources live in
`src/a3-motion-tests/unit/` (e.g. `TempoClock.cc`, `Position.cc`). There is also a
manually-invoked comparison test, `src/a3-motion-ui/tests/TempoEstimatorTest.cc`, compiled
directly into the UI target rather than the test runner.

## Architecture

The system is split into three subprojects under `src/`, plus a small standalone pattern
generator and the test runner:

- **`a3-motion-engine`** (static lib) — headless playback/recording/timing engine. No UI or
  hardware dependencies beyond JUCE core/OSC. Depends on GSL.
- **`a3-motion-ui`** (JUCE plugin, `Standalone` format only) — the JUCE UI, application shell, and
  hardware I/O adapters. Depends on `a3-motion-engine`.
- **`a3-motion-pattern-gen`** — small CLI (`a3-pattern-gen`) that links only against the engine,
  used to generate pattern files offline.
- **`a3-motion-tests`** — GoogleTest console app linking the engine.

### Engine (`src/a3-motion-engine`)

- `MotionEngine` is the core: owns `Channel`s, a `TempoClock`, and an `AsyncCommandQueue`. It
  processes recording/playback state machines on tempo-clock ticks (`tickCallback`) and
  communicates with the rest of the engine and UI through lock-free FIFOs (`juce::AbstractFifo`) —
  commands are enqueued from the UI/message thread and drained on the high-priority clock thread,
  never called directly across threads.
- `TempoClock` (`tempo/`) is the timing engine: runs at tick resolution relative to the current
  metrum (bar/beat/tick), with pluggable `TempoEstimator` strategies (`Last`, `Mean`,
  `MeanSelective`, `IRLS`) for turning tap events into BPM.
- `Pattern` / `PatternFile` / `PatternLibrary` — trajectory data model, on-disk (de)serialization,
  and directory-backed library of available patterns (system patterns in `pattern/system`,
  user-recorded patterns in `pattern/user`; see `pattern/system/*.svg` for the built-in shape set).
- `backends/SpatBackend*` — abstract backend interface with `SpatBackendA3` and `SpatBackendIEM`
  implementations; these are what actually format and dispatch OSC motion data.
- `elevation/HeightMap*` — maps 2D recorded positions onto a 3D sphere (`Flat` vs `Sphere`
  strategies), used for elevation coverage behavior.
- `AsyncCommandQueue` — the lock-free bridge from the high-priority tempo-clock thread to the
  backend/network thread, so OSC I/O never blocks realtime scheduling.

### UI (`src/a3-motion-ui`)

`A3MotionUIComponent` (in `components/`) is the central orchestrator — it owns the main UI tree,
registers all hardware listeners, translates hardware events into `MotionEngine` calls, and
handles OSC in/out (beatclock, VU, tap). Read `team.md` (German) for a detailed, currently-accurate
description of this component's event flow, button semantics, and the clock-mode/overlay-menu
state machine — it's the best single source of truth for UI behavior and is worth consulting
before changing button/menu/clock logic.

High-level structure, top to bottom in `A3MotionUIComponent::resized()`: `StatusBar` →
`LoopLengthDisplay` → `ElevationDisplay` → per-channel `PadRowDisplay` rows → `FilterDisplay` →
`MotionComponent` filling the rest → `OverlayMenuComponent` as a fullscreen overlay on top.
`ChannelStrip` instances still exist but are currently hidden (`setVisible(false)`).

Clock mode (`_clockMode`: `0=INT, 1=EXT, 2=PIO`) governs whether the UI drives its own tempo (tap
button sets BPM, `/beat` sent via OSC) or follows an externally received `/beat` OSC stream
(playback synced to external phase, `/beat` not re-sent to avoid feedback loops).

#### Hardware I/O (`src/a3-motion-ui/io`)

`InputOutputAdapter` is the shared abstract base: it runs a background `juce::Thread` that polls
hardware (`processInput()`, implemented by subclasses), converts raw events into typed
`InputMessage`s (Pad/Button/Encoder/Pot/Tap), and hands them to the UI/message thread via a
lock-free FIFO; a `timerCallback()` dispatches them onto `juce::Value`s that `A3MotionUIComponent`
listens to via `juce::Value::Listener`. This keeps the UI protocol-agnostic.

Two concrete adapters, selected at configure time via `HARDWARE_INTERFACE_VERSION`:
- `InputOutputAdapterV2` — older, textual serial protocol (`B`/`EB`/`Enc`/`P` line prefixes).
- `InputOutputAdapterV3` — current hardware, binary packed poll-frames (`GET_BUTTONS`,
  `GET_ENCODERS`, `GET_POTS` commands per `host.py`'s protocol docstring). Response ordering
  (`BTN+ENC` vs `ENC+BTN`) can vary by USB-CDC stack, so parsing is marker-based and must accept
  both layouts. Two physical buttons ("MenuToggle left/right") are combined into a single
  chorded `Menu` press/release pair inside `dispatchButtonEvent()`.

`host.py` (repo root) is a standalone diagnostic/reference tool — not part of the CMake build — for
polling the firmware directly over serial and printing decoded input events; useful when debugging
whether hardware issues are in the firmware, the serial link, or the C++ adapter. The firmware
itself lives in a separate repository (see `team.md` §5.3 for the link).

### Known documentation-vs-code drift

`team.md` §8 explicitly notes: some in-code comments reference outdated button labels/indices, and
the overlay-menu chord comment mentions a different chord (`00+09`) than the one V3 currently uses
(`50+59`). When debugging hardware mapping, trust the actual `buttonMap` in
`InputOutputAdapterV3.cc` over comments. If you change firmware-facing indices, update both the
mapping code and `team.md`.

## Code style

Formatting is enforced via `.clang-format` (GNU base style, 2-space indent, Cpp). Files carry a
GPL-3.0-or-later header block (see `COPYING`/`.reuse/dep5` for REUSE licensing metadata) —
preserve existing file headers when editing, and add one consistent with neighboring files if
creating a new source file.
