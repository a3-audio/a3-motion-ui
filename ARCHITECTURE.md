# A3 Motion UI -- architecture

How this repository is built, and why it is built that way. It is written for whoever touches the
code next; every section that explains a decision is there because getting it wrong cost something.

Called `CLAUDE.md` until 2026-09-24. It lives here, beside the code, so that it changes in the same
commit as what it describes.

## What this is

A3 Motion UI is a JUCE-based (C++17) standalone application that drives a spatial-audio motion
controller: recording and playing back movement trajectories for up to 4 audio channels, driven by
a dedicated hardware controller (buttons, pads, encoders, pots) and communicating with external
spatialization software via OSC.

## Build

Requires **JUCE 9.0.3** (release tag, not `develop`; the one JUCE every A³ product builds against,
pinned as `JUCE_VERSION` in a3-system's `installer/roles/base.py`) built/installed separately; requires
`pkg-config`/`gsl` dev packages and `libegl-dev` (JUCE 9's OpenGL module includes `EGL/egl.h` on
Linux — and if `egl.pc` is absent at configure time JUCE drops its `egl;gl` group silently, so the
build directory must be configured again after installing it, not merely rebuilt), and (when hardware support is on) `libserial`/`libgpiod` dev packages, plus GoogleTest
(`libgtest-dev libgmock-dev`) for the test target.

At **runtime** the UI also expects `onboard` and `dbus-send` for text entry on the touchscreen —
see "On-screen keyboard" below. Neither is needed to build, and without them every field is still
reachable with the encoder.

`build.sh` finds JUCE at `~/local/juce` on its own and prints which version it
picked before it builds — nothing needs exporting. `JUCE_DIR` still overrides
it, for building against a JUCE somewhere else.

```bash
./build.sh              # Release build (default)
./build.sh -d           # Debug build
./build.sh -c -r        # Clean + Release build
./build.sh -r -s        # Release build + restart the a3-motion.service systemd unit
./build.sh -t           # ... and build the test runner too (see Tests below)
```

`build.sh` builds the app and **not** the tests unless given `-t`: a full test build takes minutes
where an incremental app build takes about one, and the quick way to the device has to stay quick.
That trade is the reason `test.sh` exists — see Tests.

`build.sh` configures CMake into `build/` with `-DHARDWARE_INTERFACE_ENABLED=ON` and builds only
the `a3-motion-ui_Standalone` target. It also symlinks `resources/` and `config/` into the build's
artefact directory. The `resources/` link is how a dev build finds its pictures
(`resourceDirectory`, see "Packaged: what lives where"); the `config/` link is
unused at runtime, since `config/` and `pattern/` are read from the working
directory.

Manual CMake invocation (equivalent, useful for other targets like the test runner or pattern
generator):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHARDWARE_INTERFACE_ENABLED=ON \
      -DCMAKE_PREFIX_PATH="$HOME/local/juce"
cmake --build build -j4
```

Key CMake options (see `src/a3-motion-ui/CMakeLists.txt`):
- `HARDWARE_INTERFACE_ENABLED` (bool) — compile in real hardware I/O vs. UI-only.
- `HARDWARE_INTERFACE_VERSION` — `V2` or `V3`, selects `InputOutputAdapterV2`/`V3`.
- `TESTS_ENABLED` (top-level `CMakeLists.txt`) — builds `src/a3-motion-tests`.
- `MOTION_NUM_CHANNELS` (in `a3-motion-engine`) — number of channels compiled in, default 4.

`Config.hh` is generated from `Config.hh.in` for both `a3-motion-engine` and `a3-motion-ui` at
configure time and is gitignored — don't hand-edit the generated header, edit the `.in` template.

**A failed *configure* leaves the build tree compiling nothing, and says "Built target" while it
does it.** On 2026-09-23 a source was listed in `src/a3-motion-engine/CMakeLists.txt` before the
file existed — a deliberate red step, TDD. CMake aborted during configure; from then on
`liba3-motion-engine.a` held **only the JUCE modules**, not one project object, and every build
reported success. The failure surfaced two steps later as the whole engine being undefined at link
(`a3::TempoClock::start`, `a3::MotionEngine::~MotionEngine`, …), which reads like a linker problem
and is not one.

The fix is to **configure again**, not to build again:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHARDWARE_INTERFACE_ENABLED=ON \
      -DCMAKE_PREFIX_PATH="$HOME/local/juce"
```

Same shape as the `egl.pc` trap above, and worth the same reflex: **any error during configure means
reconfigure before trusting another build.** The tell is a target that links nothing of its own —
`ar t build/.../liba3-motion-engine.a` listing only `juce_*.o` says it in one line.

## Run

```bash
./run.sh                # runs Debug binary by default (A3_BUILD_TYPE=Debug), falls back to Release
A3_BUILD_TYPE=Release ./run.sh
```

Runtime config lives in `config/config.json` (OSC hosts/ports, LED colours, corona/glow visual
tuning, per-channel colours). It is read from the working directory; `run.sh` does not
change it, so run from a checkout it is the checkout's `config/` and `pattern/`.

## Packaged: what lives where

The `a3-motion-ui` package (`packaging/stage` lays out its tree) puts the program and
what never changes under `/usr`, and keeps everything the performer edits in their home:

| What | Where |
|---|---|
| binary | `/usr/bin/a3-motion-ui` |
| helpers | `/usr/lib/a3-motion-ui/` (`a3-motion-ui-seed`, `a3-wait-for-the-screen`) |
| unit | `/usr/lib/systemd/user/a3-motion.service` |
| shipped defaults | `/usr/share/a3-motion-ui/{resources,config,pattern}` |
| working directory (the performer's) | `~/.local/share/a3-motion` (`config/`, `pattern/`, `.shipped.json`) |
| log | `~/.local/state/a3-motion/a3-motion-ui.log` (`$XDG_STATE_HOME` if absolute) |

**Resources** are not seeded: they are code. `resourceDirectory` (`AppPaths.hh`) takes
`resources/` beside the executable if it exists (a build with `build.sh`'s link), else
`../share/a3-motion-ui/resources` from it (the package). A missing folder is logged once.

**The seed** (`a3-motion-ui-seed`, the unit's `ExecStartPre`, as the user, never fatal)
copies `config/` and `pattern/` from the shipped defaults into the working directory
and never deletes. Per shipped file: a link or non-plain file is left alone (1); one
that is missing is added unless the manifest says it was removed (2); one equal to the
shipped version needs nothing (3); one still as the last package shipped it takes the
new version (4); on the first seed after a migration every existing file counts as the performer's (5);
one changed locally that the package did not change stays as it is (6); one changed locally
while the package did gets the new version beside it as `<name>.shipped` (7).
`.shipped.json` remembers `path -> sha256` of what was shipped. With no working
directory yet, the seed copies an old checkout's `config/` and `pattern/` (the
`a3-system/a3-motion/ui` layout, or `--from DIR`) instead of starting from the defaults.

**`.shipped` files are invisible to the app** (`ShippedCopies` tests): not a skin, not a
library entry, not moved into a user half. `a3-motion-ui-seed --report
~/.local/share/a3-motion` lists what differs from the shipped defaults and what waits.

**Factory work moves.** A rig edit does not land in the checkout any more. To promote it
into the shipped library, copy the file from `~/.local/share/a3-motion` into the
checkout's `config/` or `pattern/` and commit.

**Dev route.** A dev build reaches the rig only through an explicit drop-in,
`~/.config/systemd/user/a3-motion.service.d/zz-dev.conf`:

```ini
[Service]
ExecStartPre=/bin/echo "a3-motion: DEV BUILD from <path> -- remove zz-dev.conf to go back to the package"
ExecStart=
ExecStart=<path>/build/src/a3-motion-ui/a3-motion-ui_artefacts/Release/Standalone/a3-motion-ui
```

It keeps the packaged working directory, so the dev build plays the live set. To go
back: `rm` the file, `systemctl --user daemon-reload`, restart. `build.sh -s` restarts
the *unit*; without `zz-dev.conf` that is the package.

## Tests

Unit tests use GoogleTest via `src/a3-motion-tests` (built when `TESTS_ENABLED=ON`, the default).

```bash
./test.sh                             # build the tests, then run them all
./test.sh -- -R <TestSuiteName>       # ... only that suite
./test.sh -d                          # the Debug build
```

**The library as committed:** `test.sh` exports HEAD's `config/` and `pattern/` into
`build/committed/` before every run, and the tests read the shipped material from there — not from
the working copy, which ACTION and the skin panel write into while a set is played. A bare `ctest`
without that export fails `ShippedLibrary.TheTestsReadTheCommittedState` and says why.

**The tests send nothing** (#67). An engine built without a backend aims at Core as the truth
names it — on the rig, the live one — and the suite used to send positions and 3D/FREQ/Q there.
Two walls now: every `juce::OSCSender` is aimed through `connectOscSender()`
(`a3-motion-engine/OscSendGuard.hh`), which the test runner switches to throw for every host
before any test runs, so an engine needs an `OfflineBackend` (`src/a3-motion-tests/OfflineBackend.hh`)
or its test fails; `TestsSendNothing` holds that and scans the sources for a sender connected past
the guard. And without an `A3_OSC_TRUTH` of yours, `test.sh` runs against
`build/a3-osc-offline.json`, a copy of the installed truth with every host but `local`/`any` moved
to 192.0.2.x; the `OSC truth:` line says which file the run used.

**One build at a time:** `build.sh` (and so `test.sh`) takes a machine-wide lock
(`$XDG_RUNTIME_DIR/a3-motion-ui-build.lock`); a second build in another worktree waits and says
for whom. Two `-j4` builds on four cores take as long as both in a row.

**Use `test.sh`, not `ctest` on its own.** `ctest` *runs* a binary; it does not *build* one, and
`build.sh` builds only the app unless given `-t`. So `./build.sh && ctest` runs whatever test
runner was built last — on 2026-09-13 that was the evening before, and two runs were reported as
green having tested nothing of that morning's work. Demonstrated the other way round on
2026-09-18: with a planted test deleted from the source, `./build.sh && ctest` still ran it and
still failed on it. A red result announces itself; a green one that tested the wrong code never
does.

`test.sh` therefore does the two steps in the order that makes the answer true, and prints **when
the runner it ran was built**. Quote that timestamp when reporting a result — it is the difference
between "the tests are green" and "the tests are green for the code that is actually here".

The two steps by hand, if you need them:

```bash
cmake --build build --target a3-motion-tests -j4
cd build && ctest
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

### Motion is pure OSC

**This app renders no audio and opens no audio device** (decided 2026-10-06). It sends positions
and control values to A³ Core over OSC; Core does the audio processing, StemDeck plays. The audio
engine that Plan 1 (2026-09-21) had put here — `src/a3-audio-engine/` (`ControlSurface`,
`SpatBackendInternal`, `OutputOrder`, `SpeakerTest`, `ChunkedRender`), the `A3_AUDIO_ENGINE_ENABLED`
switch, the app's own `AudioDeviceManager`/`AudioProcessorPlayer` and its JACK switch — moved to
its own repository, [a3-audio/a3-engine](https://github.com/a3-audio/a3-engine), where it is parked and not built.

`A3MotionAudioProcessor` stays only because the app is a JUCE plugin in the `Standalone` format,
and that format needs a processor; it does nothing. For the same reason `juce_audio_utils` and
`juce_audio_devices` stay linked: `juce_audio_plugin_client_Standalone.cpp` stops with `#error`
without them. What keeps the rule is that **no device type is compiled in**: `JUCE_ALSA=0` and
`JUCE_JACK=0` on the app target. `MotionIsPureOsc` in the test runner holds it — it fails if either
flag flips, if the old switch or `a3-audio-engine` comes back, or if a source under
`src/a3-motion-ui`/`src/a3-motion-engine` names `AudioDeviceManager`, `AudioProcessorPlayer` or
`AudioIODevice`. Dropping the two modules would mean building the app with `juce_add_gui_app`
instead of as a plugin.

#### FPV phase 2: gravity flight

> „was wir unbedingt vermeiden müssen ist, dass die raumschiffe nicht die ganze zeit auf ihrer bahn
> rumfliegen, das klingt langweilig. [...] wir brauchen eine große flugbahn und wie planeten die
> usergruppen" (maintainer, 2026-10-08)

**In FPV a channel is CLIP or ORBIT.** CLIP flies the clip as FULL does. ORBIT flies one large
ellipse, one lap per four bars, and guest groups the DJ places on the floor bend it. The mode is
engine state (`MotionEngine::setFlightMode`), not view state: an ORBIT ship keeps flying in FULL.
Groups live for the session only. Plan: `.claude/notes/fpv-phase-2-plan.md` in the workspace.

| Part | What it does |
|---|---|
| bodies | Up to 8 points on the floor with a mass: G +0.5, C +1, H +1.5, X (dead zone) -2 (`FlightTuning`). Drawn relative to a group, so the sizes did not change with the masses |
| big path | `BaseOrbit`: an ellipse that precesses once per 32 bars; a "rabbit" runs on it, locked to the bar |
| ship | `a = steer + gravity + separation + wander - damping*v`, stepped one tick at a time |
| beat pulse | The gate (default, `gravityOnlyOnTheOne`): gravity pulls only during beat 1 of each bar, so every bend lands on the one. Off, it is `1 + depth*(1 - beatFraction)^2` on every beat. The floor draws `drawnPulse`: the discs swell on the one and rest at their size otherwise |
| escort | The goal becomes a circle round one body; its own pull is left out so the ship holds the circle |
| dead zone | A soft wall at `deadZoneClearance`, because capped repulsion lost against the steering |
| breath | On by default (`flightBreathAtStart`). When on, every ship stands still through the last beat of each bar, velocity kept, while its rabbit runs on; it restarts on the one (`Breath.hh`, `MotionEngine::setFlightBreath`). The switch guarantees no partial stop, it is not quantised to the downbeat: made inside a stop it waits for the one, so no stop is cut short or started late; made outside one it acts at once, so switched on in beats 1-3 this bar's beat 4 already holds. Under three beats a bar there is no breath |

Time is in beats, so tempo needs no code: a lap is four bars at any BPM. The plane is the clip's
floor (x, y, rim at radius 1), mapped through the clip's own elevation band, so an ORBIT ship stays
in its band.

**Groups stand on the dance floor; the ships fly on the sphere.** A group's floor point maps onto
the sphere as a ship's would, and the group is drawn straight below that point, on the plane the
shader draws the dance floor on (`speakerFloorZ`), so a ship circling it is seen right above it.
Straight from above it lands where its sphere point is. Its blob, label, swell, ring, the escort
line and a game's dashed line (both now run from the ship down to the group) and the hit test all
use that floor point; the ORBIT guide stays on the sphere, because the ships fly there
(`FloorSurface`). A finger is a ray down the orthographic view: where it meets the floor plane is
lifted straight back onto the upper half of the sphere and through `mapTo2D`, the exact inverse.
A ray that never meets the floor (seen from the horizon, or the floor lies behind the eye, which
the shader does not draw either) is not floor, so the camera has it.

**Ships and groups are things in the room, raytraced by the sphere shader** (2026-10-09). They
were flat 2D arrows and discs painted over the finished GL picture, which looked bare and could
not go behind anything. Now `buildFlightScene` (GL thread, every frame, before the sphere pass)
turns the field into a `FlightScene` (`components/fpv/FlightScene`), packed into fixed uniform
arrays (`packFlightScene`: 4 ships, 8 groups, four floats an entry, no allocation) the way the
blobs are; FULL hands the shader an empty scene, and the shader skips the whole path on one
compare, so FULL's picture is what it was.

| | Shape | Size |
|---|---|---|
| group G / C / H | an upright spheroid, feet on the dance floor (`speakerFloorZ`), lit and soft-edged, darker at the feet, with a little noise in it | one person tall, 1.75 m; wide by weight, 1.4 / 2.0 / 2.6 m (about 3 / 6 / 10 people), straight between the masses (`groupBlobSize`); metres through `metrePerSphereRadius` (the ball's radius is about 8.5 m) |
| dead zone X | stays the flat red hatched mark on the floor, in the 2D pass: no people | as before |
| ship | a long hull ellipsoid with a flat pair of wings set back: an arrowhead, with a hot engine at the tail, in the channel's colour | `shipLengthOfBlob` blob diameters long, as the arrow was; hovering `shipHoverOfLength` over the ball so it never sinks into it |

Every shape is an ellipsoid intersected exactly (a quadratic, not a marched distance field), and
`ellipsoidEdge` also gives the pixel's distance from the outline on the screen, which is what the
antialiased edge and the ghost's line are drawn from. The beat swell (`drawnPulse`) widens a group
by `bodyPulseScale` and lifts it by half that (`swollen`).

A ship points along its course **in the room** (`ShipCourse`, from its last step long enough to
read), not on the glass: the 2D arrow needed a screen heading, a craft in the room needs the
room's, and so it turns with the camera like everything else. `shipInScene` takes the course
along the ball and the ball's normal as the ship's up.

**Depth decides what is seen** (`sceneHidden` in the shader, `hiddenByTheBall` on the CPU). A ship
or group is hidden where the ball lies between it and the eye (the point is beyond the ball's far
side along its ray), where a tower stands in front of it (the tower's own depth), or where another
ship or group is nearer. Hidden, it leaves only a **ghost**: a thin dimmed line round its outline
in its colour and a faint breath of its body, so its place is never lost. Inside the ball nothing
is hidden by it: the glass is the sound field the guests stand in, and most groups stand under it.
A ship on the far half of the ball (seen farther than the ball's middle plane) is also drawn
smaller and darker (`shipDepthCue`, eased over `shipBackSideBand`), and on the far half it is
behind the ball, so it is that ghost.

The 2D pass keeps the words: a group's `G1`..`G8` stands on top of its blob (`groupLabelAt`,
`bodyLabelBox`), a game's word under the drawn ship, the escort and game lines start at the drawn
craft (`drawnShipPixel`), and a hidden one's label is dimmed like its ghost (`labelAlpha`; the
labels know the ball, not the towers). The touch stays on the 2D floor point; a group's hit
circle is its blob's footprint as the camera sees it (`footprintRadiusOnView`, the mean of the
circle's two axes), never less than a fingertip (`groupHitRadius`); a dead zone is hit on its mark.

**Cost.** A pixel outside the box all ships and groups lie in (`uFlightBounds`) pays four
compares. Inside it, each of the twelve entries is a circle test (a subtraction and a dot) twice,
and only a pixel inside a thing's own circle pays its intersections: two ellipsoids for a ship,
one for a group, each done twice (once to find the nearest, once to shade), plus one value noise
for a group. Those circles are a few percent of the picture. Measured on llvmpipe only; on the
rig's iGPU it is expected to stay small beside the net and the beams, and is to be checked there.

**Groups reach to the speakers; the ships stay on the disc.** A group may stand anywhere out to
`floorReach` (1.35 sphere radii, `SpeakerLightScaling.hh`), between the four towers: `FloorBodies`
holds a placed or dragged group there, `onTheFloor` takes a finger up to there and gives the camera
anything beyond. Past the rim (floor radius 1) there is no sphere point to stand below, so
`DanceFloor` walks a group straight out from where the rim stands (with the default elevation
about 0.71 out, well inside the ball's outline) to `floorReach`, where its floor point is its room
point again, so a group at the edge stands just inside the towers seen from above, with no jump
at the rim. (The shader scales the same constant by `speakerRadius`, so the drawn floor runs on
past the towers; the groups stop at them.) The inverse undoes the walk and goes on past
`floorReach`, so the caller can tell an edge from the background. The ships keep to radius 1: `stepShip`'s hard wall holds
them there whatever pulls outwards (gravity, an escort circle that lies outside the disc, a game
target). An escort of a group past the rim slides to and fro along the rim in front of it.

**Switching never jumps.** CLIP to ORBIT starts at the channel's position with the path's tangent
as velocity and the rabbit at the nearest phase on the ellipse. ORBIT to CLIP glides over one
beat (`handover`). The glide is skipped while the channel is stopped or held: it cuts on Play. The
clip's playhead runs on during ORBIT, so CLIP returns in phase.

**The send path is unchanged.** `performFlight()` runs after `performPlayback()` and writes
through `Channel::setPosition`. `PositionPacer` still limits to 60 positions a second, and the
newest wins. No new OSC address.

**Clock-thread rules.**

| Rule | How |
|---|---|
| bodies reach the clock thread without a lock | `FlightBodiesBox`, a SeqLock snapshot; the message thread is the only writer; a torn read is refused, never spun |
| mode and target | `std::atomic<int>` per channel, read once per tick |
| no allocation, no lock, no log | fixed `std::array`s only |
| ship state | owned by the clock thread |

**Controls in FPV.** The three knobs stay sound, as in phase 1.

| Input | Does |
|---|---|
| tap empty floor | new group G; a ninth is refused with `-- 8 GROUPS` |
| tap a group | weight G, C, H, X, G |
| drag from a group | moves it; the camera does not turn |
| hold a group (600 ms) | removes it; its escorts go back to PATROL |
| Page, short press | that channel CLIP <-> ORBIT (on release) |
| Page held + tap a group | that ship escorts it (switches to ORBIT) |
| Page held + tap empty floor | that ship back to PATROL |
| drag outside any group, pinch | camera, as phase 1 |
| double tap | resets the view only off the floor (past `floorReach`) |
| BREATH (status bar, both views) | the breath on or off for the session, readout `-- BREATH ON`/`OFF`. In FULL too, since the ships keep breathing there. The beat display gives way to the tempo, down to four row heights, so `BPM 000.0` reads whole (`statusLabelWidth`) |
| Play, action pads | as phase 1 |

`FpvFloor` decides who gets a finger (`fpvFingerDown`) and what a Page release means
(`FpvPageHold`, `fpvPagePress`). Group ids are the lowest free 0..7, so G-labels stay stable while
a group lives and are reused after its removal.

**The breath and light planets are the defaults.** The MJ lab (2026-10-08) found that a one-beat
stop on beat 4 of every bar, not gravity, is what keeps the ships from sounding like they only
circle, and that with it the planets cost attention. After the A/B on the rig the maintainer chose
the lab's set (2026-10-08): the breath on at start-up (the key switches it off for the session,
never saved), planets at half the plan's masses (0.5/1/1.5), gravity gated to beat 1. The planets
say *where* the sound goes; the breath keeps it alive. The lab's fourth point, two ships moving
rather than four, is a way to play (playbook rule 13), not an engine rule: nothing stops four.

**Where it lives.**

| Code | Files |
|---|---|
| physics, pure | `src/a3-motion-engine/flight/` |
| groups, gestures, drawing | `src/a3-motion-ui/components/fpv/` (`FloorBodies`, `FloorGesture`, `FpvFloor`, `BodyLook`, `DanceFloor`, `FlightScene`) and `SphereShader` (`flightScene`) |
| app wiring | `MotionComponent` (touch, `drawFlight`), `A3MotionUIComponent` (`publishFloor`, Page) |

**Tune in one place: `FlightTuning` (`flight/FlightTuning.hh`).** Every constant is named there
and tagged as lab value, research value or guess. The rig changes values there; no test changes.
History: the steer and gravity caps are 1.1, not the lab's 0.6, because with the plan's masses
and an always-on pull 0.6 slung only ~1.4x, under the 1.5x a speed-up needs to be heard. That bar
is retired (below); the caps stayed. **Quiet planets, by decision (2026-10-08):** the planets say
*where*, the breath carries the motion, so a planet's bend is small and need not be heard on its
own. A crowd beside the path moves the ship 0.054-0.167 floor units with the breath off and
0.058-0.124 with it on, a hotspot 0.085-0.210 off and 0.075-0.174 on (the ear's bar for a bend
heard alone is 0.30). `FlightGravity` pins that a planet still moves the ship, with and without
the breath (bars between a halved pull and the real one), towards it, only on the one, and that a
lap with a group differs from one without.

**Tests** (`src/a3-motion-tests/unit/`): `FlightField`, `BeatPulse`, `BaseOrbit`, `ShipDynamics`,
`FlightGravity`, `FlightWorld`, `Handover`, `FlightBodiesBox`, `FlightEngine`, `FloorBodies`,
`FloorGesture`, `FpvFloor`, `FpvPagePress`, `BodyLook`, `FpvStripsPaint`, `GroupsOnTheDanceFloor`,
`FlightScene`, `FlightSceneRender` (the sphere shader compiled and run offscreen through EGL, its
pixels read back; skipped where there is no GL). They are deterministic:
one tick per step, seeds through `spreadSeed`, no wall clock. They assert behaviours (bends
towards, slings out, stays out), except one determinism test. Engines in tests take
`offlineBackend ()`, so nothing is sent.

**Open.** FULL's pad readout does not yet say `CH n ORBIT`. Whether bodies need a floor shadow is
judged on the device. Checklist: `smoke-test/fpv-phase-2.md` in the workspace.

#### FPV: actions drive pilots

> One action library for FULL and FPV: the same script turns a clip, flies an orbit, and asks a
> pilot for a game.

**An action fired at an ORBIT ship moves the flight.** Only the keys its script assigns
(`ActionScriptResult::assigned`) fly; a clip's own spin or lean never steers an orbit, so a ship
with no action flies exactly as the gravity flight above. The flight meaning follows the ship's
*mode*, in either view, because ORBIT is engine state and the sound must not change with a screen.
CLIP ships keep the action's clip meaning; 3D/FREQ/Q accents are unchanged.

| Key | On a flying ship | Where |
|---|---|---|
| `~spin` | bars per lap off the TempoLfo table, sign the clip's sense (`spinPosition`), 0 stands the orbit | carried: the ship rides its ellipse along with the rabbit (`BaseOrbit::carryAlongOrbit`), not steered, so steering and gravity stay as tuned |
| `~speedLog2` | the lap × 2^−speedLog2 | carried |
| `~swell` | the base ellipse breathes out (long axis to 0.95) or in (to 0.4) on `lfoTravel` | the rabbit (`rabbitAt`'s `radiusScale`) |
| `~sway` | the heard height swings ±30°, positive down first | `ShipHearing::heardShip` |
| `~tilt`, `~roll`, `~tswp`, `~rswp` | the heard plane leant and turned, as a clip's `SpaceTurn` | `heardShip` |
| squeezes, `~rotate`, elevation keys | nothing: a flying ship maps through its clip's own band (`flightBand`, the accent's restore point) | |

- **Lifetime:** as long as the action's accent runs, eased in and out over `motionRampBeats` (one
  beat). Taken at the press with the clip settings, in the same `SetChannelAction` message, so the
  latest press wins for both.
- **Caps** (`FlightTuning`): every rate under 180°/beat round the listener (one lap per two
  beats, about 360°/s at 120 BPM; direction stops being heard as a path near 900°/s, Feron 2010),
  `~sway`'s height under 90°/beat (tilt and roll sweeps and swell are held only by the turn limit below); and while an action drives a ship the heard direction may turn at most
  180°/beat (`limitTurn`), which holds even for every key at once. A FREQ-dependent cap is not
  built: what FREQ means in Hz is not measured yet.
- **The breath holds the floor:** the carry pauses in the stop; sway and lean keep turning.
  **An escort ignores the floor keys**; its circle is its order.

**The Pilot section** (`// ---- Pilot ----`: `~game`, `~target`, `~with`; `PilotOrder`) is read
with the script and kept with the press (`FiredAction`). In FPV a named game is posted to the
channel's ship (`MotionEngine::requestGame`, message thread only) and waits on the clock thread's
`PilotDesk`, newest wins, `\none` calls one off; the readout says `CHn An GAME X`. FULL ignores
the section (`pilotOrderAtPress`), because the games are played on the floor FPV shows. The games
take a request on the next clock tick (see the next section). Every shipped script carries the section commented out;
`pattern/actions/system/README.scd` is its manual.

**One route:** the bar's ACT, the pads and a chain all go through `sendFiredAction`; nothing else
calls `setChannelAction` (asserted from the source by `ActionReach`).

Tests: `PilotOrder`, `ActionScript`, `FlightMotion`, `FiredAction`, `FlightMotionWorld`,
`BaseOrbit`, `ShipHearing`, `PilotDesk`, `FlightEngine`, `ClipSettings`, `ActionReach`.

#### FPV: games and the pilots' level

> Pilots that play: four games above the gravity flight, timed by the music, and a key that says
> how much the pilots do of their own accord.

**A game never moves a ship.** `PilotGames` (`flight/`) is a board on the clock thread: per ship
and tick it names a point (`steerOf`), and `FlightWorld` flies it as `FlightGoal::Steer`: no
rabbit, no carry by an action's floor keys, under the same steering, gravity and walls. A ship in
a game is spared its target group's pull (as an escort is; `ShipOrders::bodyId`) and flies through
the breath's stop. `GameFigures` sets a game up once (`GamePlan`: crew, parts, target, its 1) and
works its goal out at every tick; `GameMoments` says when a game fits and where its 1 lands. All
pure and seeded, counted in beats of the running meter; constants in `GameTuning`. The plan is
fixed when the game starts: the formation's axis (the direction it faces, the line call & response
lies on) is taken from the target's place then, or the leader's, and is not followed afterwards.

| Game | Crew | Fits | Figure |
|---|---|---|---|
| fake-out | `~with` | build, drop ≤ 8 bars away, ≥ 2 bars left | glides to a stand-off short of the target, veers 60° away from the side it came in on a bar before the 1, dives at the target from 2 beats before |
| formation & scatter | `~with` (`\all` in the shipped scripts) | build, ≥ 1 bar left | a line across the target's direction at 0.4 from the middle, bursting on the 1 along rays from a focus behind the line to 0.85, every ship ≥ 45° from its place |
| hide & seek | `~with` | breakdown, ≥ 3 bars left or no known end | slips 75° round its way over 2 bars, runs across from 3 beats before the 1 |
| call & response | always a pair | groove, the first 4 of every 16 bars | the two sides of the room, a 45° call a bar each, two each |

- **A crew flies one rigid figure.** A fake-out crew shares the leader's figure in lanes 22°
  apart round the middle, widened near the middle so neighbours stay ≥ 0.08 apart (outer lanes
  therefore strike up to ±52° off a group standing near the middle). The lanes are dealt by the
  shortest glides, so no ship crosses another to reach its place, and each glides to its
  stand-off at a paced approach (waiting first when the approach is long) rather than at once.
  The formation glides to its places in the same way.
- **Timing:** a `MusicCue` (section, next, the bar it changes on, energy) made on the message
  thread: `MusicPreview` while fresh, else `LiveMood` from the channel meters (`BarMeter`: the
  loudest channel's mean per bar; drop ≥ 1.5×, breakdown ≤ 0.5×, build three rising bars; changes
  expected on the next 8-bar line). It is sent to the engine on every downbeat and preview
  (`setMusicCue`). A game's 1 is the next fitting change at least its run-up away, else the next
  4-bar line.
- **Requests** from `PilotDesk` are taken on the next tick; one whose leader cannot fly is
  dropped. A DJ's game takes its leader out of any game it was in and recruits free ships whose
  clip runs, ORBIT or CLIP; `~game = \none` ends the game its ship plays in.
- **Never two games on one ship.** A game never writes the DJ's mode or escort: a borrowed CLIP
  ship flies (`readFlightModes`) and glides back to its clip when the game ends; a ship whose
  mode or escort the DJ changes (`ShipResume` differs) leaves at once.
- **The 180°/beat heard-turn limit** holds every ship in a game, as it holds a driven one. The
  glide of a borrowed CLIP ship back to its clip is outside it (one eased beat).
- **Levels** (`PilotLevel`, the status bar's key left of CLEAN, saved as `pilotLevel` in the app
  settings): OFF, games only from actions; HINT, the lowest action button whose `~game` fits the
  current moment pulses in the notice colour on every beat (`hintedButton`, `padHintColour`),
  while the channel's ship flies ORBIT and plays no game; FLY, on a downbeat a moment opens, a
  pilot starts one game on free ORBIT ships (dice) and rests 4 bars after. The hints go by the
  current bar and are refreshed whenever a fire, a clip, a script or a game changes, on every
  step, on each music-cue push, and when the level or the view changes. A DJ's tap at
  FLY that fires a game, or fires anything at a ship in a pilot's game, drops the level to HINT
  (`levelAfterTap`), which ends the pilots' games; FLY again is one press. Leaving FPV calls every
  game and request off (`callOffGames`) and keeps the engine's level OFF until FPV is back.
- **Threads:** `requestGame`, `setMusicCue` and `callOffGames` share the engine's single-producer
  queue and are called from the message thread only (`PilotWiring.OnlyTheComponentQueuesForTheGames`);
  `PilotGames`, `PilotDesk` and the cue belong to the clock thread; `gameOf`, `pendingGame` and
  the level are atomics.

Tests: `MusicCue`, `GameMoments`, `LiveMood`, `BarMeter`, `FlightSteer`, `GameFigures`,
`PilotLevel`, `PilotGames`, `PilotGamesFlight`, `FlightEngine`, `PilotKey`, `StatusBarPilotKey`,
`StatusBarLayout`, `SettingsPersistence`, `PilotWiring`, `PilotHint`.

### Engine (`src/a3-motion-engine`)

- `MotionEngine` is the core: owns `Channel`s, a `TempoClock`, and an `AsyncCommandQueue`. It
  processes recording/playback state machines on tempo-clock ticks (`tickCallback`) and
  communicates with the rest of the engine and UI through lock-free FIFOs (`juce::AbstractFifo`) —
  commands are enqueued from the UI/message thread and drained on the high-priority clock thread,
  never called directly across threads.
- `flight/` holds the pure gravity-flight physics for FPV's ORBIT mode (bodies, base orbit, ship
  step, beat pulse, handover). `MotionEngine::performFlight()` runs it on the clock thread. See
  "FPV phase 2: gravity flight".
- `TempoClock` (`tempo/`) is the timing engine: runs at tick resolution relative to the current
  metrum (bar/beat/tick), with pluggable `TempoEstimator` strategies (`Last`, `Mean`,
  `MeanSelective`, `IRLS`) for turning tap events into BPM.
- `Pattern` / `PatternFile` / `PatternLibrary` — trajectory data model, on-disk (de)serialization,
  and directory-backed library of available patterns (system patterns in `pattern/system`,
  user-recorded patterns in `pattern/user`; see `pattern/system/*.svg` for the built-in shape set).

  **What is saved where follows one line: is this a property of the take, or of the arrangement?**
  A take's own file carries everything that makes it that take — the movement, the elevation it is
  mapped through, its speed, direction and end action, its spin, swell and accent. None of that
  changes when the take is used in another set, and a take handed to somebody without its elevation
  is a different sound. The arrangement — which take sits in which slot, the length the next take
  into that slot gets, where the channels' 3d/freq/Q are parked — is `SetFile` (`a3-motion-ui`),
  a `set.json` beside the takes. A folder with a set and the takes it names is a gig on a stick,
  which is the whole reason it is a file of its own rather than something in the app's settings.

  **A take is recorded into its clip's settings, but over the whole sphere** (decided 2026-10-07,
  #66; band changed 2026-10-08). It starts with every setting of the clip the slot held and, by the
  rec mode, its path: TOUCH overdubs (an untouched tick keeps the clip's), WRITE replaces the whole
  pass. The exceptions are 3D, FREQ and Q, which belong to the actions: no take records them and no
  lane carries them (`TakeRecording`) -- and the elevation band. A clip's band grows south from its
  base, so with the base at ear height only the lower half was playable, and with the camera
  looking from above every finger was held on the equator. So a take's band is the whole sphere:
  base at the north pole, reach 1, no clips, no sway or swell, no elevation lanes
  (`openToTheWholeSphere`, `TakeSeed`; the knobs are `isTakeBandKnob`). What is heard during the
  take is what it plays back, and the bar's elevation knobs show the whole sphere from the moment
  the take is set up -- ▶ after REC PAUSE, or the panel's REC + Play|Pause -- not at REC PAUSE.

  - **Laid out when it is asked for.** `MotionEngine::recordPattern` prepares the take on the
    caller's thread before scheduling it (`prepareTake`): its ticks, its lanes, and the clip's path
    moved into the whole sphere where it was heard -- tick by tick through the clip's own band,
    lanes and sweeps at each tick's first-pass phase, in each tick's own play direction. Untouched
    parts play where they did. The move costs a 64-bar take some 43 ms, and that lands on the
    message thread when the take is set up, not on the clock at the downbeat (eleven ticks at 120 BPM). The seed and
    the pass length are taken at the press. A `KnobLane` keeps a bit per written tick (and one per
    word) so `at()` finds the last value in a few word reads; walking back over a lane read from a
    file, which holds only where it changes, was quadratic and took 47 s for a 64-bar take.
  - **One lap, as baked** (maintainer, 2026-10-08). A clip whose sway or swell runs slower than the
    take is long is moved at the phases of the take's *first* lap. Played back, the take repeats
    that lap, so where the clip's sweep had not come round by the lap's end, the take jumps at the
    loop seam. Accepted: the take is one lap of what was heard.
  - **The band is held** from the take being laid out until it is saved or discarded:
    `Pattern::isBandHeld()`, set by `prepareTake`, cleared on Save; a discarded take is gone. Every
    way settings reach a pattern spares a held band -- `applyClipSettings` keeps its own band and
    `applyLanes` drops the band's lanes -- so an ACT accent on a take (and the restore when it
    lets go, even one taken from the clip before REC), or a clip without a figure loaded onto it
    from FILES, the browser or a Cue, lands everything but the band. The bar draws the band's
    knobs disabled and refuses them (`refusedWhileBandLocked`); a finger already on one is let go
    of (`releaseTheBand`). `Pattern::recordKnobs` writes no lane for them, WRITE's pass over every
    knob included.
  - **How a take ends.** `TakeUnderway` holds the take from REC to its end: the slot, the take
    and what the slot held. **One take at a time:** while one is scheduled or running anywhere, or
    unsaved on another slot, REC is refused with `-- TAKE UNDERWAY ON CH n`
    (`refusesANewTake`); again on an unsaved take's own slot it may, keeping the first's before.
    REC again before the downbeat calls the take off in the engine (`cancelScheduledRecording`,
    which also stops it if it started in the same tick; `startRecording` starts and announces
    only the take still scheduled) and puts the old clip back only while the slot still holds the
    take. Something else put into the slot -- a shape, a clip with its own figure -- ends the take
    there through `dropPendingTake`: called off or stopped, nothing put back. A set load ends it
    wherever it is, puts its slot's old clip back before the outgoing set is written, then loads.
    A new figure in a take's slot gets the band the slot had before the take, not the take's
    whole sphere (`settingsToCarry`).
  - **Threads.** The band's fields on `Pattern` are each atomic; `prepareTake` sets them before the
    take is the engine's, and with the band held nothing writes them during the take, so the
    clock and the UI never race on them. `HeightMapSphere::mapTo2D(…, ElevationParams)` still
    answers with the *nearest playable* direction for any band that does not cover a direction --
    the flight engine relies on it.

  The same goes for the rest of what playback does to a tick (decided 2026-10-07): the clip's
  squeeze and turn — rotate plus the spin's phase, the squeezes swept by their stretch — and the
  band's swell and sway and the lean's sweeps. `TakeProjection` holds the chain once:
  `playedPosition()` is what `playTick()` sends, `writtenPosition()` its inverse, and the engine
  writes each tick through the inverse at the phases the **first pass** reaches that tick with
  (`setPassPhases`, `ticksIntoFirstPass`). Squeeze and turn are exact (a squeeze is ×½…×2, it
  never collapses an axis — a point can be written up to twice outside the pad); the band is the
  only clamp. What cannot be known at recording time is answered on purpose, not exactly: a
  sweep whose cycle is not a whole number of passes is somewhere else on later passes (that is
  what it is for), Random enters at a phase drawn when it plays and is treated as Forward, and a
  playback length changed after the take re-times every sweep against it. Before, a take over a
  turned or squeezed clip played back turned or stretched by exactly that much.

  Deliberately **not** in the set: the clock mode and the rec mode. The clock depends on what is
  plugged into the switch at the venue and the rec mode is a working habit; a set that changed
  either out from under you on load would be a surprise at the one moment nobody wants one.

  **A slot carries the file it came from** — `clip`, by name and without a path, so a set
  travels — and its whole `ClipSettings`, not only what differs from the clip's own file. Since one
  clip per channel (2026-09-27) there is one slot per channel, and the channel carries
  **`actions`**: six entries, a script by name and the button's feel where it was turned (see
  "One clip per channel, six action buttons" below). The slot's old `action` is read only from sets
  written before, as A1/A2. The
  difference was worked out with `clipHasDrifted()`, which answers false for a slot with *no* clip
  file at all, so every slot filled straight from a shape wrote nothing and came back as a bare shape
  with its settings gone. `overrides` stays a `std::optional` for one reason only: a set written
  before this has none, and that has to go on meaning "leave the clip's own settings alone" rather
  than "reset it to the defaults".

  **What was running runs again** — from the top, on the next *downbeat*. Whether, and nothing more:
  where a clip had got to is not in the format, because a set coming back mid-figure would start
  somewhere nobody chose. The downbeat rather than the next beat because this is four clips starting
  together and together is the whole point of a set; a pad press is one clip and gets the nearer
  quantisation.

  A set names its takes the way the library resolves them (`indexForName`) rather than by index: a
  library's order depends on what is in the folder, so a set meaning "the third file" would mean
  something else on the next stick. A missing or unreadable set is an empty set, and a set written
  by a device with fewer channels or slots grows to fit this one — hardware outlives file formats.

  It is written **debounced**, not on exit: a drag across the grid is dozens of changes and one
  arrangement, and a device that loses power mid-set should not lose where everything was.
- `backends/SpatBackend*` — abstract backend interface with `SpatBackendA3` and `SpatBackendIEM`
  implementations; these are what actually format and dispatch OSC motion data.
- `elevation/HeightMap*` — maps 2D recorded positions onto a 3D sphere via the `HeightMapSphere`
  strategy, used for elevation coverage behavior.
- `AsyncCommandQueue` — the lock-free bridge from the high-priority tempo-clock thread to the
  backend/network thread, so OSC I/O never blocks realtime scheduling.
- `TempoLfo` — the clock the clip's slow movements run on. Everything that moves on its own here
  is held the same way: a **signed power of two in bars per cycle**, counted off the tempo clock
  rather than the wall clock, so a cycle comes back to where it started on a bar line instead of
  drifting through the loop underneath it, and follows a tempo change instead of being left behind
  by one. The sign is a direction and what it means is the caller's to say. `lfoSweep()` moves a
  0..1 parameter out of where it was set to the end the sign points at and back — *which* end
  rather than *how far*, because how far then answers itself, and because a sweep that ran
  symmetrically either side of the set value would shrink to nothing as that value neared a limit,
  which is one control quietly switching another off.
- `TrajectorySpin` — turning the whole trajectory around the vertical axis while the blob keeps
  running along it. Not a second motion path: it is one rotation of the *recorded 2D* position
  around the origin, applied at playback time and never written into the take, so it can be turned
  down again as freely as up. In the 2D disc the radius is the elevation and the angle is the
  azimuth (`HeightMap::mapTo3D()`), which is why turning the disc turns the trajectory around the
  pole and leaves every point at the height it was played in at.

  **A spin that is not running turns nothing.** The engine advances the phase and nothing rewinds
  it, so a spin turned off stands still at whatever angle it stopped at; counting that angle left
  `rot` saying one thing and the trajectory doing another, with no way back but a double tap on rot.
  `turnsOf()` is where that is decided, and it is the rule `lfoSweep()` has always had for the other
  two sweeps — the spin was the one of the three that added its phase whatever its step said. Three
  readers take the answer from there: the engine, the renderer, and the blue arc on the rot knob.

  How fast it turns is a `TempoLfo` step. The sign is the direction, and `spinPosition()` is the
  single place that decides which way that looks: the screen mirrors these coordinates
  (`cartesian2DHOA2JUCE` maps HOA to `{ -y, -x }`), so a mathematically positive turn reads as
  anticlockwise and the rotation is negated there to make a right-hand turn of the control a
  right-hand turn on the sphere.

  Two places apply it and both must, or the blob leaves its line: `MotionEngine::performPlayback()`
  turns the position before projecting it, and `projectLine()` (`components/LineMapGeometry`, which
  `drawPathOnSphere()` draws from) turns each point of the drawn path by the same phase — inside
  `projectPoint()` rather than by transforming the path, which would mean copying it every frame. The phase lives on the `Pattern` beside
  `playPosition` and resets when playback starts, so a clip fired again begins where it was
  recorded.

  **The pad is wrapped around the base, not sheared towards it.** The radius is the angular distance
  from the base *direction* and the disc's angle is the bearing around it — so the figure is a cap
  centred on the base, and it grows out of it in every direction rather than towards a pole it has to
  pick. At a base of 0 that is the same arithmetic as before, which is why every clip sitting at the
  pole sounds exactly as it did.

  It used to take the disc's angle as the room's azimuth and its radius as a change in colatitude,
  which is a proper wrapping only while the base is a pole. Anywhere else the pad's centre stood for
  "this colatitude, *any* azimuth" — a whole circle of directions — so two neighbouring ticks either
  side of the pad's centre landed on opposite sides of it. Measured on a Clover's 2048 ticks: at a
  base of 0 the largest step between two ticks was the average one, at 0.25 it was **117 times** it,
  at 0.5 **164 times**. That was the sound teleporting four times a lap, and the torn line was the
  drawing being honest about it. `Clover`, `Infinity` and `Rose 4-Petal` pass exactly through the
  origin; so does any take driven through the middle of the pad.

  Two things fell out with the shear: the cone had a *direction* to pick, which was discontinuous at
  a base of exactly 0.5 and needed holding across a sweep, and it could run out of *room* at a pole.
  A cap has neither. Both were answers to the shear and both are gone.

  **What the clips cut off runs along the cut.** A point pushed past `clipTop` or `clipBottom` keeps
  its bearing and gives up only its height, so a figure that reaches into the ceiling comes out as a
  figure travelling around the ceiling rather than a heap of points on one spot.

- `TrajectoryShaping` — **everything done to a recorded 2D position before it is projected**, in
  one bundle (`PlaneShaping`: the turn, and a squeeze per horizontal axis) and one function
  (`shapedPosition()`). Neither the engine nor the renderer composes transforms of its own any
  more; both build a `PlaneShaping` with `shapingOf(pattern)` and hand it over. That is the same
  reasoning behind `sweptElevation()`: five places each composing their own transforms is five
  chances for the drawn line and the running blob to disagree, and the disagreement is only
  visible as a blob floating beside its own path.

  The two squeezes (`sqzX`, `sqzY` in the Motion section, the clip file, a set and a script) are
  bipolar with their middle at zero and multiply their axis by `2^value` — half at one end, double
  at the other, so a squeeze undoes the stretch the same distance the other way and the middle of
  the travel is the take as recorded. **X is front-back, Y is left-right**, and since the screen
  mirrors these coordinates (`{ -y, -x }`) `sqzX` is what a viewer reads as the sphere's
  *vertical*. Unlike the spin there is no sign to arrange: a mirror leaves a scaling about the
  origin alone.

  **The squeeze happens before the turn.** The ellipse belongs to the figure and travels with it,
  which is what "I flattened my orbit and set it spinning" means. Turning first would leave the
  ellipse standing in the room, and a spinning circle — the commonest take there is — would look
  as though the spin had stopped working.

- `SpaceTurn` — **the figure's plane leant in the room**, after the height map: `tilt` about the
  left-right axis (positive takes the front down), `roll` about the front-back axis (positive takes
  the left down). Both in **quarter turns on a closed ring**, `-2..2` the whole turn, wrapped by
  `wrappedLean()` rather than clamped, the way `rot` wraps. Their sweeps `tswp`/`rswp` **turn them
  round like spin turns rot** (2026-09-28, *„nicht hin und her sondern rundrum"*): one revolution
  per the step's bars on the `TempoLfo` table, sign the direction, 0 still — and a stopped sweep adds
  nothing, `turnsOf()`'s rule. Before that they swept out and back (`lfoSweepBipolar`) within
  `-1..1`. The unit stayed the quarter turn so every lean saved as `-1..1` is the same angle now; a
  clip with a sweep set tumbles instead of rocking, which was the point. Past a quarter turn is still
  a rigid turn of the whole sphere, so upside down is a figure hanging from the floor, with its
  clip-top/-bottom turned over with it (the clips cut before the lean). The knobs are rings with
  upright at the top (`knobAngleFraction`, which measures a ring from its value zero).

  **The clip's third modulation is not a movement at all.** `Envelope` is the accent: it rises while
  the **ACT pad is held**, stays up for as long as it is held, and falls when it is let go. The hold
  is the finger, which is why there is no sustain control — on a pad, how long a thing lasts is a
  gesture, and a gesture beats a number you would have had to set beforehand for a moment you did not
  know was coming. Its two times are `atk` and `dec`, in bars off the tempo clock like everything
  else, but on a shorter table (1/16 of a bar to 4) since it is a gesture rather than a cycle.

  What it drives is the channel's **3d**, and only upwards, between two ends it does not choose:
  `envelopeOver (set, max, level)` returns the set value at rest — exactly, not nearly, or the pot
  would drift every time an accent finished — and raises it towards the clip's `max` as the envelope
  climbs. A ceiling set *under* the floor leaves the floor alone: it is a setting somebody will make
  by accident, and an accent that pushed the value down would surprise in the one direction nothing
  else here moves. The hardware pot and the grid keep meaning what
  they always meant; what they mean *is* now the bottom of the swing. `MotionEngine::advanceAccents()`
  runs it on the tempo-clock thread beside playback, and `getChannelPot3Effective()` is what both the
  OSC sender and the mixer pages' 3D knobs read, so the knob on screen moves with the accent instead
  of leaving you to take it on trust.

  The knob draws it where you can watch it: the 3d knob's **pointer stays on the set value** and the
  arc from there to the effective value is filled in the notice colour, so the knob shows the floor
  and the movement at once. That is why `ChannelPotValues` carries the setting and the effective
  value side by side — a knob whose pointer moved with the modulation would have nothing left to
  say where the hand had put it.

  **When the decay runs out the clip does what its end action says** (`applyEndActionAfterAccent`),
  and only on that edge, once. Stop and Pause end the pass; Loop means "keep going" and is left
  alone, or the accent would be a stop button that only some settings noticed.

  **The bar follows the hand.** Pressing play or the accent on a pad selects that clip in the clip
  settings, so what you are reading is what you just touched. On a *press*, not on every start: a
  clip that an end action or a chain started did not come from a finger, and moving somebody's
  selection out from under them mid-adjustment is what made this a question rather than an obvious
  yes. Stop and Settings do not move it — Settings is the one that selects without doing anything
  else, which is what it is for.

  It fires on ACT **whatever the clip is doing** — an accent is not a start, and behind the
  start's "is this idle" check it fired only on a clip that happened to be standing still, which is
  the opposite of when you reach for it.

  **The clip's second slow movement, `swell`, works the same way** and is the reason `TempoLfo`
  is its own module. It sweeps the Pattern's `reach` — how far down the sphere the trajectory's
  outer edge lands — out of where it was set and back, positive opening the coverage towards the
  far pole and negative closing it towards the near one. Same two places have to agree: the engine
  sweeps `params.reach` before projecting, the renderer sweeps it before drawing the line, both
  from the phase on the `Pattern`. Its control lives in **Motion**, beside the `reach` it sweeps —
  every row of that section is a standing value next to the movement that works on it, the way `rot`
  stands next to `spin` and each squeeze stands next to its own `str`. Motion is five such rows and
  nothing else — `rot|spin`, `reach|swell`, `sqzX|strX`, `sqzY|strY`, `fade|bias` — numbered in
  reading order, which is the first time its sub-indices and its layout have agreed. (Since
  2026-09-26 `fade|bias` stand on the REC page, still as Motion's 8 and 9.)

  `sway`, which does the same to the elevation base, sits in **Elevation** beside `elv`, the base
  itself, for the same reason: a sweep says what it does only when it stands next to what it does
  it to.

### UI (`src/a3-motion-ui`)

`A3MotionUIComponent` (in `components/`) is the central orchestrator — it owns the main UI tree,
registers all hardware listeners, translates hardware events into `MotionEngine` calls, and
handles OSC in/out (beatclock, VU, tap). Read `team.md` (German) for a detailed, currently-accurate
description of this component's event flow, button semantics, and the clock-mode/settings-area
state machine — it's the best single source of truth for UI behavior and is worth consulting
before changing button/settings/clock logic.

High-level structure, top to bottom in `A3MotionUIComponent::resized()`: `StatusBar` → the rest of
the screen split into `MotionComponent` (the sphere) and, docked to the bottom quarter, the
"settings area" — `ClipSettingsComponent` (permanent; three sections describing the last-selected
clip — Shape, Elevation, Motion — plus a global section taking the bar's right half) with
`GlobalSettingsComponent`
(Skin, Skin Editor, Network, Button LEDs, Pattern Folder, Sphere in Menu — opened by
the Menu button; sizes and fonts are skin values, edited in the Skin Editor's *Text and size* section) drawn on top of it while open. Both settings
components share that bottom-quarter rect, carved out of `MotionComponent`'s actual bounds rather
than just overlaid — `MotionComponent` renders via its own directly-attached `OpenGLContext`, which
always composites above normal JUCE components regardless of z-order/`toFront()`, so nothing can
visibly overlap it without a real bounds change. `LoopLengthDisplay`, `ElevationDisplay`,
`PadRowDisplay` rows, and `FilterDisplay` still exist and keep receiving their normal update calls,
but are permanently hidden (`setVisible(false)`) and no longer given screen space — same for
`ChannelStrip`.

Each channel's two rotary encoders have one job each, the same one whatever is on screen:
**upper = freq, lower = Q** for that channel (`handleChannelValueChange`, writing
`MotionEngine::setChannelPot1/2`). Pressing an encoder does nothing.

**The panel's four physical potentiometers drive the third per-channel value ("3d",
`setChannelPot3`), one per channel** — it goes out on `/channel/{ch}/3d` and A3 Core crossfades
that channel between its stereo and multi encoder on it. Core's boolean `3d` toggle has moved to
`4d`; the A3 Mixer button that used to send the boolean is gone in hardware v3.2, so nothing
collides. They are `InputOutputAdapter::getGlobalPot(0..3)` — *not*
`getPot(channel, n)`, which despite the name is the **pot-encoder's synthetic value** (turning it
adjusts the selected one, pushing it switches which; see the protocol comment at the top of
`InputOutputAdapterV3.hh`). Wiring 3d to `getPot()` looked right and did nothing, because that
encoder turns Q outright now and never produces those values any more. `getGlobalPot()` is virtual
on the base class and returns a value that never changes where the hardware has no pots.

Note also that `handleChannelValueChange` takes a `ChannelPot`, not a pot number. It took a grid
row until 2026-09-26, and the rows read 3d, freq, Q from the top, so a literal 0 meant for "freq"
reached 3d — exactly what went wrong when the rows were reordered. The engine's numbering (pot 1
freq, pot 2 Q, pot 3 3d) is translated in one place, `channelPotValue` / `setChannelPotValue`.

They used to scroll the bar's sections, change the selected row's value, and — on channel 3 —
navigate the settings menu, the skin editor and the colour picker. **All of that is touch now.**
Nothing in the encoder path depends on what is open any more, which is the point: a knob that means
something different depending on the screen is a knob you have to look at.

The trade the maintainer accepted knowingly: with the touchscreen out, the device cannot be
operated at all. It used to be fully drivable from the hardware.

#### Touch

The encoders are not the only way in: `ClipSettingsComponent`, `GlobalSettingsComponent` and
`SkinEditorComponent` are also operated with a finger. None of them draws a `juce::Slider` or
`juce::Button` — `LookAndFeel_A3` sets colours for those, but nothing in the project instantiates
one — so the touch path is built from three small pieces instead:

- **`components/TouchControl.{hh,cc}`** — an invisible `juce::Component` laid over what `paint()`
  draws. It contributes only what JUCE will not give you without a component: bounds-based hit
  testing, event routing, and a drag with a proper origin. It draws nothing and holds no value.
  Each carries an identity (`primary`/`secondary`, e.g. section and sub-element) that comes back
  unchanged in its `onTap`/`onDragIncrement`/`onRelease` callbacks.
- **`components/DragAccumulator.{hh,cc}`** — turns a drag into whole ±1 increments. Vertical and
  relative, up is more. It counts against what it has already emitted rather than per event,
  because JUCE coalesces movement: per event a fast drag loses steps and a to-and-fro drifts.
  The threshold is the skin's `touchDragPixelsPerStep` (default 12), so it is adjustable on the
  device like Pot Size and the font sizes.
- **`components/ClipSettingsLayout.{hh,cc}`** — every rectangle in the clip settings bar, from one
  pure calculation that takes bounds plus the header/body font sizes and Pot Size. `paint()` draws
  into it and `resized()` puts the `TouchControl`s on it, so the picture and the hit areas cannot
  disagree. `ControlMetrics` lives here too. `GlobalSettingsComponent` and `SkinEditorComponent`
  do the same thing with their own row geometry (`globalSettingsRowBounds` and friends; the skin
  editor's stays a private member because its name/value split follows the value's length).

Touch produces the same increments the encoders do and goes through the same handlers in
`A3MotionUIComponent` — there is no second value model. Where the encoders *cycle*
(`handleClipSettingsScroll`, `handleClipSettingsSubElementCycle`), touch *sets*
(`selectClipSettingsSection`, `selectClipSettingsSubElement`), which is why a tap reaches a
control in one move. A tap on a few-valued control also changes it, and *how* depends on how many
values it has: three or more **step** on and wrap (direction, end-action, rec mode —
`tapAdvancesValue`), exactly two **flip** (pole, flat — `tapTogglesValue`, its own callback
`onControlToggled`). The split exists because stepping a boolean is direction-tied — an encoder
turned right meant South — and a tap has no direction, so it always said +1 and the value could
only ever be switched on. A drag on those two keeps the direction: up is on, down is off. A
continuous value is dragged, never tapped.

Two consequences worth knowing when changing this code:

- A container that should let its children be touched needs `setInterceptsMouseClicks (false,
  true)` — false for itself, true for children.
- `ClipSettingsComponent` implements `ThemedComponent`: its whole geometry is built from skin
  values, so a skin change is a re-layout there, not merely a repaint.

Clock mode (`_clockMode`: `0=INT, 1=EXT, 2=PIO`) governs whether the UI drives its own tempo (tap
button sets BPM, `/beat` sent via OSC) or follows an externally received `/beat` OSC stream
(playback synced to external phase, `/beat` not re-sent to avoid feedback loops).

#### OSC addresses

Every address, port and IP this device speaks comes from the **one truth**,
a3-core's `/usr/share/a3/a3-osc.json` (decided 2026-09-30; `$A3_OSC_TRUTH`
points elsewhere). `a3-motion-engine/OscTruth.{hh,cc}` reads it — the C++
counterpart of a3-core's `a3_osc.py` — and three functions turn it into what
the device needs: `oscAddressesFrom()` (the addresses, by the truth's keys),
`oscEndpointsFrom()` (where Core and the beat-analyzer listen, and Motion's own
three sockets) and `vuRoutingFrom()` (which `/vu/N` feeds which meter, looked
up by the meter's **name** in the channel map — `in1_pre_L`, `main_sub`,
`main_top1` — never by its number). `config.json`'s `oscSender`,
`oscReceiver` and `oscAddresses` blocks are no longer read.

Three things are not obvious:

- **Nothing has a default.** A key the truth lacks becomes
  `/a3-osc-missing/<key>`: JUCE takes it, so nothing throws (`juce::OSCMessage`
  throws `OSCFormatError` on an address it will not take), and on the wire it
  names what is missing; start-up prints every missing key. A listener the
  truth lacks is port -1: its socket does not open, and the log says so.
- **One optional key: `stemdeck.ahead`**. StemDeck's
  preview of the music, relayed by Core, is only *listened for*, so it has no
  `/a3-osc-missing/` mark: `OscAddresses::stemdeckAhead` stays empty when the
  truth lacks it, and `optionalOscAddressKeys()` keeps it out of
  `missingOscKeys()`/`unusableOscTruth()` — an older Core's truth stays
  usable. `OscMessageHandler::routePreview` reads `ssif` into a `MusicAhead`
  (`a3-motion-engine/preview/MusicPreview.hh`), "none" clears it, anything
  else is ignored; the UI keeps it in `_musicPreview`, which counts it absent
  after four bars without a new one; while it is fresh the pilots time their games by it (see
  **FPV: games and the pilots' level**).
- **Channels count from 1 on the wire, from 0 in here.** `withChannelIndex()`
  is the one crossing, and says so in its name.
- **One meter may feed two places.** The main sub is the sphere's glow *and*
  the first meter of the master column, the first four tops light the towers
  and fill the column too — so `OscMessageHandler::routeMeter` asks every
  table, not an else-chain, and the column has its own listener call
  (`onOutputVU`).
- **A channel's meter is stereo, one value on screen.** Since 2026-10-06 each
  channel is metered as `inN_pre_L` and `inN_pre_R`; the handler remembers
  the side that did not just arrive and hands on peak and RMS each as the
  louder side's (`routeChannelSide`), as a DJ mixer shows a stereo channel.
  The mono `inN_pre` read 3 dB low (REAPER's send downmix) and is no longer
  read; the `inN_post` pairs are the desk's, not Motion's.
- **The meters' ballistics are the truth's, and they do have defaults.**
  Since 2026-10-07 the analyzer and StemDeck send raw peaks (the highest
  sample since the last tick, no fall, no hold) and every display applies the
  same ballistics on its own clock: attack immediate, release 20 dB/s, a hold
  line that stands 1.5 s and then falls at 20 dB/s. The numbers are Core's
  top-level `meters` block (`attack_ms`, `release_db_per_second`,
  `peak_hold_seconds`), read by `OscTruth::meterBallistics()`; unlike an
  address, a missing block or key falls back on those decided numbers
  (`MeterBallisticsParameters`), because JUCE reads a missing key as 0 and a
  release of 0 is a meter that never falls. `MeterBallistics` is the one
  implementation; `VuLevels` keeps one per meter, so Motion's bars are the
  falling peak and the white mark is the hold (`VuReading`). The rms on the
  wire is no longer drawn by a meter; the sphere's corona, glow and speaker
  lights still read peak and rms through their own skin envelopes.

The addresses are applied once, at start-up (`applyOscAddresses`) — the truth
only changes with a package install. They still cross a thread on the way:
the send backend stores them under a lock (`SpatBackend::setAddresses()`) and
rebuilds its cached per-channel patterns on the sending thread, and the beat
address has its own copy for `tickCallback()` on the tempo-clock thread.
Reading a `juce::String` on one thread while another replaces it is a race,
refcount and all.

The tests build their messages from a made-up truth (`MadeUpOscTruth.hh`), so
they test the mechanics, not a copy of the vocabulary; `OscTruthContract`
holds Motion against the real file, which `test.sh` finds (installed, or the
a3-core checkout beside this one) and names.

#### Core's truth at start-up

Motion opens its window **once**, on Core's truth (a3-system#74, decided
2026-10-07). `StandaloneApp::initialise` starts the truth link before any
window exists and hands it the digest of the file it *would* load
(`oscTruthFileDigest (oscTruthFile ())`). On Core's first `/core/here` with
that digest the window opens; with another digest the link fetches it into
`~/.cache/a3/a3-osc.json` and then opens — nothing has loaded the truth yet,
so `installedOscTruth ()` reads the new file. No announcement within
`truthkeeper::startupWaitMs` (10 s), a refused fetch, a busy announce port or
a set `$A3_OSC_TRUTH`: it opens on what is on disk. The decisions are
`truthkeeper::StartupWait`; once the window is open the link follows the
loaded digest, and a truth changed *later* (a deploy) still fetches and
restarts as before. The one start-up case that still restarts: a fetch that
lands after the 10 s.

The bar's **global section** takes its right quarter and holds three things: the elevation picture,
the four channel faces and the transport two by two. A **Filter section** used to sit among
the clip's sections showing freq and Q — but those were never the clip's: they are the same
per-channel values the hardware drives. They moved into a 4x3 grid in the global section, and on
2026-09-26 out of the bar altogether, into the mixer strips (3D, FREQ, Q under SEND; see the mixer
paragraphs below).

**How tall the bar is** comes from `clipSettingsPreferredHeight` — what the tallest section's
contents need at the current fonts and pot size — times the skin's `clipSettingsHeightScale`
(default 1.0, clamped 0.5..2.0), and clamped again to half the screen. The scale is a skin value
like `potSize`, so it is dialled in the Skin Editor rather than compiled in. Note that changing it
is a *layout* change and A3MotionUIComponent is what hands the bar its bounds — that is why its
`applyTheme()` ends in `resized()`. Telling the bar alone changes nothing.

**The Shape section has one face**: the clip as it plays — its picture, the **clip field** naming
what is in the slot and how long the next take will be, four speed buttons (`1`, `1/8`, `1/16`,
`1/64` on a fresh device), and the **direction and end action** under them. Those two came from
Motion: what a pass does when it runs out is a property of the take, and the take is what this
section is about.

It had a second face until 2026-09-23 — `BarPage::Record`, eight length keys and the `fade` — and
the REC tab that turned the card over. Asked for at the device: *„das REC Tab im settings soll weg.
rec nimmt cliplänge auf die gerade in playmode ausgewählt."* A take is now as long as the clip it
is recorded over (`RecordingLength.hh`), so the keys had nothing left to set; `fade` kept its own
knob in Motion, and REC records straight away instead of turning a card first.

Three things fell out with it, and they are the argument for the change rather than a side effect:
the Shape knob stopped meaning two different values depending on which face was up (`rot` on the
front, `fade` on the back), the picture stopped being drawn twice in two places, and a length that
used to be set on a page you had to open is now written where the clip is named.

**The picture never needed the page.** It is drawn from the pattern: a library entry gives its SVG,
and a pattern with no entry — which a fresh take is — gives its ticks. So the take appears in the
CLIP picture as it is played in, and the timer keeps the bar refreshing while `isRecording()`. The
record face showed the same picture in a different place.

**The speed keys are tapped and dragged, and what they carry is the performer's.** A tap plays the
clip at the speed the key carries; a drag on a key gives *that key* another speed, out of the whole
of `speedLog2Min..Max`, and applies it straight away — every other drag in the bar changes what you
hear while you drag, and one that only rearranged keys would be the exception you have to remember.
The key keeps it, so a speed the four do not yet name is reached once and found again next time.
The four are part of the set (`speedKeys` in the session file, since 2026-09-25) as well as the
device settings: loading a set brings its keys, and a set written before then leaves them alone.
The drag used to walk the *shown clip* through the range instead: only the key matching the current
value ever lit, so it read as jumping between the keys, and the value was gone again the next time.
Twelve buttons covering the whole of `speedLog2Min..Max` took three rows to say every value the
range holds; the two rows they gave back are what the field stands in. Their names are computed
from their values by `speedLog2Name()` — one place where a speed is put into words, since a key
that can be dragged has to retell itself — and the four values live in `AppSettings`, defaulting to
what the fixed table carried, because a favourite speed is a working habit like the rec mode rather
than part of an arrangement a set would rewrite on load. `speed` left Motion for this section: a clip's tempo belongs beside its shape. That forced the first
**renumbering** of a section's sub-indices rather than the appending everything else has used — an
index kept for a control that is gone is worse than a finger relearning where three things are.

**The Shape section stands on its own button grid.** The picture takes three of the four columns the
buttons use and the knob takes the fourth, measured from the left exactly as the buttons are stepped
across — taken from the right it came out three pixels off, because `colW` is an integer division and
the remainder sits against the right edge. A row that nearly lines up with the grid under it reads
as a mistake; one that lines up exactly reads as structure.

**The sphere can be looked at from somewhere else.** `SphereCamera` is two angles — how far the eye
has come down from straight above, and how far round it has walked — and **both being zero is the
view the device has always had**, short-circuited to the identity so a device nobody has tilted
computes exactly what it computed before, to the bit. **Camera mode** moves it (2026-09-26): a touch
on the elevation picture at the top of the bar's global strip selects it — its grey field lights —
and while it is on a finger on the sphere turns the view instead of taking a blob
(`MotionComponent::setCameraMode`), and two taps put the view back where it starts. It was SHIFT
with a finger on the sphere once, then a little sphere in the view's corner; a mode shown by a lit
picture in plain view replaced both. **The lean runs from straight above to the horizon, one way
over only** (`cameraFromBallDrag` clamps it to `[0, π/2]`, dragging up leans): past the horizon
the sphere is seen from below, and the other side of the zenith stood every speaker on its head —
which is what the first limit, `[-π/2, 0]`, kept by mistake. Walking round is left and right, so
one way over is every view there is. **Zoom** is camera mode's too: the wheel (`zoomFromWheel`, a
tenth a notch), a trackpad's magnify and a two-finger pinch on the sphere (`zoomFromPinch`) scale
the sphere between `minCameraZoom` and `maxCameraZoom`; it is a factor on the skin's sphere size in
`updateBoundsAndTransform`, so everything placed through that region follows. A second finger
turns a turn into a pinch; only a finger alone on the sphere counts towards the double tap, which
puts view and zoom back. **A finger is counted once** (`CameraFingers`, 2026-10-07): on the device X
sends every touch a second time as an emulated mouse at the same point, and with grabs keyed by
source (9a04236, for the blobs) the camera took one finger for two — a pinch of zero width — and
stopped turning. Only sources of the kind that touched first count until its last one is up, so two
real touches still pinch and a mouse alone (VNC) still turns. A small camera in the picture's top right corner (`elevationCameraMark`)
says what touching the picture selects. **The view survives a restart**: lean, walk and zoom are
device settings (`AppSettings::camera*`), saved whenever a camera gesture settles
(`MotionComponent::onCameraChanged`) and held to the same limits when read back.

Everything that projects goes through `MotionComponent::projectToScreen()` and everything that reads
a finger goes through `pixelToDirection()`. There were seven hand-written projections, and a camera
applied at six of the seven is a picture whose halves disagree about the view. The shader gets the
same two angles as a uniform, so the ball turns with the trajectories drawn over it.

**The sphere's graticule is the room's, not the screen's.** It was a Cartesian net on the *screen*
normal — planes of constant N.x, N.y, N.z — which draws the same picture whichever way the room is
looked at, and that picture happens to read as a globe seen edge-on. It is circles of equal height
and lines of equal bearing now, so looking straight down reads as rings around the zenith and spokes
out of it, and a tilted view reads as a globe. The meridians' width is weighted by `cos(lat)` or they
crowd into a blot at the pole, which is the middle of the picture in the view this device is usually
in.

**The Shape section has two controls, and they are two questions**: the picture is which figure the
sound traces, the field under it is which values it is played with. Each is scrolled with a thumb —
the field was briefly a dropdown like ACTION's, which covered the picture you are choosing by.

They walk two different lists, and `stepThroughLibrary()` is what keeps them apart: the picture steps
through the shapes, the field through the settings presets. One scroller over both walked them in a
single list, so pushing the picture could quietly apply somebody's preset. **Swapping the figure
keeps the values** — the settings are read off the old pattern and put back on the new one, because
the one thing the picture must not change is how the slot is played.

Two names, for the same reason: the shape's lies over the picture, the clip's is the field's value,
and an empty field (`--`) is a slot playing a figure with no clip behind it. The field also carries
the **drift dot** — warning-coloured, the same mark the slot keys used to have, saying the values
have been turned since they were loaded and something is waiting to be written.

**The header's channel faces keep the browser open.** A face selects the clip the settings area
describes, and FILES has one in mind — the slot a picked file is put into. Being thrown out of the
list halfway through "choose the slot, then choose the file" meant losing the list you were reading.
FILES lies over the sphere since 2026-09-27 (below), so a face changes the bar underneath and leaves
the browser where it is; `selectClip()` refreshes the browser while it is open so the highlighted
row follows the slot — only while it is open, because refreshing walks the pattern folder.

**`rot` is a closed ring**, the only control in the bar that is. A rotation comes round to itself, so
its scale has to: on the usual 270-degree sweep the two ends are the same angle with a dead zone
between them, and a value that wraps then reads as an amount rather than as a position. A ring has
no start to fill from either, so it has no value arc — the pointer says where the hand left it and
the blue says where the spin is holding it now. Watch for the trap that caught this: the pointer's
colour used to be inherited from whatever the value arc had set, so removing the arc drew the
pointer in the modulation's blue. It sets its own colour now.

**A knob's blue arc is fed, not drawn from what the knob knows.** A `PotKnob` holds its value; where
a modulation is carrying that value right now — the spin under `rot`, the swell under `reach`, the
stretches under the two squeezes — comes from the page, through `putReachOnKnob()`, converted by
`reachOnKnob()` (`ClipKnobs.hh`, tested) into the angle the knob is drawn in. When the knobs became
sliders on 2026-09-23 that wiring stayed behind with the old painting code, and every blue arc in
the bar was gone for three days without a test noticing (a3-motion-ui#35).

**The header's four transport keys are the shown clip's pads**, routed through `handlePadPress()`
rather than reimplemented. The timing rules — play on the next beat, stop now, the accent for as
long as the finger is down — live there, and `padIndexFor()` in `PadFunctions.hh` reads the pad
tables backwards to find the right pad. Two routes to one function that each keep their own copy of
what it means will differ eventually, and the difference shows up mid-set.

**Record is a toggle on the screen, and a modifier on the panel.** The bar's key and the transport
key both call `toggleRecordingOnShownClip()`: press to start a take on the clip the bar is showing,
press again to end it. A key that only ever goes one way leaves you reaching for a different control
to undo what it did.

**A take is kept when somebody says so** (issue #28). The Stopped message no longer saves: a take
with anything written in it stays in its slot, playing, marked unsaved, until SAVE writes it --
the shape and a clip with every setting on it at the moment of the press, including what was dialled
after the take ended -- or DISCARD puts back what the slot held. `PendingTakes` holds that "before"
per slot, and a second take on a still-unsaved slot keeps the *original* one, so DISCARD always goes
back to the last saved state and a second take that comes to nothing leaves the first standing.

It is dropped only when something **replaces** it: a new take, a shape dropped on the slot, a set
loaded, a restart. Never on a timer and never because the performer moved on -- the good take at 2 a.m.
is exactly the one nobody remembers to save at once. A set names only what is on disk
(`PendingTakes::forSet()`), so `current.json` brings back the old state after a restart.

REC and ACT change **face** in place rather than a fifth key appearing (`transportFace()`): SAVE is a
tick where REC was, DISCARD a cross where ACT was, asked twice like Delete in FILES. A row that
narrows under the finger is a row you miss in. The accent is not on the bar while a take waits; the
pad and the panel still have it. FILES keeps Save and Save as dark for clips and shapes on an unsaved
slot: it still points at the clip file of what it held before, and Save would write the take's
values over that.

**The panel's REC key is not that key**, and this is the one place the two halves of the device
deliberately differ. There it is held while a slot's Play|Pause pad names the slot
(`handlePadPress()`), because the panel has pads and a finger on the screen does not. Pressed alone
it arms nothing; pressed while a take is running it ends it, because the finger no longer bounds a
recording and something has to.

**Pressed alone on the panel it now does nothing visible**, and that is a known cost of removing the
REC tab: it used to turn the Shape card over, which was how you saw the key arrive. Raised at the
device on 2026-09-24 — *„per touch aufm display geht record nur am hardwarecontroller nicht"* — and
the maintainer chose to leave the behaviour as it is rather than make the panel key a toggle too.
Documented rather than changed, on purpose. Anything that says the panel key toggles is this
paragraph's older, wrong version.

**`TransportLook.hh` is the one rule for what the four clip actions look like**: red for record
and stop, green or red for play/pause depending on whether the clip is running, yellow (`highlight`,
a skin value like every other colour) for the accent. The bar's header keys, the pads page, the
global strip's REC and the ACT beside the envelope all read it, and `drawTransportGlyph()` draws the
marks — circle, square, triangle, bars — for both the header keys and the pads. Words needed a dark
plate behind them to survive a channel-coloured pad, which is why they are shapes.

**A shape alone did not survive either.** On the pads page each mark was drawn in its function's
colour straight onto the pad, and nothing asked whether the two told apart: a running clip turns its
Play pad `accent`, the triangle's own colour, and the mark vanished; with the sunset skin eleven of
fifteen function/ground pairs measured under 3:1 (2026-09-22). `padGlyphInk()` decides the ink now: black or white on every pad, whichever
stands out more — one of the two always reaches 4.58:1. Keeping the function's colour wherever it
could be read was tried first and made the page a patchwork (one column all black, the others
green, yellow and white); the maintainer settled it on black and white, and the shape says which
key it is. Settings is the one pad that is not a
transport action: it gets three bars from `drawMenuGlyph()`, always in black or white, because it
stands for no state and so has no colour of its own.

Play is green whether or not it is running. The colour says which key it is, not what it is doing —
a key that changes colour with its state has to be looked at twice, once to find it and once to read
it, and finding it is the job that matters mid-set.

**A section's lists are keyed by sub-index in three places** — `tapAdvancesValue()`,
`dropdownValues()` and `dropdownCurrentIndex()` — and renumbering Motion without renumbering all
three broke every one of its lists at once: `act` offered the direction's words, `dir` offered the
end action's, and `end` offered none. `dropdownCurrentIndex()` had in fact been wrong for two
renames already (`sub == 1 ? direction : endAction`) and nothing noticed, because a tap in a list is
applied as the *difference* to the current index — a wrong current index lands on the wrong entry
rather than failing.

**A pattern file normalises its scale, never its position.** `PatternFile` used to recentre on the
bounding box, and `trajectoryIconFromTicks()` still did the same for the picture. The origin is the
middle of the room, so recentring moved the sound: a triangle came back sitting below the listener,
and because `spinPosition()` turns about the origin, rotating it swung it round instead of spinning
it in place — the wobble. Sixteen of the thirty-nine system patterns were off by more than 3% of
their radius, the triangle by 35%. The furthest point from the origin becomes 1; nothing moves.
`smoke-test/scripts/pattern-centre.py` measures it.

**A take is not a shape, and its file says so** (#68). Normalising, thinning to 128 points a run
and closing an open run with its reversed copy are right for a drawn figure and wrong for a take:
since a take keeps its clip's squeeze (#66) a written point can lie up to twice outside the pad,
so the whole take shrank on save; the resample by arc length on load evened out its pace; and an
open run played forward and then back. A pattern that went through `Status::Recording` is a take
(`Pattern::isTake()`), and `PatternFile` writes it with `data-kind="take"` and a path of every
tick, as it plays: one `M … L …` polyline per run, cut at gaps and teleports, with `data-ticks` on
the path naming each run's first tick. Loading reads it back vertex for tick and finishes it the
way a recording is finished (`markComplete()`), so its jumps are stood on between ticks. It is
still an ordinary path, so the browser, the pads and the preview draw it unchanged; a tapped take
also carries its taps as circles, and `peek()` hands those out instead of the path for its picture.
A file without the mark is a shape — every shipped one, and every take written before #68, which
loads as it always did (its scale was thrown away when it was written, and is not coming back).

**A jump dot is a hit, and says when** (#61). A shape of jumps used to be written as the places
it visits, one dot each, and read back spread evenly over the clip — so a tresillo, a gallop, an
offbeat, a shuffle and both claves played as even jumps, and a gallop's twelve hits over three
places folded into three dots. Each hit is now its own `<circle>` with `data-at`, the tick (of
`data-ppqn`) it lands on; it holds to the next one, and the tick before each landing stays empty,
as `fromSteps` plays it. A file whose dots carry no `data-at` — every one written before, and any
where only some do — is spread evenly as it always was. The rhythm shapes in `pattern/system` are
`a3-pattern-gen`'s output again, and `PatternFileJumps.TheShippedRhythmShapesPlayTheirRhythm`
holds them to the generator tick for tick.

Some shapes are legitimately off-centre and must stay that way, and
`SystemPatternIcons.EveryShippedShapeSitsWhereItShould` names them rather than leaving them to be
"fixed" later: `Arc` and `Petal` are one-sided by construction, `Orbit` is a Kepler ellipse with the
listener at a *focus* — the sound comes close and goes far — and `Random` is random. Everything else
is held under 7% of its own radius.

`Figure 8` used to be nudged 0.05 sideways "to avoid the azimuth singularity". There is nothing to
avoid *while the elevation base is on the pole*: `Infinity`, `Clover` and `Rose 4-Petal` all pass
exactly through the origin and always have. Azimuth is undefined at r = 0, but so is the direction
of a sound directly overhead — `HeightMapSphere::mapTo3D()` takes r → 0 to the north pole
continuously, so the crossing rises over the listener and comes down the other side.

**A held blob pushes the others aside on the screen only** (#56, maintainer 2026-09-29). An
untouched blob within reach of a held one is drawn pushed out onto that circle so the held one
stays reachable, and slides back along its edge when it is clear (`BlobPush`: `nextPushOffset`,
`easedPushOffset` once nothing is held). The push is `ChannelUIState::pushOffset`, in the height
map's 2D pixels; `drawnChannelPosition()` is what the shader draws and what a finger aims at.
The engine is never written by the push — before #56 it was, so dragging one blob carried the
other channels' sound and left it where it was pushed.

**Off the pole it is a real singularity, and the drawing has to know.** With a base of 0.5 the
disc's origin is drawn out at the rim, not in the middle, so a path passing *near* it swings the
azimuth through most of a revolution in almost no 2D distance — and `drawPathOnSphere()` (today
`projectLine()`, see below) decided how finely to cut a step by its length in the disc alone, so it drew that arc as one straight line clean
across the sphere. `discStepPieces()` (SphereProjection, and testable) weighs the swing as well as
the length. A path passing *exactly* through the origin is not fast but discontinuous — it arrives
at one bearing and leaves at the opposite one — so no amount of cutting helps and `addPoint()` lifts
the pen instead, the way it does at a take's gaps. Both only became visible when `sway` started
moving the base off the pole as a matter of course.

**Where the line runs is decided once, in `projectLine()`** (`components/LineMapGeometry.{hh,cc}`,
tested in `unit/LineMapGeometry.cc`): the shaped, lifted, camera-turned points with their depth and
the places the pen lifts. `drawPathOnSphere()` draws from what it returns. It was taken out on
2026-09-26 so that everything drawn from a line — the visible strokes, the line map, the braid —
starts from the same points: two projections would be two lines that merely happen to agree, and
the blob runs on exactly one of them.

**What goes into the line map, and in which order, is `lineMapStrokes()`'s**
(`components/LineMapStrokes.{hh,cc}`, tested in `unit/LineMapStrokes.cc`): the cone's ten steps
widest first, then the core, each piece one opaque colour — R nearness, G where along the figure
(core only), B the depth fade — painted over what is already there. The painting order *is* the
distance field: a narrower step lies wholly inside a wider one, so the last colour down is the
nearest. The constants (`lineMapSteps`, `lineMapCoreWidth`, …) live in that header with their
reasons. The braid's strand map works the same way: `braidCord()` cuts the strands into pieces by
depth band and tier, `strandMapStrokes()` turns them into the list for the 1024² map, back tiers
first. The visible braid of a line that has no map (previews) is stroked from the same
`braidCord()`.

**Both maps are painted on the GPU** (`components/LineMapRenderer.{hh,cc}`, a3-motion-ui#34).
`drawPathOnSphere()` only *collects* the strokes during the 2D pass; `uploadLineMaps()` paints them
at the start of the next frame, ahead of the sphere pass that samples them. `capsuleVertices()`
(`LineMapCapsules`, tested) turns a stroke list into two triangles per segment, and a fragment
shader keeps what lies within half the stroke's width — curved joins and round ends for free —
fading the last texel, composited premultiplied "over" into a framebuffer, one per channel and map.

- Stroked in software with `juce::Graphics`, the two maps were ninety per cent of the renderer with
  four clips playing. Measured on the rig on 2026-09-26: 8.5 fps in software, 14.3 with the line
  map on the GPU, 38.5 with both. The software stroking was removed after the maintainer compared
  the two side by side.
- Orientation: the sphere shader was written against maps uploaded with
  `OpenGLTexture::loadImage()`, which flips an image on its way to the GPU, so the pass writes
  `clipY = 1 − 2·y/size` to put its rows where those were.
- `OpenGLFrameBuffer::makeCurrentAndClear()` binds and clears **but does not set the viewport**.
  Without its own `glViewport` the first version painted the map at the screen's size: everything
  magnified from the bottom left, with a hard edge through the sphere at the map's own border. The
  pass sets its viewport and restores viewport, framebuffer, buffer, program and blend state.
- A GPU that cannot build the program says so in the log, and the trajectories then have no glow.

**No section is held any more.** Each of the bar's three sections had a lock that kept its values
when a clip was loaded. The maintainer took the locks out on 2026-09-27, when CLIP, MOTION and REC lost
their headings (the locks stood on those). Loading a clip now lands every value it carries, and the
figure it names. Holding a movement while stepping through clips is no longer a thing.

**A clip names its shape; a shape knows nothing about clips.** A clip is the playable thing — a
figure and every value it is played with — and `Clip::svg` names that figure by the name the library
resolves (`indexForName`), not by file name, which carries a beat-count prefix. The relation runs one
way and by name, so a shape renamed is rewritten the same way a set is.

It used to run the other way and by convention: `32_Helix.svg` went looking for `clips/Helix.json`.
That is what let a clip and a shape wear one name, which `indexForName()` cannot tell apart — and it
is how a **Save new** ended up renaming the instrument's own shape. Names are unique across the whole
library now, enforced where a copy is made.

The two are **two lists on two tabs**, CLIPS and SVG. They shared one list with a coloured dot saying
which kind a row was, which made what a tap did depend on a dot: choosing a clip fills the slot with
a figure *and* its values, choosing a shape swaps only the figure and leaves the values where the
hand put them — the same thing the picture on the CLIP page does, because it is the same gesture
reached from the other side.

`Default.json` is the exception that stays shapeless: it is the fallback a slot with no clip gets,
and a fallback naming a figure would put one into every empty slot.

**A clip file is the SVG.** There is no separate settings file: `PatternFile` writes the trajectory
*and* every value the clip settings menu holds — speed, rotate, spin, swell, the envelope, the whole
elevation block, direction, end action, act mode, fade. One file is one clip with its settings,
which is why the browser's library list *is* the list of clips. A **set** (`SetFile`) is the layer
above: which clip and which six actions each channel has, plus what belongs to the device rather
than to a clip — record length and per-channel 3d/freq/Q.

**The header row reads left to right in the order it is reached for**: folder, the three views of
the clip, the two slots, the four things you do to it. Marks are square and one row high; the three
words get what is left, shared. The row is a fingertip tall at the sizes the device ships with and
gives way below that — `clipSettingsPreferredHeight()` solves for it twice, once as a share of the
bar and once as a fixed thirty-four pixels, because at the smallest font and pot the fingertip is
not a share and solving as though it were left the global grid six pixels tall.

**The slot keys are gone** (2026-09-27): a channel holds one clip, so there is no second one to
reach. `slotButtons` is still laid out, empty, until the slot dimension is collapsed (step G of
the one-clip plan).

**The library's five keys are Filter, Rename, Save, Save as and Delete**, and all five say the same
words on every tab.

**Save writes what is on show back over the file it came from; Save new writes it to a new one.** Two
keys rather than one and a modifier — which of the two you meant is the whole question, and a
modifier makes it something you find out afterwards. Per tab, "where it came from" is the slot's own
clip file, the slot's own action file, or the set that is loaded; Save lights only when there is such
a file *and* something to write to it (for a clip, that means drift), Save as only needs something to
write. Before this, Save on ACTIONS and SETS always made a new file — so an action could never be
corrected without collecting "Action 4" beside "Action 3", and a set could never be updated at all.

**Save new opens the new row for typing**, keyboard and all — and finds that row *by identity*, not
by name. Found by name, a new clip landed on the shape's row whenever the two shared a name, and the
rename that followed then renamed the instrument's own shape and every set pointing at it. A clip is
found through its file (`indexForClipFile`); actions and sets are one folder each, where a name is
an identity. For the same reason a copy is given a name the whole library is free of, not merely one
the clips folder is: `freeClipName()` looks at the folder alone, so a copy of a slot playing "Helix"
was itself called "Helix", which `indexForName()` cannot tell apart. The name it gets is a counted one that
says nothing, and naming a thing is part of making it — a second key press to get there is one
somebody skips and then cannot find what they saved. On CLIPS what it writes is always a settings
preset, whatever it was copied from: what is being kept is how the slot is played, the shape is
already in the library under its own name, and a copy that named a shape would be listed nowhere at
all (the settings scan skips a clip that names one, and a shape finds its clip by file name).
 The filter stands first because it changes *what is listed* and the other three act on a
row of it: narrow the list, then do something to a row. It steps `All → User → System` on a tap and
wears the state it is in, not the one the next press would bring — a key naming what you would get
rather than what you have is a key you press to find out where you are.

It narrows the library and only the library: `Category::System` is the instrument's own shapes and
everything else is the performer's, which is a split the library already knows. The actions and the
sets land shipped and hand-written in one folder each with nothing marking which is which, so the
key goes dark on those two tabs rather than offering a choice it cannot make.

Once the list can be narrowed, **a row's number and a library entry's number are two different
things** — `_browserRowToLibrary` is the map, and everything acting on a chosen row goes through it.
Row zero stays on the list whatever the filter says: it is the library's "Empty", which is how a
slot is given nothing, and that is wanted however narrow the list is. What they act on is the row you chose in the list you are looking at, and the tab above the
list has already said which list that is — "Save Action" spent a word saying it again, and keys that
reword themselves between tabs are keys you read instead of aim at. The outer two work wherever a
row has a file behind it.

What a rename has to carry differs sharply by tab, which is most of the work:

| Tab | What moves |
|---|---|
| ACTIONS | the `.scd` file, and every slot firing it |
| SETS | the set written out under the new name — it carries its own name *inside* it, so a file merely moved would show its old name in the list it was renamed in — plus `_sessionName` if that is the loaded one |
| CLIPS | the name inside the SVG, the SVG's file name (keeping its beat-count prefix), the clip file beside it, every slot holding it, **and every set that names it** |

`PatternFile::setName()` writes that one attribute rather than re-saving: a re-save re-derives the
path from the ticks and hands back a file that is nearly, but not quite, the one that was read — and
a rename is the one operation that must not change the shape.

**Deleting deliberately does less.** A set's file goes and what is loaded stays loaded; a clip's
files go and the sets that named it are left alone, because rewriting somebody's arrangement because
a shape went would be a delete key editing files it was not pointed at — and a name a set cannot
resolve already loads as an empty slot, which is what the set now honestly holds. How many sets that
is, the arm step says *before* the second press (`chosenEntryCost()`). Everything playing goes on
playing: the pattern is in memory, and a file going is not a reason to stop the room.

**A rename is typed into the row itself**, not into a field somewhere else: what you are renaming is
a row of a list, and a name typed anywhere but where the name is makes you check two places. The row
wears the warning edge and a caret, Enter or the "Keep" key settles it, Escape or losing focus drops
it. `nameCharacterIsAllowed()` (in `ClipFile`, where a test can reach it) decides what may be typed,
as the key arrives rather than when the file is written — otherwise a name that cannot be a file
fails after the row has already stopped showing the old one. A name already taken is refused rather
than overwritten, and the row stays open so another one can be typed.

**Delete asks twice.** The key says "Delete", then "Sure?", and anything else you do — choosing a
row, changing tab, starting a rename — puts it back to sleep. Not a dialogue: there is nothing here
that could put one up without covering the list it is asking about. Renaming carries every slot that
fires the file across to the new name; deleting stops every slot that fired it from firing anything,
rather than leaving them pointing at a name with nothing behind it.

**Escape no longer quits.** On a desk that shortcut is a convenience; on a device standing in a
booth with a keyboard plugged into it, it is one stray key from ending the set, with no dialogue in
between because there is nothing here that could ask.

**A double tap on a knob puts it back to the middle of its range** (`onControlReset` →
`handleClipSettingsReset()`). Only knobs: a list has no middle, and `TouchControl::onDoubleTap`
fires *instead of* the second tap, so a control that stepped on tap would otherwise step and reset
in one gesture. The double tap is timed and distance-limited rather than taken from JUCE's
double-click, because on a touchscreen the second tap lands a few pixels from the first.

**The bar only refreshes itself while something is moving**, and the condition in `timerCallback()`
is the list of what counts. It said "while recording", so a playing clip got a transport key that
never turned green and a tick indicator that stayed empty. Anything new that animates has to be
added there; nothing in the bar repaints on its own.

The rec mode and clock buttons in the global section deliberately **never light**: they carry a
value, the value is written on them, and a wash that comes and goes says the same thing again in
grey and reads as a button stuck half-pressed. What they do carry is the value's *colour* — see
`recModeColour` and `Colours::clockMode`. REC and TAP still light, because what they show is
momentary and has no label of its own.

**The elevation graphic is a picture, and `elv` is the base** (2026-09-26). The base — where the
middle of the trajectory sits — used to be set by a finger on the circle, with the sway drawn as a
blue band beside it. Both went: the circle takes no touch and draws the base line alone, and the
base is Elevation's fourth knob, `elv`, left of `sway` (sub-index 3, so the other three kept
theirs). The knob turns the way a level does, clockwise higher, while the base counts from the top
(0 north), so it shows `knobForElevationBase(base)`; `elevationBaseForKnob` goes back, snapped to
ear height as the finger was and clamped into the band the clips leave. The sway is the blue arc on
`elv`, as every swept knob wears it: `setElevationBase()` still takes the setting and the swept
value, and a `swept` below zero means "standing still".

**No section wears the selection.** The selected card used to be filled with the channel's colour —
a coloured field a third of the bar wide, laid over the controls you are reading, that moved every
time a finger landed somewhere else. It said which *section* was armed and shouted it, and what
needs saying is which *control* is, which the pointer and the control's own colour already do. Every
card carries the same wash now; `isSelected` still reaches the controls inside it and the section's
title, which is a change of brightness rather than a panel.

The bar has one button face, `paintBarButton` — a wash and a thin edge, never a filled slab, so a
button reads as part of the bar rather than pasted on it. Elevation's pole and flat use it, so do
Motion's two lists and the global section's four. Only an active one carries colour; the global
four pass `isSelected = false` because they belong to no channel and must not wear the shown clip's
colour.

**Direction and end-action are lists, not values you nudge** — see `opensList`.
Their buttons carry a small chevron so a list announces itself. A tap opens it *inside its own
section*, over that section's controls — it cannot open anywhere else, because MotionComponent's GL
context composites above anything drawn over it, so a popup outside the bar would be invisible. The
backdrop is painted opaque before the card wash: `cardColour()` is translucent by design, and on its
own it left the list and the controls it covers drawn through each other. Picking an entry sends the
*difference* to `handleClipSettingsValueChange`, whose modulo arithmetic lands it exactly on the
entry tapped.

Every section's buttons sit on the bar's bottom edge — Shape's `len`, Elevation's `flat` and
`pole`, Motion's `dir` and `end` — so the bar reads as one row of buttons across its floor rather
than three sections each arranging their own.

The global section is laid out top to bottom (2026-09-26): the **elevation picture** — moved out of
the Elevation card into a grey field of its own (`elevationFrame`), never more than nine twentieths
of the strip's height, so it stands on every page, and touched it switches camera mode (below) —
then the **four channel faces** in a frame of their own, moved up out of
the clip's header row, then the **transport two by two** down to the
strip's foot, arranged like a clip's pads on PADS — play and stop over act and rec, rec taking the
corner the pads give to Settings. The 4x3 grid and the six function keys that stood there are gone.
The ACTION page used to line its rows up with the grid's (`setGridReference`); with the grid gone it
lays out freely, and `layOutActionPage` is handed an empty reference.

The header row reads **CLIP MOTION ACTION FILES CHMIX MAINMIX REC PADS**. (The MAINMIX key reads
**MIXER** since 2026-09-27 evening; the code keeps the name MainMix.) CHMIX is the shown channel's
strip (the MIX tab before). MAINMIX shows the big mixer over the sphere and is the lit tab while it
is up; a second tap takes it away again, and so does any other tab (`pageTabIsLit`). It stood in the
status bar as a MIX toggle until then.

**FILES lies over the sphere too** (2026-09-27), the same way: it is not a `BarPage` any more but
one value of `SphereOverlay` (`None`, `MainMix`, `Files`, `Pads`), and `showOverSphere()` is the
one place that shows any of them. One value, not a flag each, because they share one rectangle —
MAINMIX while the browser is up swaps it for the mixer, and so on. **The three keys toggle**
(2026-09-27): a tap on the lit key takes its overlay away, a tap on another swaps that one in
(`overlayAfterTap`), so only one is ever on. Any page tab, Back, Close and the Menu key take it away; the bar underneath stays
on the page it was on and its tab goes dark while the overlay is lit. It moved because a list of
seventy names showed a handful of rows in the clip area and shows most of a folder over the sphere.
Like the mixer it is opaque and keeps the band for back and close clear
(`layOutBrowserOverSphere`), and the side strips stay away while it is in front
(`sideStripsHaveAList`) — its edges are its own tabs and keys. Closing it ends a rename without
keeping it and disarms Delete, as leaving any mask does. The encoders follow the page underneath,
as they do under the mixer.

**PADS followed the same day**, for the same reasons: `BarPage::Controller` is gone, the pads lie
opaque over the sphere (`layOutControllerOverSphere` keeps the back/close band clear), and with it
went `pageDescribesAClip` — PADS was the one page that did not describe one clip, where a channel
face brought CLIP back. Every page left describes one, so a face now only selects (and turns over
the face you are on), whatever lies over the sphere.

**A face's 3D, FREQ and Q are columns a fingertip tall** (2026-10-07, #65). Each pot's touch area is
its whole column — the face's height above a strip that carries the clip's name over its progress —
and the ring is drawn in the middle of it, so a vertical drag anywhere in the column turns that pot.
The fingertip there is the **display's**, `displayFingertip()`: 9 mm from `Displays::Display::dpi`
over its `scale`, read in `MainWindow::resized()` (`useDisplayForFingertip`), 69 px on the device's
panel. Every other floor stays on `fingertipSize` (34 px, the same 9 mm at an unknown display's
96 dpi): at 69 px the mixer overlay has no room for four strips and FILES/MIXER/PADS overrun the
global strip, so moving the rest is a decision still to make. A tap on a column without movement is
a tap on the face (`PotKnob::onTapped`); landing on another channel's pot chooses that face first, as
reaching for a pot always did. Every `PotKnob` turns its whole range over four fingertips
(`fingertipsForTheWholeRange`), relative from where the finger lands — not four of its own heights,
which made a face pot's range 14 mm. A knob follows the first input source of a gesture only
(`FirstSourceOnly`): on the device X delivers every finger a second time as an emulated mouse.

**CLIP and MOTION** (2026-09-26). The clip area is three columns (`layOutClipSettings`), and which
card stands in them depends on the page. CLIP: Shape's card with the clip picker over the picture,
then a card with **dir** (Fwd Rev Bnce Rnd) over **end** (Loop Stop Paus), each one field that
steps on a tap (`tapAdvancesValue`) — for a day they were a key per choice, and the maintainer
wanted the toggles back — then the four lengths two by two (`lengthCard`). MOTION
(`BarPage::Motion`): Motion's eight knobs in two rows of four across the first two columns,
Elevation's four in the third. The sub-indices did not move: `directionButton`/`endActionButton`
are still Shape's 2 and 3, so the encoders step them as before. `controlIsOnPage` and
`cardOfControl` say where each control is shown and drawn; cards of different pages overlap by
design (Shape and Motion both start in the left column).

**REC** (`BarPage::Record`) is the take about to be made: Shape as CLIP shows it, beside one card
across the other two columns (`recordCard`) with the rec mode's key on top and **fade and bias**
under it. Fade and bias came out of Motion, which keeps eight knobs in four rows, but they keep
Motion's sub-indices 8 and 9 — the encoders, the take and `numControlsInSection` see no change;
only where they are drawn moved. `controlIsOnPage` says which control stands on which page, and
`ClipSettingsComponent::showControlsOfPage` hides the rest; `cardOfControl` names the card a
control is drawn in. The rec mode's key is still the global section's one sub-element.

**REC PAUSE** (2026-09-26). The bar's ● no longer starts a take outright: it arms the shown slot,
jumps to REC and lights ● and ▶ together (`TransportState::armed`); the take is set up there —
the length keys stand on REC too (`lengthKeysStandOn`), beside rec mode, fade and bias — and ▶
starts it on the next downbeat. ● or ■ while armed takes it back and writes nothing; showing
another slot drops it (`refreshRecArmed`). The clip on the slot keeps playing while armed. What
each key does is `RecArming.hh`'s, as pure functions with tests. The panel's REC + Play|Pause pad
still starts a take directly. **Play|Pause wears ▶ or ❚❚** (`TransportFace::Pause`) — ❚❚ while the
clip runs, ▶ otherwise and whenever armed — on the bar's key and on the pads page
(`ControllerComponent::setPadPlaying`).

Where the other five keys went: **CLOCK** leads the status bar, left of the tempo; **MENU** closes
it, beside CLEAN and KEYS (the on-screen keyboard), the three alike words (`StatusBarLayout`); **TAP** is a touch on the
beat display, taken on the finger's way down — the screen tap brings its own timestamp through
`handleScreenTap()`, since only the hardware's tap arrives with one. **REC** and **SHIFT** left the
screen: the transport's rec key records, and the Shift gestures need the panel now. Their state
stays in `functionKeyLook()`, because the panel's LEDs still show it.

MENU is exactly the key (`toggleGlobalSettings`), closing one level at a time.

The menu's config pages (Button LEDs, Pattern Folder) slice their keys out of
`config.json` and derive their rows from the JSON — a key added to the block
shows up there without anyone registering it. There was a **Network** page for
the OSC blocks until 2026-09-30; the addresses and ports live in the one truth
since, and a page of values nobody reads would have been a lie. `SkinEditorComponent` draws a **heading** wherever a row's group changes, and the group comes from
`theme/SkinGroups.hh` rather than from the path; row labels then show only their last segment, since
the heading has already said the rest.

It used to be the parent path, which meant the file's own nesting grouped the list. That works for
a config page, whose keys *are* shaped like what they mean, and badly for a skin: eighty-five
keys in alphabetical order put `background` and `surface` forty rows apart with the speaker light's
thirty-four in between, and scattered the twenty-one values that design a skin among the blocks that
tune a shader. Grouped by what a value *is* now — surfaces, text, states, channels, sphere, type,
touch, then the effects, each split small enough that a heading still means something. A path that
matches nothing keeps the old behaviour and is grouped by its parent, which is why the config pages
are untouched; the search walks *up* the path, so a group stated once for `accent` also holds for
`accent.r`. Headings are
rows in the display list (`_rows`) but not landing places: `browseRow()` and
`navigate()` step over them and they carry no hit areas. That display list is
also why `_index` is no longer `_actionRows + parameter`: headings belong to
neither, so the offset stopped being enough — use `browsedParameter()`.

A changed address only changes *this* side of the conversation: `beat` has to
match what the beat-analyzer sends, the channel addresses what A3 Core listens
for. A typo does not fail loudly — the app sends correctly to an address
nobody subscribes to. The reference for what the rest of the system expects is
`web/a3-doc/src/ressources/osc.md`.

#### The menu pages: scroll, select, open a mask

On every menu page — the main menu, the skin editor, the Network and other config pages — **a
value changes in a mask and nowhere else**. A drag scrolls, a tap selects, a double tap or Enter
opens the selected row. Asked for on 2026-09-17: *"kein edit ohne eingabemaske, das kollidiert mit
scroll."* The old model was the encoder's two levels laid out for a finger — tap a value to arm it,
drag it or the right strip to change it, let go to apply — and it put an edit one drag away from
every scroll.

What "open" does depends on the row:

| Row | Mask |
|---|---|
| main menu, a row with values (Skin, Sphere in Menu) | the list of its values replaces the rows (`GlobalSettingsComponent::openPicker`); the arrows walk it and the skin previews, a tap or Enter chooses, Escape or Back puts it back; in the Skin list (`PickerTap::previews`, #54) a tap previews and a second tap on the same skin keeps it, and closing the menu puts the running skin back |
| main menu, a row that leads somewhere | that page |
| a number or text | the typing mask and the bar keyboard; Enter keeps, Escape, Back and Close undo |
| a skin number | the same, plus **− / +** keys (and the arrows) that step it live — dialling while watching the sphere lives here now |
| a colour | the colour picker |
| Save, Rename, Delete, Reset | fires — only on a double tap or Enter, never on a tap |

**Undo restores the whole document**, not the one number (`_documentBeforeMask`). A skin that does
not state a value shows the theme's default in its row but reads as 0 from the document; putting
"the old number" back wrote a 0 the file never had, and closing the editor saves.

The strips left and right of an overlay's panel are drag zones, not margins, and **both scroll**.
`OverlaySideStrips` is one component for all overlays, a sibling of `OverlayButtons` under
`MotionComponent`, asking the open page for `panelBounds()` — which for the main menu changes size
while its list of values is open, so `updateOverlayButtons()` runs again then. The menu panel is
narrower for them: `globalSettingsSideZoneWidth` reserves a fifth of the width on each side.

**The list scrolls; it does not walk a selection.** A drag moves the page in the finger's direction,
the way it does on a phone, and a row is chosen by touching it. The window is its own value
(`_scrollTop`, `_pickerTop`) and `ListScroll.hh` holds the two rules — move by a drag, and move as
little as possible to bring a selection into view.

**A list follows the finger one row per row.** A list area sets its row height as its step
(`TouchControl::setPixelsPerStep`, the strips take it from the open page), and steps half way
through a row, so list and finger are never more than half a row apart. With the skin's 12 px step
a 34 px row ran three times faster than the hand; stepping only on whole rows left the first 33 px
of every drag dead. Two more things only a list needs: its row areas stay **visible on a heading**
(a drag keeps going to the area it began on only while that area is visible, and scrolling
re-labels them — the drag stopped half way, on the list and never beside it), and it takes **no
drag resume** (a finger coming down right after a scroll is picking a row, and was taken for more
of the drag).

**Two fingers scroll a list as one.** Two fingers land on two hit areas, or twice on one, and each
scrolled it — double speed, or a jump as the second restarted the first one's drag. Every scrollable
area of the menu pages and both strips share `FingerLatch::forGroup (menuList)`: the first finger
leads, others are ignored until it lifts. A leader whose page was hidden under it never sends its
mouseUp, so a new finger takes over when JUCE says the leader is no longer down. Controls meant to
be held two at a time — the channel grid's knobs — take no latch.

**Clockmode is not in this menu.** It is a button in the clip settings bar, visible and switchable
without opening anything — a setting in two places is a setting whose location you have to
remember.

#### The skin panel: sections, switches, bars

The menu's **Skin Editor** row opens `SkinPanelComponent`, not the list. Asked for on 2026-09-28:
*"der skineditor ist unübersichtlich. wir müssen stark parameter reduzieren und sinnvoll in
kategorien trennen … jede sektion benötigt als erstes eine option an/aus und für seine effekte auch
an/aus. der skineditor liegt komplett über der einzustellenden fläche. pack ihn nach links."*

- **Where.** A panel two fifths wide at the left edge of `MotionComponent`, full height, no scrim
  (`skinPanelBounds`). The sphere moves over into what is left
  (`MotionComponent::setSphereLeftInset`, `sphereRegion` in `SkinPanelLayout.hh`); where the
  picture with its towers is wider than that, the whole of it is drawn smaller
  (`speakerSceneReach`) rather than cut off at the edge — the speakers are three of the sections.
  It stays touchable: move a blob while tuning its corona.
- **What.** Six sections named after what a performer sees — Sphere, Background, Speaker tops,
  Speaker bass, Blob, Trajectory — plus *Text and size* for the fonts, pot size and bar height, which
  have no other home on the device. The table is `theme/SkinSections.cc`: per section a switch, its
  effects (each a switch and the one amount it is tuned by, on one row), a few more bars and a few
  colours. About forty of the skin's ~110 values; the rest are hidden, not removed.
- **Switches** live in their own block, `switches.<section>.on` and `switches.<section>.<effect>`.
  **Missing means on**, so every skin written before them draws exactly as it did (tested against
  every shipped skin). A switch never touches the value it governs: `withSkinSwitchesApplied()`
  writes each switched-off effect's "off" into a *copy* before anything renders — `loadTheme()`
  applies it itself, and `MotionComponent::applyVisualConfig()` does for what it reads straight off
  the var. The file keeps the number; on again brings it back.
- **How a value is set.** A `juce::Slider` in `LinearBar` style per row, relative and one to one
  with its own width (`setSliderSnapsToMousePosition (false)`, `setMouseDragSensitivity` = width in
  `resized()`), so landing on a bar to read it changes nothing. `−`/`+` beside it step a hundredth
  of the travel and repeat while held (`Button::setRepeatSpeed`). A double tap puts the bar back
  to what it held when the panel opened (`setDoubleClickReturnValue`). Ranges are skewed
  (`NormalisableRange::setSkewForCentre`) so what ships sits near the middle. The encoders were
  left alone: they are freq and Q whatever is on screen.
- **The old list** is one level further in, behind the footer (*All values and skin actions*):
  every value, typing, Save as new / Rename / Delete / Reset. It edits the same document and hands
  it back when Back leaves it. Config pages (Network) still use it directly.
- **Back** closes one level: picker → list → panel → menu. The panel saves on close, like the list.
- `sphereGrid` is new: the graticule was compiled in at 0.08; it is a skin value (default 1) so
  the Sphere section's *Grid* switch has something to switch.

#### The bar's two pages

The bar shows one of two pages (`BarPage`): the shown clip's settings, or **the panel's pads**.
Tabs close the header row and switch them; the header row and the global strip on the right stand
on both, because recmode, clock, MENU, REC, TAP and SHIFT belong to the device rather than to the
clip and losing them while firing clips is the wrong moment to lose them. The **readout sits over the
global strip**, in the header row's band and on its line: what it reports comes from either page, so
it belongs beside the part that stands on both. In that band rather than inside the strip's card,
because a row taken there comes out of the channel grid, whose cells collapsed to five pixels at the
smallest skin sizes.

The controller page exists because **a plain build has no panel** — `HARDWARE_INTERFACE_ENABLED`
is off by default — and without pads such a build cannot start a single clip. It decides nothing
of its own: a press goes out as `(channel, pad)` into the same `handlePadPress()` the hardware
reaches, and a pad's colour comes in already worked out by `padLEDCallback()`, the one loop that
also writes the panel's LEDs. Empty, idle, armed and running therefore look on screen exactly as
they look on the hardware, because one place decides what they mean.

Its geometry is `ControllerLayout` — **the panel on screen, in its own proportions** (2026-09-28):
a grid of square cells, six rows as the panel has (`InputOutputAdapterV3.hh`: the function keys in
col0/col9 rows 0–5, the pads in rows 2–5), sized by whichever direction runs out first and centred
in the rest. Filling both directions is what had stretched the pads tall. Each channel's **eight
pads stand in the panel's own arrangement: two columns of four, Play|Pause top left, PAGE top
right, A1–A6 below** (A1 A2 / A3 A4 / A5 A6). A pad's identity comes from `padFunctionByPadIndex` /
`actionButtonForPad` in `io/PadFunctions.hh`, the same tables the panel is read with — those say
which pad is which function; the arrangement on screen was read off the device, because only the
hardware says where a function sits under a hand (taken from the pad-index order once, it had
action and stop swapped). `fingertipSize` is the floor for anything hit in a hurry; over the
sphere on the device a cell comes out at about 65 px. Rows 0–1 over the pads stay empty: on the
panel the pots stand there.

**The panel's function keys stand on the page too** (2026-09-28): the right-hand column (col9)
with all six, and over the scene block, at the outer edge, the top two rows of the left column
(col0) — TAP and clock. The scene block is the screen's own and stands where col0's rows 2–5 would
be. Where each key stands is one table, `panelKeyPlaces` (side and panel row); what it does comes
from its row through `functionKeyOrder`, as on the panel. A key goes down on touch and up on
release, and lands in `setFunctionKey (key, KeySource::Screen, …)` — the same route the panel's
Values take (`KeySource::Panel`) into `functionKeyChanged()`, the one place that says what a key
does. `FunctionKeyHold` keeps both sources: a key is down while either holds it, and it means
something only when that combined state changes, so SHIFT and REC held on the screen modify a pad
exactly as held on the panel, and `isButtonPressed()` reads the same state (also in a build with no
adapter). The keys are painted from the look the LEDs are written from (`setFunctionKeyLook()` in
`updateFunctionKeyLEDs()`): the word in `functionKeyColour()`, the ground washed while
`functionKeyLit()`.

**A scene block stands left of the channels**, shaped like a channel: `scenes[0][pad]` fires that pad
on every channel — Play all, each action on every channel that has it — and in PAGE's place
**Stop all** (`stopChannel()` per channel), because the panel has no Stop pad any more and Page
across four channels would only step the shown one's pages. Everything else goes through
`handlePadPress()` per channel — one route to what a pad means. **A scene's Play starts only the
clips that stand still** (`sceneStartsClip()`); a single Play pad toggles, but a scene that toggled
would start half the room and stop the other half. Screen only: the panel has no such pads.

**What a pad shows is one rule, `padShadeStatus()`** (`theme/PadStatusColours`), read by the panel's
LEDs and this page alike. Only Play|Pause follows its clip's status — six action pads blinking with
the clip's schedule would say nothing Play does not already say. An action pad is at the idle shade
when its button carries an action and at the empty shade when it does not; it goes **white**
(`padBaseColour()`) for exactly as long as its action runs — rise, hold and fall — lit from
`isChannelAccentActive()` and `_actionSlot`, which remembers which button fired the channel's
accent. PAGE is full on the channel the screen shows. A pad under a finger runs towards the skin's
text colour for as long as it is held.

**PAGE goes to the clip it names**: on another channel it selects it; on the shown channel it steps
the bar's pages (`nextClipPage()`, Shift backwards), and it closes FILES/MIXER/PADS first, because
PAGE is about the clip and the overlay covers it.

**The panel's six function keys** are listed once: `io/FunctionKeys.hh` holds `functionKeyOrder`
(`TAP, clock, REC, recmode, MENU, SHIFT`), and the panel is wired from it row by row. The screen
carried the same six as two columns of three in the global strip until 2026-09-26; they are spread
over the status bar and the REC page now (see above).

On the panel those keys are a **vertical column of six at each end** (col0 and col9, rows 0–5),
mirrored so either hand reaches them. The two columns are one set of keys, not twelve: a key is down
while *either* side is down, and both sides light together. Menu alone used to be tracked that way —
which meant holding the left Tap and pressing the right one read as a release. `functionRowHwIndices`
maps a row to its two firmware indices; what a row *does* is not written there.

`Button` is now an alias for `FunctionKey`, and two keys reached the panel for the first time with
this: **clock** and **recmode**, in rows that had been spare. Both cycle on press, the same cycle
their screen twins run, because the panel and the screen are two places to reach one function. All six are one size: a button sized differently
from its neighbours reads as a different kind of thing, and these are all the same kind.

**What a key looks like is one rule** — `theme/FunctionKeyColours.hh` — and it drives both displays,
which only works if both of them read it. The bar carried its own copy for REC (`recording ? danger
: warning`) and went on saying orange long after the rule said red always; nothing was wrong
anywhere, the screen simply was not asking. Every key in the strip is painted from
`functionKeyColour()` now.

A colour is not said the same way in both media. `ledColour()` raises the saturation of anything
going to the panel to a floor, keeping hue and brightness: `accent` at rgb(144, 238, 144) is plainly
a light green on a dark bar, and on an LED — which has no surround, because it *is* the light — the
same value arrives as white with a tint. A colour with no hue is left alone, so a key meant to be
white stays white.

The strip washes the colour into a button face, the panel lights the key outright. What differs is
not *which* colour but how loudly it is said, because an LED in a dark booth is about as loud at full
as a wash is on a lit screen. Button LEDs therefore carry a **colour** now, the way pad LEDs always
did; `outputButtonLED` used to look one up by the key's *name* out of the user config, which meant
the panel and the screen could disagree about what a key was doing and nothing would say so. A
transparent colour is the resting light, which the adapter fills in.

Three keys carry a colour rather than a word for their state:

- **`clock`** writes its mode in that mode's colour, through `Colours::clockMode()` — the same rule
  the status bar reads it by, because whose tempo this is has one answer and it should not be
  written in two colours.
- **`REC`** is red, always — red lettering on the bar's own grey while it waits, and a red face
  while it runs. The colour says what the key *is* and the ground says what it is *doing*: two
  questions, two places, rather than one colour asked to answer both. It is the one key coloured
  while nothing is happening, because recording writes over something you cannot get back and you
  should never have to check.
- **`recmode`** carries how much of an old take a pass will destroy (`recModeColour`): the accent
  for Touch, which mends a corner and leaves the rest; the warning for Latch, which holds on after
  the finger goes; danger for Write, which clears the pass whether you touched it or not. That is a
  scale, so it is said on the scale the rest of the device already uses.
- **`TAP`** breathes with the beat, on the screen and on the panel, from the tempo clock's Beat
  handler — and this is the one place the two media are deliberately unequal: the panel gets the
  key's colour at full, the screen a *colourless* wash, because a coloured flash on a lit screen at
  every single beat is exactly the loudness that had this removed once. It is a wash laid over the
  finished button (`beatWash`), not the button's own "active" look — routed through that it more than doubled the key's brightness, which is a blink you watch
  instead of one you catch out of the corner of an eye. It had been removed once for exactly that.
  A press owns the key while it lasts: `pulseTapOnBeat()` returns early when `_tapLit`, or a beat
  landing under the finger would cut the press's flash short.

The status bar shows **the tempo and nothing else** — `BPM 60.0`, in the clock's colour. Which clock
it is comes from the clock key, on the screen and under the hand; a third place saying it was a third
place to keep in step. That readout also had three writers, one of which set the text without the
colour, so what you got depended on which arrived last. One writer now.

**The CLEAN key** (left of the keyboard icon) switches to the skin `config/skins/clean.json` and back to the one it
left — trajectories as a thin line, plain blobs, a faint glow in the speakers, every other effect at
0. A skin rather than a layer of switches over the skins, because a skin can already turn each of
those down to nothing; what the key adds is only the way back (`theme/CleanSkin.hh`,
`toggleCleanSkin()`). The skin to return to is kept in `ui_state.json` as `skinBeforeClean`, so a
restart in clean still has a way out; gone since (renamed, deleted), it falls back to `default`. No
`clean.json` on the device greys the key out. It lights from what `config.json` says is running,
refreshed in `applyTheme()`, which every way a skin comes into force passes through. `clean.json` is
a copy of `default.json` with the effect values changed, so the colours stay the same. The tops'
bolts and the subs' ball lightning have their own Skin Editor headings, **Topspeaker FX**
(`speakerLight.bolt*`, `topGlow`) and **Kickbass FX** (`speakerLight.ball*`, `subGlow`); the balls
used to land in Other.

What clean shows instead of the bolts, found in the smoke test: `beamIntensity` *is* the bolts —
the band round the sphere is made of them — so at 0.4 it had dimmed the bolts rather than left a
glow. The only glow there was sat in the horns and ports (`topGlow` 1.7, `subGlow` 0.5, compiled in
until then), on the baffle — and from overhead the baffle is edge-on, so it was never seen.
`speakerLight.boxGlow` lights the whole cabinet with its level instead; 0 in every older skin.
The corona that says which blob is playing is the existing one, `blob.sizeMin..sizeMax` over the
level; clean raises `sizeMax` so a loud blob's reaches past its body and a silent one's does not.

**A channel's VU meter is its VOL**, on both mixer pages (the overlay's four strips and the bar's CHMIX
tab), and the VOL knob is gone from both: `mixerFaceOrder` lists what a page lays out, while
`mixerControlOrder` still counts the state and the OSC wire, which carry VOL as before. A drag on
the meter is relative and one to one (`vuMeterDragVolume()`): it starts from where VOL stood when
the finger came down, so landing low on a playing channel pulls nothing down, and the meter's full
height is VOL's full travel, stepless, so the mark stays under the finger — the knobs' 2 %-per-12 px
steps made the tall overlay meter move in visible jumps. A tap does nothing; **two taps put the
channel at full volume** (`onMeterDoubleTapped`), and a double tap there survives a wobbling finger
as long as both touches are short (`DoubleTap.hh`, `DoubleTapMovement::MayMove`), so a drag picked
straight back up cannot throw the channel to full. The meter carries VOL's setting as a **fader handle** in the
channel's colour (`vuFaderHandle()` / `paintVuFaderHandle()`): a cap with a groove across it, opaque
so the bands do not shine through, at least half a fingertip thick because it is grasped rather
than aimed at. It stands at the travel, linear, not on the meter's dB scale.

**The master is laid out like a channel**: the room's output meters -- since 2026-09-30 ten, the
main sub and main tops 1-9 as the channel map sends them (`numOutputMeters`, held equal to
`VuRouting`'s `numMasterColumnMeters`) -- stand in
the column on its left, running its whole height, and BTH, MIX, PHN and RET stand beside them in
the bottom four rows, on the channels' own lines (`masterFaceOrder`, `rowForMasterPot`). The whole column is the master's
fader: a groove down it with the handle on it in `textPrimary`, dragged one to one like a channel's,
and the meters stand in its **foot** -- a quarter of the column (`outputBarsOfBlock`), turned a
quarter themselves (`VuDirection::Right`) so they swing left to right, stacked with the subwoofer
at the bottom where it stands in the room. The MST knob is gone. **No double tap there**: full volume on the master is
the one gesture that makes the whole room loud at once. The meters get a column rather than a row
because there will be more of them than five.

**The filter stands in the master's column, and each channel carries its 3D, FREQ and Q**
(2026-09-26). The row across the overlay's foot is gone: FX FREQ and FX RES stand under RET, FX
MODE on the channels' key line (`filterPotsInOut`, `rowForMasterPot`). The height went to the
channels, which gained three rows under SEND for the engine's channel pots (`ChannelPot`,
`channelPotOrder`, `rowForChannelPot`); the bar's CHMIX tab carries the same three as a second row
under GAIN, HIGH and MID, and its keys shrank to a third of the height. These are the engine's
values, not `MixerState`'s -- they reach Core through the spat backend, not the desk's wire -- so
both pages are handed them (`setChannelPots`, from `refreshChannelValues`) with the envelope's
effective value beside the setting, and draw the envelope as the arc above the setting
(`channelPotReach`), the way the bar's 4x3 grid did. A knob is set outright
(`setChannelPotValue`); two taps put 3D and FREQ back to 0.5 and Q to 0 through that same call
(`resetChannelPot`, `channelPotRestPosition`), so a reset is a turn: the same OSC on the next tick,
the same save, the same redraw. **Also while a panel answers** (since 2026-10-06): the reset used to
be refused then, because the absolute 3d pot would disagree with the screen — but a drag on the same
knob was never refused, so the screen could move a value and not put it back. The 3d pot disagrees
after a reset as it does after a drag, until it is next moved; freq and Q sit on endless encoders.

The overlay meter takes two fifths of its strip (`meterWidthOfStrip`), not the half asked for: the
strip ends in PFL and FX side by side, and at half a key came out at 32 px on the device, under a
fingertip. The column break is derived from the same two keys (`minimumMixerStripWidth`), so a
narrower window breaks the strips two by two rather than showing the "no room" sentence. The
master's output meters stand in the top row of its column, its five controls under them with MST
on the channels' key row. Checked on the device with `smoke-test/scripts/check_vu_drag_volume.sh`
(drags down and back only — a running instance sends VOL to the live Core; the double tap is not
exercised there, since it would send a live channel to full).

**`Stop` and `Pause` are two different end actions**, and used to be one under the wrong name. What
was called Stop stood still wherever the playhead happened to land — that is a pause, and calling it
a stop left no way to ask for the other one. `Stop` now returns to the beginning of the take,
whichever way it was running, so the next start is visibly a start; `Paus` is the old behaviour,
correctly named. The end-action list's length lives in one place (`numEndActions`) because it was
written as a literal `4` in three.

**Direction and end are two axes** (2026-09-26). `PlayDirection` is how a clip travels — **Fwd,
Rev, Bnce, Rnd** — and `EndAction` what it does when that travel is over — **Loop, Stop, Paus, Clip** —
and any direction combines with any end. Bounce and Random were end actions before, which made them
exclusive with stopping. A bounce's travel is its whole round: the far end only turns it, and the
end action, and a Play press asking it to finish (`stopAtEnd`), apply when it is home. A random lap
starts at a random phase (`initialPosition`, the first lap too) and runs to the end of the pass.
Only Fwd/Rev with Loop travel the step from the last tick to the first (`travelsTheWrap`), which
decides whether the fade joins it. Files, sessions and scripts that named bounce or random as the
end are read as that direction, looping (`playbackModeFromNames`, and the script's `~end` setter),
which is how they played. The bar's index for both is the enum itself, in the captions' order.

**`Clip` hands the channel to another clip** (2026-09-28) — Ableton's follow action, for chains
like a Build clip giving way to its Peak after sixteen bars. A clip whose end is Clip names its
follow by file name (`"endAction": "clip", "endClip": "Peak"`; a set slot may carry its own
`endClip`, and none is written when there is none). The name lives on the `Pattern` and in `Clip`,
**not in `ClipSettings`**: those are copied on the clock thread when an action fires, and a string
copy there is an allocation there. `applyClipValues()` is the one call that puts a clip's settings,
lanes and follow on a pattern.

The hand-over is **on the tick the pass ends**, the tick a looping clip would go back to its top on
— not the next downbeat after it. That is only possible because nothing is loaded then: the message
thread keeps each channel's follow built and armed (`armFollowClips()`, every frame, cheap when
nothing changed — an action's `~end = \clip` reaches the clip on the clock thread, where nobody
could have told the UI), and `MotionEngine::armFollowPattern()` hands it over for *that* clip alone.
On the end tick `performPlayback()` starts the follow through the same `beginPass()` a start uses and
writes its first position in the same tick, then posts `Playing`; the UI answers with
`putPatternInChannel()`, so the channel row, the set and the CLIP page follow. The follow's own
follow is armed on the next frame, which is what makes a chain.

What does not follow: no follow armed, or an unknown name (armed as nothing) — the pass ends as
**Stop** ends it, and the END field says `→ Stop`; a Play press asking the clip to finish
(`stopAtEnd`); a stop or a take scheduled on the channel; a take ending (end actions are playback's,
and a channel holding an unsaved take arms nothing, since the follow would replace it). The
**accent** running out leaves Clip running like Loop: the chain belongs to the bar grid the pass
ends on, and an accent ends wherever a finger let go.

On the CLIP page END steps Loop → Stop → Paus → Clip on a **tap**; a **drag** on it while it says
Clip walks the follow through the clips, the same walk the clip field makes. The tap therefore
arrives through `tapTogglesValue()` rather than as a drag's first step. `loadClipIntoChannel()` is
the one route by which a clip file becomes what a channel plays — FILES' Load goes through it via
`applyClip()`, and the Cue actions are meant to.

**When a pad takes effect** is a set, not four separate decisions:

| Pad | When |
|---|---|
| PlayPause | the **next beat**, starting and stopping alike (`TempoClock::nextBeat()`) |
| Stop | **now** |
| Action | **now** — the instant start beside PlayPause's quantised one |
| Shift+Action | now, in preview mode, for as long as it is held |

The bar is the take's unit — a recording is a whole number of bars — but it is the wrong unit for a
press: a bar is up to a metre's worth of beats away, and a clip that starts that long after the
finger reads as a button that did not work. The beat is close enough to feel immediate and still
lands in time. Stop is the way out of something going wrong and a way out that waits for the music
is not one, so it is unquantised; Action is the same escape hatch for starting. Quantised by
default with an instant variant beside it is what a deck offers, and it is the pairing that matters
rather than either half.

The page is handed `ClipSettingsLayout::clipContent` — the clip part **under** its header row — not
the whole clip part. It used to work the header's height out for itself from the font, which came
out eleven pixels short of the bar's own arithmetic and drew the top row of pads under the tabs
that switch to it. One place says where the content begins.

Two things this cost, both worth knowing before touching it:

- **The page is a child of `ClipSettingsComponent`, not a sibling.** The bar fills its whole area
  with `surface` at `panelOpacity` (0.85), so a sibling underneath came through at fifteen percent
  of itself — the page whose job is showing which clip is running, showing it in the dark. A child
  is painted after its parent by construction and no `toFront()` can undo that.
- **`TouchControl` has two release callbacks and they are not synonyms.** `onDragEnd` fires only
  after a drag; `onRelease` fires whenever the finger comes up. The modifiers and the pads need the
  second, because Shift+Action previews for as long as it is held — with only `onDragEnd` a press
  that never moved was never released, and the channel previewed forever.

`SHIFT` is **held, not latched** — Shift+Action previews for as long as it is down, so a latch would
have nothing to release. On screen it stands on the PADS page with the other five keys (see above),
beside the pads it modifies: a modifier you have to change pages to reach is one you cannot hold
while pressing what it modifies.

#### One clip per channel, six action buttons

Decided 2026-09-27 (plan: `.claude/notes/a3-motion-one-clip-per-channel.md` in the workspace).
A channel holds **one clip** and **six action buttons**; the two slots with one action each are
gone from the panel and the screen. Internally the slot dimension still exists with size one
(`numPadSlots = numClipSlots = 1`) until it is collapsed; every slot index is 0.

**The pads** (`io/PadFunctions.hh`): `PadFunction` is `PlayPause`, `Page`, `Action`;
`padFunctionByPadIndex` and `actionButtonForPad` say which of the eight is which, and
`padIndexFor()` / `padIndexForAction()` go the other way — the screen's PLAY and ACT keys and the
ACTION page's fields reach `handlePadPress()` through them, so there is one route to what a pad
means. STOP on the screen is `stopChannel()`: the panel has none.

**Play|Pause on a running clip pauses** (`MotionEngine::pausePattern`, since 2026-10-08) on the next
downbeat, and the next start goes on from where it stood (`Pattern::resumesOnPlay`); a paused ▶
blinks slowly. **SHIFT means now and from the top** (`playPausePress()` in `PlayPausePress.hh`): on a
running clip, or a pause still waiting, it stops and goes back to the top -- the panel's only way
there -- and on a still clip it starts from the top at once, a pause forgotten. ■ (`stopPattern`)
goes back to the top as well, a paused clip and a pending pause included (`stopReachesClip()`).

A pause on the downbeat keeps the place as it is. One made off the downbeat -- the engine allows it,
no key asks for it since SHIFT stops instead -- goes back by the ticks since the music's last
downbeat (`MotionEngine::rewindToTheBar`; `rewoundPlayhead()` / `rewoundLapTick()` in
`Playhead.hh`, `rewoundLfoPhase()`), place, lap and slow movements together, so a resume on a
downbeat plays that bar again. Counted in ticks rather than snapped to the clip's bars: a pass that
is not whole bars, Reverse, Bounce (home is the start of the outward leg) and a long pass of
drifting float steps all come back exactly. A clip started inside the bar, or a pass that ends and
would have to go back before its start, starts from the top instead.

**A button is `ActionButton`** — file, source, dice seed, errors, and its **feel** (`ActionFeel`:
the three envelopes and the act mode). The feel is what the script says (`actionFeelFrom` in
`runButtonScript`); since 2026-09-29 the ACTION page's knobs write it into the script, not onto the
button (see **ACTION writes into the script** below). With one feel per clip, six buttons would all
have felt the same, and assigning a script used to *write* its envelope onto the clip, so the last
one assigned won.

**A script is worked out at the press** (`firedActionOf()` → `resolveActionAt()`), against the clip
as it stands then, with the seed the button rolled when it was assigned. It used to be worked out
once, at assignment, and the whole result kept — every field of the old clip — so after a new clip
or a turned knob a press threw the channel back for the length of the accent. During a running
accent the pattern already wears the first action, so `_accentBase` keeps the clip's settings from
before it and a second press is resolved against those. The engine side
(`applyAccentHeld`): the latest action goes over the **original** restore point, so the clip comes
home to itself, not to the first action.

**The ACTION page** (`ActionLayout`, `ActionComponent`): left to right, the six fields (3×2, as on
the panel), the list the chosen button is assigned from, a key column (EDIT, the mode, *then*,
AUDIO, MOTION), and the card with the chosen button's tile. Since 2026-09-28 **a field only
chooses** its button (`onButtonChosen`); everything else on the page shows the chosen one, and the
card is outlined white while its action runs. Firing is the pads' job: a push on an action pad of
the panel or the PADS page fires as before and then brings up the ACTION page of that channel with
the pushed button chosen (`showPushedAction`, `actionPressShowsItsPage`) — not with Shift (a
preview is auditioned from wherever the hand is, FILES most of all), not from the bar's ACT key, a
scene or a chain, and not away from a take being armed or recorded. Each field carries a **1/H
badge** for its own button's mode (`actionFieldParts`). The encoders (maintainer, 2026-09-28): enc 1 `ActionButton` chooses A1–A6;
enc 2 `ActionList` walks a highlight through the list and a press assigns it (`moveListCursor`/
`chooseListCursor` — walking is not assigning, so turning past forty scripts mid-set changes
nothing); enc 3 `ActionKey` rings EDIT / mode / then and a press does what a tap on the ringed key
does (`moveKeyRing`/`pressKeyRing`, through the tap's own callbacks); enc 4 `ActionTile` switches
AUDIO/MOTION, which are tabs along the card's top. enc 5–8 `ActionValue` turn the four values of
the card's marked row left to right (AUDIO: atk/dec/max, enc 8 idle; MOTION: four across, five
rows, `ActionLayout::motionColumns`), and a press on any of them marks the next row and comes
round (`stepValueRow`) — the MOTION page's "a press switches the row", on a card of nine or
nineteen values. Another tile starts at its first row. The ring and the row mark are drawn only
once an encoder has been used. The list highlight resets only when the name or the chosen button
really changes — the page is told the name on every update. SHIFT keeps FREQ/Q on all eight. `_chosenActionButton[ch]`
is also what the screen's ACT fires and what FILES' Load on ACTIONS assigns to.

**Two tiles, one card** (2026-09-28). AUDIO is the nine feel knobs. MOTION is every knob of the
MOTION page and CLIP's speed, direction and end, for what the button puts on the clip:
`motionParamOrder` lays them out four across, MOTION's eight fields two to a row, then speed, dir
and end. Each value is shown as it will land (`motionShownFor`, with no overrides since
2026-09-29): grey where the script leaves it to the clip (the clip's own value, as a hint), the
channel's colour where the script sets it (`ActionScriptResult::assigned`). A turn writes the line
into the script; two taps comment it out again. The third look ("turned on the button", a wash) is
no longer reached — `MotionSource::Button` stays in the component for now. A button without an
action shows "no action" instead of the tile.

**Then** (2026-09-28): each button can name another of its channel's buttons to fire when its
accent is over — the script's `~then = N;` (1..6), read into `ActionButton::after`; the key under
the mode steps --, A1..A6 and writes the line, two taps comment it out. The engine only counts accents that end (`MotionEngine::accentEndCount`, on the edge
where the clip is given back); `ActionChain` decides on the message thread, once per end, from the
button that ran the accent. A chained button plays as a one-shot — no finger holds it. Chains may
loop; another action press, Play|Pause or Stop on the channel ends one.

**ACTION writes into the script** (maintainer, 2026-09-29: "what you see is what you get"). Every
value set on the page — the nine AUDIO knobs, the mode, "then", the nineteen MOTION values — is a
line edit on the chosen button's script (`setScriptLine`/`unsetScriptLine` in `ScriptLine.hh`):
the one `~name` line changes, comments and every other line stay; a commented line is uncommented,
a missing one added under its section, a dice expression becomes the number. A double tap on a
MOTION value or on "then" comments the line out ("as the clip is" / nothing after). The edit is
in place and reaches every button on every channel holding that file (`editShownScript` →
`buttonsHoldingFile` → `runButtonScript`). **A shipped script with developer mode off is not
written** (maintainer, 2026-10-08, the rule FILES keeps for Save): the first turn writes a copy
into `actions/user/`, named after it ("Bloom 2", `scriptFileToWrite` in `ActionEditing.hh`), every
button that held the factory script holds the copy from then on, and the FILES editor showing it
moves to the copy; the set names the copy once it is saved. With developer mode on, shipped scripts
are written in place as before; the FILES editor showing it takes
the same line change even while it holds unsaved typing (`ScriptPanel::applyEdit`). The file is
written ~300 ms after the last change (`PendingScriptWrites`, through `writeTextFile` — JUCE's
`replaceWithText` would write CRLF), flushed before a set load, before FILES reads a file, and on
quit; a failed write says `-- CANNOT WRITE <NAME>`.

**Sets** keep `"slots"` with one entry, so an older build still reads a new set, and add
`"actions"` (six entries, written only if one is non-empty). An entry holds `"script"` and nothing
else since 2026-09-29; a set's older `"feel"`, `"motion"` and `"after"` are ignored on load. A set
from before maps its two slot
actions to A1/A2. On the first start of the new build `migrateTwoSlotSets()` copies every two-slot
set to `pattern/backup-two-slots/` once — the debounced save and `renameInSets()` would otherwise
truncate them all within a minute.

**A Cue loads a clip** (library v2, 2026-09-28):
- **Script:** a line `~clip = "Name";` makes the button a Cue. It is the only place the language
  takes text.
- **Assignment:** `cueClipFor()` resolves the name when the script is put on the button. An
  unknown name is an error in the strip, and the button does nothing.
- **Press:** the clip goes onto the channel through `loadClipIntoChannel()`, the one route FILES
  Load takes too (`applyClip` calls it and then starts what stopped). It starts on the next
  downbeat, or at once with Shift.
- **No accent:** a Cue changes what plays, not how it plays. It does nothing while a take is going
  in on that channel.

**The shipped library** is 50 shapes, 50 clips (plus the shapeless `Default` fallback), 50 actions
and 10 sets:
- **Names:**
  - shapes are `<Family> <Name>`: Rhythm, Orbit, Loop, Spiral, Flower, Cycle, Edge, Wander;
  - clips are `<Phase> <Name>` and sets are the phases of a night: Warmup, Groove, Build, Peak,
    Drop, Break, Dub, Deep, Float, Closing;
  - actions are `<Kind> <Name>`: Move, Lift, Width, Speed, Dub, FX, Cue.
- **Only FX changes the sound:** every other action writes all three ceilings as 0, and
  `ShippedLibrary.OnlyFXChangesTheSound` holds it.
- **Every set's buttons stand the same way:** A5 is the FX and A6 the Cue into the next phase.
- **Every clip says what it is for:** a `mood` line in its file, like an action's `// Mood:`
  line (`ShippedLibrary.EveryClipSaysWhatItIsFor`). It is on `Clip`, so a device save keeps it;
  a3-doc's clip table is rendered from it by a3-core `tools/render_library.py`.
- **The content is generated:** by `tools/library-v2/{clips,actions,sets}.py` from the tables in
  the spec, and the shapes by `a3-pattern-gen`, whose table is the source of their names.

**The build fails on a forgotten enum case** (`-Werror=switch` on the UI target). The rewrite of
`handlePadPress()` dropped the whole Play|Pause case; gcc said so with `-Wswitch`, and the line
drowned in the float-equal warnings. Do not silence it with a `default:`.

#### Every file beside its list: `ScriptPanel` in FILES

**Where it stands (2026-09-27).** The editor left the ACTION page and stands in FILES beside the list
-- on every tab: sets, clips and shapes are read and edited as text like the actions (JSON, and SVG
coloured as XML, `languageFor`). `ScriptPanel` (`components/ScriptPanel.{hh,cc}`) is a plain text
editor with an error strip and four keys; `BrowserComponent` places it in `BrowserLayout::detailArea`,
as wide as a shipped action's longest line needs (`ScriptPanel::usualWidthFor`, measured the way
JUCE's editor measures itself -- a character is "0", the gutter a fixed 35 px, less the empty room
left of the numbers that the panel's frame cuts off). The text reads at the list's size; a line
longer than the column scrolls. The list keeps the
rest: its own keys (Load on SETS, All, Rename, Delete) at the top in one row with the panel's -- where
back and close stood, which now appear only on the main menu (`overlayKeysAreShown`) -- and the four
folders two by two above the rows.

What a file means stays each list's (`LibraryList`: `fileAt`, `folder`, `extension`,
`currentStateText`, `afterSaving`, `afterCopying`), and every decision stays in
`A3MotionUIComponent`:

- the panel holds the file it was loaded from (`_panelFile`); when the list's chosen row moves under
  it, it reloads, or -- holding unsaved text -- the row goes back to it (`panelSyncFor`,
  `syncFilePanel`), so one file's text is never saved into another;
- a tap on any tab **chooses and shows** and loads nothing; **Load** (on every tab) puts the row on
  the shown slot -- its clip, its figure, its action (`LibraryList::assign`) -- or loads the set.
  The drift dot stays on the row of the slot's own clip, not on the chosen one;
- **Save** writes the file and what uses it takes it up now (`afterSaving`): every clip firing an
  action (`slotsFiring`), every slot holding a clip (`applyClip`) or a figure (`putFigureInSlot`,
  a playing one plays on from the next beat); a set is only written -- loading stays Load's;
- **Save as** writes a copy into the user half, named after the original (`freeFileIn`,
  `copyBaseFor`) -- on SETS after the set that is loaded, "Tribal 2" (2026-10-08), whatever the
  editor shows; on ACTIONS the clip EDIT came from fires it, once (`takeEditOrigin`);
- **FROM** ("from clip", on SETS "from set") puts the current state in as the file's text,
  unsaved, written by the same writers Save used to call straight into the file
  (`currentStateText` through a temporary file) -- so there is one pair of Save keys, and you see
  what is kept before it is kept;
- a set or SVG that does not parse is **not written** (`fileErrorsOf`, `errorsBlockSaving`); a script
  with an error still is, as before; the error stands in the strip;
- **unsaved text holds the list and the tabs**: a row tap, Rename, Delete or another tab say
  `-- SAVE OR CANCEL` and flash the two keys (`listWaitsFor`, `fileTextHoldsTheList`).

The rules are pure functions in `ActionEditing.hh`, `ScriptPanelLayout.hh` (`scriptKeysFor`) and
`BrowserLayout`, tested there; the panel lights and guards its keys by the same `scriptKeysFor`, so a
dark key is also a dead one. JUCE's code editor calls itself opaque but is drawn transparent; the
panel says so (`setOpaque (false)`), or a scroll over the sphere let the trajectory through.

**Four keys over it: from clip, cancel, save, save as**, equal width, in that order. Save writes
the editor's text over the chosen file; **it stays dark on one of the instrument's own
while developer mode is off**, because writing over a shipped script takes it from every clip that
fires it with no way back. Save as is the way out of exactly that: it writes the text to a new file
in `user/`, **named after the one it came from** — "Bloom 2" beside "Bloom", counted against both
halves. Cancel puts the file's own text back. Save, Save as and Cancel are lit only while something
has been typed; FROM CLIP whenever the shown slot holds a clip.

**That lock is one rule for clips and scripts alike** — `shippedFileMayBeOverwritten (fileExists,
fileIsShipped, shippedClips ())`, which is why its name no longer says clip. The page is told the
answer (`ScriptPanel::setProtected`), not the ingredients, and it is told again whenever developer mode
is switched, or the key would stay dark and make the switch look broken. Two rules for one question
is how they come to differ, and the difference then has to be explained on a screen with no room to
explain it. Whether a *script* is shipped is `isSystemFileIn()`, asked of the file rather than
remembered; whether a *clip* is, is `slotClipIsShipped()`.

Developer mode is what let eleven shipped clips be written over on 2026-09-22 — the lock worked, it
was simply unlocked (`config/ui_state.json`, `"developerMode": true`).

The save point is set by the host once the file is written (`markSaved`), not by the key: a write
that fails leaves the edge marked, and `setScript()` returns early on text the document already
holds, so nothing else would clear it.

**The list and the editor no longer share an area.** Until 2026-09-27 they stood in one field on
ACTION and took turns (`updateScriptLayers()`), because the editor, a child with a transparent
ground, was painted over the list however opaque the list made itself. Side by side in FILES, and
with ACTION's list standing open on its own, that trap is gone rather than worked around.

**Every script names every parameter, and comments out what it does not touch** (asked for on
2026-09-23: *„alle Action skripte alle parameter enthalten. wo nichts passieren soll bitte
auskommentieren"*). A commented line assigns nothing, which is already what "leave this as the hand
left it" means — so the convention costs the language nothing and makes each script its own
reference: the range and half a line of what a name does stand on the line, not in `README.scd`.

That text lives once, in `actionScriptNotes()` beside the reader, and two tests hold the twenty-six
shipped scripts against it — one that every parameter appears exactly once, one that every
annotation is the table's word for word. Without them the same range would be written in
twenty-six places and corrected in one. `actionScriptTemplate()` is the same list with everything
commented at its default; `actionScriptFor()` is the same list with everything live, which is what
Save Action writes.

`mirrorSouth` is the one field left out: it is dead, kept only so clips written before `~base` load,
and a script naming it would be teaching it. The two round-trip tests skip it by name rather than
the writer growing an exception nobody can see.

Renaming is not here. It is the browser's Rename key on the ACTIONS tab, which also carries every
slot firing the file across — a second place to type a name would be a second thing to keep in step.

#### Getting out of an overlay

`OverlayButtons` draws **back** and **close** in the top right, over whichever overlay is open —
the menu, the skin editor, the colour picker. One component rather than three: they are the same
two questions wherever you are. It is a child of `MotionComponent` like the overlays themselves,
so it composites above the GL context, and `A3MotionUIComponent::updateOverlayButtons()` shows and
places it whenever one opens or closes.

Back is exactly what the Menu key does (`toggleGlobalSettings`) — one level at a time. Close
(`closeAllOverlays`) is the one thing the key cannot offer: out of all of it at once, however deep.

#### On-screen keyboard

**The keyboard is the app's own and stands in the bar** (2026-09-28, replacing Onboard). Asked for
as *"ein keyboard welches nur den bereich vom clipsettingeditor ausfüllt und auf unser hardware
controller layout passt"*. `BarKeyboardComponent` covers exactly the clip content
(`ClipSettingsComponent::clipContentBounds()`, the rectangle CLIP, MOTION, ACTION, CHMIX and REC
use), so the sphere, the channel row, the header and the global strip stay in view -- and with
them every field text is typed into, because all of those lie over the sphere: the menu's masks
and skin names (`SkinEditorComponent`), the FILES rename row (`BrowserComponent`) and the FILES
script editor (`ScriptPanel`). It is a child of the bar, always on top of ACTION and CHMIX.

**Laid out on the encoders' four by two.** `pageFieldGrid()` is the one grid of eight fields CLIP,
MOTION and REC stand in; the keyboard splits each field into two rows of three keys
(`BarKeyboardLayout`), so it is four rows of twelve and every key stands under exactly one
encoder. Wide keys (space, HIDE) span whole slots. QWERTZ, because the maintainer types German:

| Row | Letters page | Symbols page (123) |
|---|---|---|
| 1 | `q w e` `r t z` `u i o` `p ü DEL` | `1 2 3` `4 5 6` `7 8 9` `0 . DEL` |
| 2 | `a s d` `f g h` `j k l` `ö ä ENTER` | `- / "` `: ; =` `~ \ '` `, + ENTER` |
| 3 | `SHIFT y x` `c v b` `n m ß` `. - _` | `( ) {` `} [ ]` `< > *` `_ \| !` |
| 4 | `123 ◀ ▶` `SPACE ···` `··· ···` `ESC HIDE ···` | `ABC ◀ ▶` (the rest as on letters) |

The symbols page holds what the shipped scripts, clips and sets are written with; the editing
keys stand in the same place on both pages. SHIFT once is the next letter, twice CAPS, a third
time off. DEL and the arrows act on touch and repeat while held; every other key types on release,
so a finger can slide off a wrong key.

**It types nothing itself.** A key becomes a `juce::KeyPress` (`BarKeyboardModel::pressKey`) and
goes through the window's peer (`ComponentPeer::handleKeyPress`) to whatever holds the focus,
exactly as a plugged-in keyboard's key would -- the masks, the rename row and JUCE's code editor
already know what Backspace, Enter, Escape and the arrows mean. The keyboard therefore **never
takes the focus** (`setMouseClickGrabsKeyboardFocus (false)`): the rename row and the script editor
end their edit when they lose it.

It opens and closes with the edit: the same `onNamingChanged` / `onRenameEditingChanged` /
`onEditingChanged` callbacks that used to call Onboard call `showKeyboard()`. So ENTER closes it
wherever Enter ends the edit (masks, rename); in the script editor ENTER is a new line and ESC or
HIDE put it away. HIDE leaves the field open; the status bar's KEYS key toggles the keyboard
always.

**The panel while it is up** (the table lives in `BarKeyboardModel.hh`):

| Panel | While typing |
|---|---|
| encoder, turned | walks the six keys of the field it stands under (upper: key rows 1-2, lower: 3-4); the first detent only shows the ring |
| encoder, pressed | types the key its ring is on |
| SHIFT + encoder | FREQ / Q of its channel, as always |
| SHIFT held + a key | capital letter |
| pads, pots, TAP, clock, REC, recmode, MENU | unchanged |

No pad types: the pads play the set, and the keyboard is opened mid-set.

`io/OnScreenKeyboard.{hh,cc}` (the Onboard D-Bus client) is no longer called by the app; it is
still built and tested until it is removed.

#### FPV: a second view of the same state

**The app has two views, FULL and FPV** (2026-10-07; spec in the workspace at
`.claude/notes/fpv-motion-ui.md`, this is phase 1 of it). FULL is everything described above. FPV
is for watching and playing a set: below the status bar, the sphere on the top two thirds and one
strip per channel on the bottom third; the clip-settings bar is gone. The status bar stays, since it
carries the key. `AppView` (`components/AppView.hh`) is the value; the FULL/FPV key
stands right of CLOCK in the status bar (CLOCK is at the left end) and lights while FPV is up. The
view survives a restart as `fpvView` in `config/ui_state.json` (`SettingsPersistence`), because a
desk that was left in FPV should not wake up in the editor.

**It is a view, not a mode of the engine.** Positions, clips, OSC are untouched -- Motion stays
pure OSC and phase 1 adds no address. `A3MotionUIComponent::setView` only decides what is shown
and who may be touched.

**The sphere stays the GL sphere.** The camera and tilt are FULL's, so phase 4's cockpit can be a
camera sitting in a ship instead of a second renderer. What changes is the content:
`MotionComponent::setFpv` stops the shader drawing blobs and has it draw each channel as a ship
instead: a small lit craft over the ball, raytraced in the same pass, pointing along its course in
the room (see "Ships and groups are things in the room" under the gravity flight). Phase 1 drew a
flat dart in the 2D pass with a heading taken on the screen; that was taken out once the shader's
craft drew. The app takes only the sizes from `ShipShape` now (`shipLengthOfBlob`,
`shipStepOfLength`); its dart and screen heading (`shipPath`, `ShipHeading`) are no longer called.

**Touch on the sphere is camera only outside the dance floor** (phase 2 gives the floor to the
group gesture, see below). One finger tilts and turns, two zoom, a double tap resets.
A blob cannot be grabbed, since nothing is drawn to grab. Entering camera mode now releases held
blob grabs and the recording position (`TouchGrabs::releaseAll`) -- this holds for FULL's own
elevation-picture camera toggle too: before, a blob held while the toggle was pressed stayed
held, and the finger that let go was no longer listened to.

**The strips take the clip settings' place.** `FpvLayout` is the pure arithmetic (sphere rectangle,
four strip rectangles, `fpvStripRow`); `FpvStrips` paints a strip: `CH n` or `AUTO`, the clip name
with ▶ or ❚❚, the 3D / FREQ / Q bars and a horizontal meter. The header is `CH n` at the left and `CLIP` or
`ORBIT` at the right (phase 2). Only the strip's tint and the bar fills use the channel's colour
(`ChannelUIState::colour` via `FpvChannel::colour`); text and metrics come from the theme. `FpvLayout` (`components/fpv/FpvLayout.hh/.cc`), `ShipShape` and `FpvStrips`'s paint code are in
`a3-motion-ui-shared` and tested without a window (`FpvLayout`, `ShipShape`,
`FpvStripsPaint`, `AppView`, plus cases in `SettingsPersistence`, `StatusBarLayout`, `TouchGrabs`).

**Encoders in FPV act as with SHIFT:** each one turns its own column's channel FREQ/Q
(`encoderTarget(..., shift)` with `shift = SHIFT held || FPV`), and a press does nothing, so
nothing on FULL's hidden bar page is edited. REC + Play/Pause switches to FULL first and starts the
take there, since a take is steered and saved in FULL. Tapping the view key with FILES open drops
a rename in progress, the same as CLOSE.

**What may be touched in FPV.** The panel works as it does in FULL: Play/Pause, pots, SHIFT. An
action pad fires and selects its action but does **not** switch the page -- there is no page to
switch to. FULL comes back on whatever bar page it had, and shows the action only if the ACTION
page is open. The pad's job is the performance; the page was
only ever feedback. Everything that needs FULL's room does switch to it first: MENU, the
overlays, and the keyboard icon (KEYS). Entering FPV closes the overlays and the keyboard, so
nothing is left standing over a view that has no place for it.

**Depth cue.** A ship on the back half of the ball is drawn smaller and darker, and is a ghost
where the ball hides it (`shipDepthCue`, `hiddenByTheBall`); whether the amounts are right is
judged on the device.

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
  The port is not a fixed tty number: `serialInit()` asks every `ttyACM*`/`ttyUSB*` in
  `/sys/class/tty`, those whose USB ID is the panel's CH343 bridge (`1a86:55d3`, the board file's
  `build.hwids`) first, and keeps the first that answers PING (`io/SerialCandidates`). The ID only
  orders the list; the handshake decides, so a different bridge on a later board still works.

`host.py` (repo root) is a standalone diagnostic/reference tool — not part of the CMake build — for
polling the firmware directly over serial and printing decoded input events; useful when debugging
whether hardware issues are in the firmware, the serial link, or the C++ adapter. The firmware
itself lives in a separate repository (see `team.md` §5.3 for the link).

### JUCE traps

**`juce::PathFlatteningIterator::subPathIndex` counts line segments, not sub-paths.**
`juce_PathIterator.cpp` increments it on every line marker, so on a path built out of `lineTo` --
which is every trajectory built from ticks (`refreshPatternDisplayFromTicks`) -- each segment claims
to begin a new sub-path. Reading it as its name suggests drew every trajectory as a couple of
thousand disconnected two-point strokes; wherever two ticks landed far apart in the picture, the
line silently stopped and started again. Find a stroke break the only way that is true of a path:
this segment starts where the last one ended, or it does not. Pinned by
`SphereProjection.JucesSubPathIndexCountsSegmentsNotSubPaths`.

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
