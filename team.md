# A3 Motion UI: Team-Beschreibung (UI und Hardware)

Dieses Dokument beschreibt den aktuellen technischen Aufbau der UI- und Hardware-Anbindung in a3-motion-ui. Zielgruppe sind Entwicklerinnen und Entwickler im Team, die an Bedienlogik, Hardware-Mapping, Timing oder Fehleranalyse arbeiten.

## 1. Systemüberblick

Die Anwendung besteht aus drei logisch getrennten Ebenen:

1. Motion-Engine (Playback, Recording, Tempo, Pattern-Zustände)
2. UI-Komponenten (Darstellung, Bedien-Interaktion, Visualisierung)
3. Hardware-Adapter (Serielle Eingabe/Ausgabe, Entprellung, Mapping)

Die zentrale Orchestrierung liegt in `A3MotionUIComponent`.

- Erstellt und besitzt die Haupt-UI (Status, Displays, Motion-Canvas)
- Registriert alle Hardware-Listener
- Übersetzt Hardware-Events in Engine-Aktionen
- Handhabt OSC In/Out (Beatclock, VU, Tap)
- Verwaltet das Overlay-Menü für ClockMode

Die Klasse ist damit die Schaltstelle zwischen Hardware-Ereignissen und visuellem sowie musikalischem Verhalten.

## 2. UI-Architektur

### 2.1 Hauptcontainer

Der visuelle Aufbau wird in `A3MotionUIComponent::resized()` deterministisch von oben nach unten berechnet:

1. `StatusBar` ganz oben
2. `LoopLengthDisplay`
3. `ElevationDisplay`
4. Mehrere `PadRowDisplay`-Zeilen
5. `FilterDisplay` unten
6. Restfläche für `MotionComponent`
7. Overlay-Menü als oberste Ebene (volle Fläche des Parent-Components)

Hinweis: `ChannelStrip`-Instanzen existieren weiterhin, sind aber aktuell verborgen (`setVisible(false)`).

### 2.2 Wichtige UI-Komponenten

- `StatusBar`
	Zeigt BPM, Beat-Zähler und ClockMode (`INT`, `EXT`, `PIO`) inkl. Farbcodierung.

- `MotionComponent`
	Zentrale Bewegungs-/Trajektorien-Darstellung, gekoppelt an Pattern- und Playback-Zustände.

- `LoopLengthDisplay`
	Kanalweise Loop-Länge in Beats, synchron zur Engine-Länge.

- `ElevationDisplay`
	Visualisiert kanalweise Coverage/Elevation-Parameter.

- `PadRowDisplay`
	Zeigt pro Kanal die aktuell selektierten Pattern-/Trajektorien-Slots.

- `GlobalSettingsComponent`
	Geräteweites Menü (Clockmode, Pot Size, Font Size). Teilt sich den unteren
	Settings-Bereich mit `ClipSettingsComponent` (siehe dort).

### 2.3 Global Settings (aktuelles Verhalten)

`GlobalSettingsComponent::paint()` rendert derzeit:

1. Eine halbtransparente Abdunklung über die gesamte UI (`g.fillAll(...)`)
2. Ein zentriertes Panel
3. Die auswählbaren Menüeinträge mit
	 - `selected` (aktuell markierter Kandidat)
	 - `active` (aktuell angewandter Modus)

Damit ist das Menü visuell als Fullscreen-Overlay umgesetzt, obwohl die eigentliche Interaktion im zentralen Panel stattfindet.

## 3. Event- und Datenfluss

### 3.1 Input-Pipeline

Hardwaredaten laufen nicht direkt in die UI, sondern über `InputOutputAdapter`:

1. Adapter-Thread pollt Hardware (`processInput()`)
2. Adapter baut typisierte InputMessages (Pad, Button, Encoder, Pot, Tap)
3. FIFO-Übergabe in den Message-Kontext
4. `timerCallback()` des Adapters dispatcht auf `juce::Value`
5. `A3MotionUIComponent::valueChanged(...)` verarbeitet die Änderungen

Vorteil: Die UI bekommt normalisierte Events, unabhängig vom konkreten Hardware-Protokoll (V2/V3).

### 3.2 End keys and function keys in A3MotionUIComponent

The panel's two end columns (col0, col9) and the PADS page's copy of them report **end keys**
(`EndKey`, `io/FunctionKeys.hh`), not functions. One table, `endKeyTable`, says what stands where:

| Row | Left (col0) | Right (col9) | SHIFT + key |
|---|---|---|---|
| 0 | TAP | TAP | CLOCK (cycles the clock mode) |
| 1 | SHIFT | SHIFT | — |
| 2 | PLAY all | PLAY all | REC (held) |
| 3 | A1 on every channel | A2 on every channel | REC MODE (cycles) |
| 4 | A3 on every channel | A4 on every channel | — (free, does nothing) |
| 5 | A5 on every channel | A6 on every channel | MENU (toggles) |

Flow: adapter Value per `EndKey` → `valueChanged(...)` → `setEndKey(key, KeySource::Panel, down)`;
the PADS page calls the same with `KeySource::Screen`. `EndKeyHold` joins both sources (down while
either holds), `EndKeyLayer` decides the meaning, then:

- a function → `functionKeyChanged(key, down)`:
  - `Tap`: direct OSC `/tap` through `_tapSender` (time-critical, no async queue).
  - `ClockMode`, `RecMode`: cycle on press, release ignored.
  - `Menu`: toggle-on-press — opens or closes the global settings, release ignored.
  - `Record`: held modifier. REC held + a channel's Play|Pause pad records onto that channel
    (`handlePadPress()`); pressed while a take runs, it ends the take.
  - `Shift`: held modifier, read through `isButtonPressed(Button::Shift)`.
- PLAY all / an action key → `handleScenePress(pad)` / `handleSceneRelease(pad)`: that pad on every
  channel through `handlePadPress()`. PLAY all reaches the channels `PlayAllPress` picks: anything
  playing → pause what plays; nothing playing → start everything that stands still; a double tap
  goes to the top on the channels the first tap reached.

SHIFT semantics:

- **SHIFT first, then the key** is a combination; it does only the shifted function. A key pressed
  before SHIFT keeps its plain meaning.
- **REC is held while SHIFT and PLAY all both are.** Releasing either ends REC.
- SHIFT + TAP never taps: the tap time is dropped while SHIFT is down.
- SHIFT + an action key is never "the shifted action on every channel"; row 4 does nothing.
- SHIFT with a channel's own pads keeps its meanings (Play|Pause on the downbeat, PAGE back, ACT
  preview).

`TapTimeMicros`: only with `ClockMode == INT` and SHIFT up is the tap fed into the TempoClock.

On the screen CLOCK and MENU are in the status bar, REC MODE on the REC page and REC as the
transport's ● key, besides the PADS page's SHIFT layer.

### 3.3 Encoder-/Pot-Menüinteraktion

Beim Öffnen des Menüs:

- Menüeinträge werden neu aufgebaut (`INT`, `EXT`, `PIO`)
- `active` und `selected` werden auf den aktuellen `_clockMode` gesetzt
- Pot-Wert für Navigations-Delta wird gesnapshottet (`_menuNavLastPot`)

Bestätigung:

- Über Encoder-Press `getEncoderPress(3,1)` (Pot-Encoder Kanal 3)
- Bei Press und offenem Menü: `closeMenu(true)`
- Danach `applyClockMode(selectedIndex)`

### 3.4 OSC-Fluss

- OSC-In Beatclock: eigener Receiver-Port (default 7771)
- OSC-In VU: separater Receiver-Port (default 7772)
- OSC-Out Beatclock: Async-Sender
- OSC-Out Tap: direkter Sender für minimale Latenz

Addresses and ports come from the one truth, a3-core's `/usr/share/a3/a3-osc.json` (since 2026-09-30), not from `config.json`.

### 3.5 FILES and ACTION (2026-09-27)

- **ACTION** (2026-09-28): the six fields A1–A6 only choose (`onButtonChosen` →
  `chooseActionButton`), the assignment list (a tap assigns to the chosen button: `onActionChosen`
  → `setButtonAction`), EDIT, the mode, *then* (`onAfterStepped`/`onAfterCleared` →
  `setShownButtonAfter`), AUDIO/MOTION (`setTile`), and the chosen button's tile: the envelope
  knobs, or its motion (`onMotionSet`/`onMotionUnset` → `setShownButtonMotion`). A pad push on the
  panel or PADS → `handlePadPress`, then `showPushedAction`. Chains: `followActionChains` in
  `tickCallback` → `fireChainedAction`. The first encoder is `EncoderTarget::Kind::ActionButton`.
- **EDIT** (`ActionComponent::onEditPressed`) sets `_editOrigin`, opens FILES on ACTIONS; the panel
  follows the chosen row through `refreshBrowser` → `syncFilePanel`.
- **FILES**, every tab: the chosen row's file in the panel (`showFileText`). Keys:
  - Save → `saveFileText` (lock, `fileErrorsOf`/`errorsBlockSaving`, write, `afterSaving`);
  - Save as → `saveFileTextAs` (copy into the user half, `afterCopying`);
  - Cancel → `showFileText (_panelFile)`;
  - FROM → `offerScript (currentList ().currentStateText ())`.
- Unsaved text holds rows, Rename, Delete and the tabs (`fileTextHoldsTheList`). Leaving FILES stops
  editing and clears `_editOrigin` (`showOverSphere`).

## 4. ClockMode und zeitliches Verhalten

`_clockMode` kodiert:

- `0 = INT`
- `1 = EXT`
- `2 = PIO`

Bei Wechsel in einen externen Modus wird intern der zuletzt aktive BPM-Wert konserviert (`_internalBPM`), um nach Rückkehr zu `INT` sauber weiterarbeiten zu können.

`StatusBar` nutzt den Modus für Farblogik:

- INT: grün
- EXT: orange
- PIO: cyan

Zusätzlich beeinflusst der Modus, ob interne Tick-/Beat-Updates oder externe Beatclock-Daten priorisiert angezeigt werden.

## 5. Hardware-Abstraktion

### 5.1 Gemeinsame Adapter-Basis

`InputOutputAdapter` stellt die einheitliche API bereit:

- Inputs als `juce::Value`: Buttons, Pads, Encoder, Pots, Tap
- Outputs als `juce::Value`: Button-LEDs, Pad-LEDs
- Hintergrund-Thread für Hardware-I/O
- FIFO-basierte Entkopplung zwischen I/O-Thread und UI/Message-Thread

Damit bleibt die UI von konkreten Serial-Protokollen isoliert.

### 5.2 V2-Adapter

`InputOutputAdapterV2` verarbeitet textbasierte Serial-Lines.

- Prefix `B`: Buttons/Pads
- Prefix `EB`: Encoder-Press
- Prefix `Enc`: Encoder-Increment
- Prefix `P`: Pot-Werte

Spezialfall bei Buttons:

- Index 18 -> `EndKey::Tap` (+ optional Timestamp)
- Index 16 (clock) and 17 (REC) are not reported: the end keys reach both through SHIFT, which
  this panel does not have, so on V2 they are reached on the screen.

V2 ist simpel, aber stärker vom Firmware-Stringformat abhängig.

### 5.3 V3-Adapter

Repository zur Hardware / Firmware
https://github.com/a3-audio/a3-motion/tree/v03.2

`InputOutputAdapterV3` verwendet binär gepackte Poll-Frames mit dedizierten Kommandos.

Hinweis zu v03.2: Laut `firmware/host.py` kann die USB-CDC-Antwort je nach Stack in
`BTN+ENC` oder `ENC+BTN` Reihenfolge eintreffen. Der Adapter sollte Marker-basiert
validieren und beide Layouts akzeptieren.

Hardwaremodell laut Kommentar/Mapping:

- 44 Buttons
- 8 Encoder (inkl. Push)
- 4 globale Potis

Buttonzustände sind 2-Bit-kodiert und werden in `parseButtons()` zu Press/Release-Ereignissen normalisiert.

#### End columns on V3

`buttonMap` marks col0/col9 as `Function` with their row; `endRowHwIndices` lists each row's two
firmware indices (left, right):

- row 0: 40 (`"00"`), 42 (`"09"`)
- row 1: 41 (`"10"`), 43 (`"19"`)
- row 2: 2 (`"20"`), 36 (`"29"`)
- row 3: 1 (`"30"`), 38 (`"39"`)
- row 4: 0 (`"40"`), 39 (`"49"`)
- row 5: 3 (`"50"`), 37 (`"59"`)

Which key stands at a row and side comes from `endKeyTable`. Pads are mapped per channel onto
indices 4..35 (8 pads per channel).

`dispatchButtonEvent()` joins the two places of TAP, SHIFT and PLAY all with `EndColumnHold`: one
press when the first side goes down, one release when the last comes up. The action keys have one
place each. LEDs are written per end key: TAP, SHIFT and PLAY all light both sides, each action key
its own.

## 6. Pattern- und UI-Synchronisation

Beim Start wird eine `PatternLibrary` initialisiert (system/user-Verzeichnisse). Danach:

1. Pattern-Slots werden kanalweise vorbereitet
2. Erste Page wird aus der Library befüllt
3. Timer überwacht Verzeichnisänderungen
4. UI-Labels werden bei Änderungen aktualisiert

Zusätzlich wird in mehreren Pfaden darauf geachtet, Playback- und Recording-Längen konsistent zur aktuellen Loop-Länge zu halten.

## 7. Betriebs- und Debug-Hinweise

### 7.1 Wenn UI-Eingaben nicht reagieren

1. Läuft der korrekte Adapter (V2 oder V3 Build-Flag)?
2. Ist `/dev/ttyACM0` erreichbar?
3. Kommen Adapter-Events in `valueChanged(...)` an?
4. Werden Menü-/ClockMode-Zustände (`_globalSettingsOpen`, `_clockMode`) korrekt umgeschaltet?

### 7.2 Wenn Menüverhalten unerwartet ist

Aktueller Stand:

- MENU is SHIFT + a row 5 key (A5 or A6 column), or MENU in the status bar.
- `Menu` is toggle-on-press: a press opens or closes `GlobalSettingsComponent`, the release is ignored (see `A3MotionUIComponent::functionKeyChanged(...)`).

### 7.3 Wenn Tempoanzeige inkonsistent ist

1. Prüfen, welcher ClockMode aktiv ist
2. Bei externem Modus: kommen OSC-Daten an?
3. Bei internem Modus: kommen Tap-Zeiten rein und wird `TempoClock::tap()` aufgerufen?
4. Prüfen, ob `StatusBar::setClockMode(...)` und Beat-/BPM-Updates auf dem Message-Thread landen

## 8. Bekannte Inkonsistenzen und Pflegehinweise

1. Einige Kommentare nennen andere Button-Labels/Indizes als das aktuelle V3-Mapping; bei Debug immer den tatsächlichen `buttonMap` in V3 heranziehen.
2. Bei Änderungen an Firmware-Indexen muss sowohl Mapping als auch Team-Dokument synchron aktualisiert werden.

## 9. Wichtige Dateien für Änderungen

- `src/a3-motion-ui/components/A3MotionUIComponent.hh`
- `src/a3-motion-ui/components/A3MotionUIComponent.cc`
- `src/a3-motion-ui/components/GlobalSettingsComponent.hh`
- `src/a3-motion-ui/components/GlobalSettingsComponent.cc`
- `src/a3-motion-ui/components/ClipSettingsComponent.hh`
- `src/a3-motion-ui/components/ClipSettingsComponent.cc`
- `src/a3-motion-ui/components/StatusBar.hh`
- `src/a3-motion-ui/components/StatusBar.cc`
- `src/a3-motion-ui/io/InputOutputAdapter.hh`
- `src/a3-motion-ui/io/InputOutputAdapter.cc`
- `src/a3-motion-ui/io/InputOutputAdapterV2.hh`
- `src/a3-motion-ui/io/InputOutputAdapterV2.cc`
- `src/a3-motion-ui/io/InputOutputAdapterV3.hh`
- `src/a3-motion-ui/io/InputOutputAdapterV3.cc`

---

Stand dieses Dokuments: entspricht dem aktuell in der Codebasis sichtbaren Verhalten (inkl. hold-basierter Menübehandlung und Fullscreen-Overlay-Dimmung).
