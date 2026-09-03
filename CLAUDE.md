# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A3 Motion UI is a JUCE-based (C++17) standalone application that drives a spatial-audio motion
controller: recording and playing back movement trajectories for up to 4 audio channels, driven by
a dedicated hardware controller (buttons, pads, encoders, pots) and communicating with external
spatialization software via OSC.

## Build

Requires **JUCE 9.0.1** (release tag, not `develop`) built/installed separately; requires
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
```

`build.sh` configures CMake into `build/` with `-DHARDWARE_INTERFACE_ENABLED=ON` and builds only
the `a3-motion-ui_Standalone` target. It also symlinks `resources/` and `config/` into the build's
artefact directory so the binary can find them at runtime.

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

  How fast it turns is a `TempoLfo` step. The sign is the direction, and `spinPosition()` is the
  single place that decides which way that looks: the screen mirrors these coordinates
  (`cartesian2DHOA2JUCE` maps HOA to `{ -y, -x }`), so a mathematically positive turn reads as
  anticlockwise and the rotation is negated there to make a right-hand turn of the control a
  right-hand turn on the sphere.

  Two places apply it and both must, or the blob leaves its line: `MotionEngine::performPlayback()`
  turns the position before projecting it, and `MotionComponent`'s `drawPathOnSphere()` turns each
  point of the drawn path by the same phase — inside `projectPoint()` rather than by transforming
  the path, which would mean copying it every frame. The phase lives on the `Pattern` beside
  `playPosition` and resets when playback starts, so a clip fired again begins where it was
  recorded.

  **The clip's second slow movement, `swell`, works the same way** and is the reason `TempoLfo`
  is its own module. It sweeps the Pattern's `reach` — how far down the sphere the trajectory's
  outer edge lands — out of where it was set and back, positive opening the coverage towards the
  far pole and negative closing it towards the near one. Same two places have to agree: the engine
  sweeps `params.reach` before projecting, the renderer sweeps it before drawing the line, both
  from the phase on the `Pattern`. It moves an Elevation value but its control lives in **Motion**,
  beside spin: what it is is a slow movement of the clip, not a shape of it, and the two bipolar
  knobs read as the pair they are.

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
the Menu button; sizes and fonts are skin values, edited in the Skin Editor) drawn on top of it while open. Both settings
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

Note also that `handleChannelValueChange` takes a **grid row**, not a pot number: the rows read
3d, freq, Q from the top (`channelRow*`), so passing a literal 0 for "freq" reaches 3d. That is
exactly what went wrong when the rows were reordered.

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

Every address this device speaks is a config value, not a literal: the
`oscAddresses` block in `config/config.json`, read by
`a3-motion-engine/OscAddresses.{hh,cc}`. Defaults are what the system has
always used, so a config without the block behaves as before. `{ch}` stands
for the channel number and is substituted by `withChannel()`.

Two things are not obvious:

- **A bad address is refused, not sent.** `juce::OSCMessage` throws
  `OSCFormatError` on an address pattern it will not take, and these are typed
  on the device — so `loadOscAddresses()` validates each one (JUCE's own rule:
  non-empty, leading slash, every `/`-separated token printable ASCII without
  a space or `#`) and keeps the default when it fails. That validation is the
  reason this is a unit of its own rather than a few `getProperty` calls.
- **Changes apply live, and cross two thread boundaries to do it.** The
  message thread reads the new config (`A3MotionUIComponent::applyOscAddresses`,
  called from `MotionComponent::onAppConfigReloaded`). From there:
  `OscMessageHandler` takes them directly — it receives through
  `OSCReceiver::MessageLoopCallback`, so it is on that same thread. The send
  backend does not: `SpatBackend::setAddresses()` stores them under a lock and
  `applyPendingAddresses()`, called once per drain by `AsyncCommandQueue`,
  rebuilds the cached per-channel patterns on the sending thread. The beat
  address needs the same care for the same reason — `tickCallback()` runs on
  the tempo-clock thread, so it keeps its own copy handed over the same way.
  Reading a `juce::String` on one thread while another replaces it is a race,
  refcount and all.

The bar's **global section** takes its right half and holds three things: a 4x3 grid of
per-channel values (columns = channels in their own colours, rows = freq, Q, 3d), the rec mode, and
the action buttons. A **Filter section** used to sit among the clip's sections showing freq and Q —
but those were never the clip's: `handleClipSettingsValueChange` wrote them through
`setChannelPot1/2`, the same per-channel values the hardware drives. Dissolving that section moved
them where they belong, and nothing was lost. The grid's cells are dragged through
`onChannelValueDragged` and name their own channel, unlike everything else in the bar, which is
about the clip on show.

**How tall the bar is** comes from `clipSettingsPreferredHeight` — what the tallest section's
contents need at the current fonts and pot size — times the skin's `clipSettingsHeightScale`
(default 1.0, clamped 0.5..2.0), and clamped again to half the screen. The scale is a skin value
like `potSize`, so it is dialled in the Skin Editor rather than compiled in. Note that changing it
is a *layout* change and A3MotionUIComponent is what hands the bar its bounds — that is why its
`applyTheme()` ends in `resized()`. Telling the bar alone changes nothing.

**The take's length is seven buttons**, not a list: `recordLengthLog2` /
`recordLengthNames` in `ClipSettingsLayout.hh` hold 1/4 .. 16 bars, laid out four then three on the
Shape section's floor. It was a dropdown over the whole `speedLog2Min..Max` range — twelve entries
nobody wanted to scroll past. The one in force reads as active.

The rec mode and clock buttons in the global section deliberately **never light**. They carry a
value and the value is written on them; a wash that comes and goes says the same thing again, in
grey, and reads as a button stuck half-pressed. REC and TAP still light, because what they show is
momentary and has no label of its own.

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
than three sections each arranging their own. TAP lights up for **a finger only**; it used to flash
on every beat too, which put a blinking light on a bar meant to be read.

The global section is laid out top to bottom: the per-channel grid (rows read **3d, freq, Q** — see
`channelRow*`), centred as one block with its row captions, then **four buttons two by two** — rec mode, menu / rec, tap. The rec mode is a
button like the others now and steps through the modes on a tap, which is what its encoder used to
do; it reads as active whenever it is not Touch. Rows and columns of the grid are capped to what a
knob needs rather than sharing out the section's whole width and height, so the twelve knobs sit
together instead of scattered across half the bar.

The buttons carry three device-wide functions — **MENU**, **REC**, **TAP** — beside its rec-mode display. They are the
finger's way to what the hardware has keys for, and they are not sub-elements of
the section: no encoder reaches them, so they sit beside `controls` in
`ClipSettingsLayout` rather than in it, and `numControlsInSection(4)` stays 1.
The strip is a full section wide for them; it used to be half a section, and a
finger needs a target the size of a finger.

Two of the three are not quite the key they stand for:

- **REC** starts a take on the clip the bar is showing and ends a running one
  (`toggleRecordingOnShownClip`). The hardware key cannot do that: there it is a
  *modifier*, held while a slot's Play|Pause pad names the slot — and a finger
  cannot hold it while pressing a pad that only exists in hardware. The bar
  already says which slot it describes, so that is the slot it uses.
- **TAP** has to bring its own timestamp. The hardware's tap arrives with one
  from the adapter (`getTapTimeMicros`); `handleScreenTap()` reads the clock
  itself and hands it to the shared `handleTapAt()`, or the tempo estimator
  would never see a screen tap at all.

MENU is exactly the key (`toggleGlobalSettings`), closing one level at a time.

The block is **grouped**: `oscAddresses.out` (to Core and IEM, plus `beat`, `tap` and `clockMode`)
and `oscAddresses.in` (VU, energy, and `beat` again). **`beat` appears in both** and is read into
two fields, `beatOut` and `beatIn`: INT mode sends the first, EXT and PIO follow the second. They
default to the same address and usually stay that way — but the file says so in both places, which
is where a reader looks, and nothing here can check that they still agree with the other end.
`loadOscAddresses()` reads the flat shape first and lets the groups override it,
so a `config.json` written before the grouping still works: a file on a device
does not rewrite itself.

Editing happens on the Menu's **Network** page, which slices `oscSender`,
`oscReceiver` and `oscAddresses` out of `config.json` and derives its rows from
the JSON — a key added to the block shows up there without anyone registering
it. `SkinEditorComponent` draws a **heading** wherever a row's parent path
changes, so the file's shape is what groups the list; row labels then show only
their last segment, since the heading has already said the rest. Headings are
rows in the display list (`_rows`) but not landing places: `browseRow()` and
`navigate()` step over them and they carry no hit areas. That display list is
also why `_index` is no longer `_actionRows + parameter`: headings belong to
neither, so the offset stopped being enough — use `browsedParameter()`.

A changed address only changes *this* side of the conversation: `beat` has to
match what the beat-analyzer sends, the channel addresses what A3 Core listens
for. A typo does not fail loudly — the app sends correctly to an address
nobody subscribes to. The reference for what the rest of the system expects is
`web/a3-doc/src/ressources/osc.md`.

#### The overlays' two side strips

The strips left and right of an overlay's panel are drag zones, not margins. Dragging in the
**left** one walks the highlighted row; dragging in the **right** one arms that row and changes
its value, and letting go applies it. That is the encoder's two levels laid out as two *places*
rather than as a press that switches between them, which is a state you have to remember while a
room is waiting. It matters most where the list is longer than the screen — the skin editor — and
that is the case a per-page solution would have got wrong.

`OverlaySideStrips` is **one component for all overlays**, a sibling of `OverlayButtons` under
`MotionComponent`. It owns nothing but the two `TouchControl`s and asks the page it sits around
for `panelBounds()`; `A3MotionUIComponent::updateOverlayButtons()` hands it the bounds of
whichever overlay is open and routes `onBrowse`/`onValue`/`onValueReleased` to that page. Adding a
new overlay means giving it a `panelBounds()` and a branch there, not building strips again — the
first version did build them per page, and the second one existed only to be deleted.

They cover what used to pass touches through to the sphere. That was deliberate once and is not
any more: with an overlay open there is nothing on the sphere worth grabbing. The menu panel is
narrower for them — `globalSettingsSideZoneWidth` reserves a fifth of the width on each side,
because a 32px margin is narrower than a fingertip and cannot be landed on without looking.

The right strip **arms on the first increment** rather than asking for a tap first: dragging there
already means "change this". A row that leads somewhere (`opensSubmenu`) is skipped, since it has
no value to turn, and the skin editor latches the row it started on (`_dragRow`) so a list that
scrolls under a moving finger cannot hand the drag to a different row halfway through.

**Clockmode is not in this menu.** It is a button in the clip settings bar, visible and switchable
without opening anything — a setting in two places is a setting whose location you have to
remember.

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

Its geometry is `ControllerLayout` — channels across, slots down, and where they meet one clip with
its four pads: **play beside stop on top, action beside settings below**. That arrangement was read
off the device, not derived: taken from the pad-index order it had action and stop the wrong way
round, and pressing the pad drawn as ACT reported STOP. A pad's *identity* does come from
`padFunctionByPadIndex` / `slotForPadIndex` in `io/PadFunctions.hh`, the same tables the panel is
read with — those say which pad is which function, but only the hardware says where that function
sits under a hand. `fingertipSize` is the floor for anything hit in a hurry, and
`controllerPreferredHeight()` is why the bar can be taller than the clip settings alone would ask
for: both pages share one area, so it has to satisfy the hungrier of them.

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
have nothing to release — and it sits in the **global strip beside TAP**, not on the pads page: a
modifier you have to change pages to reach is one you cannot hold while pressing what it modifies.
`isButtonPressed()` ors the screen's state with the panel's, so nothing downstream knows or cares
which one a hand is on. TAP keeps two thirds of that row against SHIFT's one, because it is the
control here that has to be hit *in time* and a tempo tap that misses is worse than a modifier that
takes a second go. Record needs no screen twin: the strip's REC button already records into the
shown clip.

#### Getting out of an overlay

`OverlayButtons` draws **back** and **close** in the top right, over whichever overlay is open —
the menu, the skin editor, the colour picker. One component rather than three: they are the same
two questions wherever you are. It is a child of `MotionComponent` like the overlays themselves,
so it composites above the GL context, and `A3MotionUIComponent::updateOverlayButtons()` shows and
places it whenever one opens or closes.

Back is exactly what the Menu key does (`toggleGlobalSettings`) — one level at a time. Close
(`closeAllOverlays`) is the one thing the key cannot offer: out of all of it at once, however deep.

#### On-screen keyboard

The UI draws no keyboard. `io/OnScreenKeyboard.{hh,cc}` asks **Onboard** — the system's on-screen
keyboard — to show or hide over D-Bus (`org.onboard.Onboard`, methods `Show`/`Hide`/`ToggleVisible`
on `/org/onboard/Onboard/Keyboard`), shelling out to `dbus-send`. Onboard's D-Bus service file
starts it on the first call, so there is nothing to launch and nothing to keep running.

Onboard types into whatever window has the focus, so text arrives as ordinary key events;
`SkinEditorComponent::keyPressed` is what turns them into edits. The icon at the right of the
status bar toggles the keyboard, always.

Two machine-level settings matter, both in the user's dconf database rather than in this repo (see
the README for the one-liners):

- `org.onboard.window docking-edge` must be `bottom`. Onboard docks at the **top** by default,
  which covers the status bar and with it the icon that hides it again.
- `org.onboard theme` should be `Blackboard`, with `system-theme-tracking-enabled` off. Onboard
  otherwise follows the system theme, which here is a light beige against a dark UI.

A keyboard of the project's own used to live in `components/KeyboardComponent.{hh,cc}`; it was
removed in favour of Onboard, which already maintains a layout, key faces and a press model.

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
