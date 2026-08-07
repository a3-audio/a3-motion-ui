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
	Geräteweites Menü (Clockmode, Elevation Map). Teilt sich den unteren
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

### 3.2 Button-Logik in A3MotionUIComponent

Aktuell sind folgende Buttons als Listener registriert:

- `ClockMode`
- `Menu`
- `Record`
- `Tap`

Aktuelles Verhalten in `valueChanged(...)`:

- `Button::ClockMode`
	Der direkte Cycle ist deaktiviert (nur Legacy-Platzhalter, keine Aktion).

- `Button::Menu`
	`pressed -> openMenu()`, `released -> closeMenu(false)`.
	Das bedeutet: Das Menü ist in der aktuellen Implementierung gedrückt-halten-basiert und ein Loslassen verwirft die Auswahl, sofern nicht zuvor bestätigt wurde.

- `Button::Record`
	Long-Press-Tracking für Recording-Interaktionen, inkl. LED-Steuerung.

- `Button::Tap`
	Senden einer direkten OSC-`/tap`-Message über `_tapSender` (zeitkritisch, ohne Async-Queue).

- `TapTimeMicros`
	Nur bei `ClockMode == INT` wird Tap-Tempo in die TempoClock eingespeist.

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

Konfiguration erfolgt über `config.json`/UserConfig (`oscReceiver`, `oscSender`).

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

- Index 16 -> `ClockMode`
- Index 17 -> `Record`
- Index 18 -> `Tap` (+ optional Timestamp)

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

#### Wichtige V3-Mappings

- Tap links: Index 2 (`"20"`)
- Tap rechts: Index 36 (`"29"`)
- Record links: Index 41 (`"10"`)
- Record rechts: Index 43 (`"19"`)
- MenuToggle links: Index 3 (`"50"`)
- MenuToggle rechts: Index 37 (`"59"`)

Pads sind kanalweise auf Indexbereiche 4..35 gemappt (je 8 Pads pro Kanal).

#### Chord-Logik für Menü

In `dispatchButtonEvent()` wird für die beiden MenuToggle-Tasten ein interner Zweibit-Zustand geführt (`_menuButtonState`).

- Wenn beide gedrückt sind und vorher nicht beide gedrückt waren -> `inputButtonValue(Button::Menu, true)`
- Wenn der Chord aufgelöst wird -> `inputButtonValue(Button::Menu, false)`

Das erzeugt genau ein Press-/Release-Paar für den Chord, statt einzelner linker/rechter Menüsignale.

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
4. Werden Menü-/ClockMode-Zustände (`_menuOpen`, `_clockMode`) korrekt umgeschaltet?

### 7.2 Wenn Menüverhalten unerwartet ist

Aktueller Stand:

- V3-Chord erzeugt `Menu true/false`
- UI behandelt `Menu` als press-hold-semantik (`open` bei true, `close(cancel)` bei false)

Wenn Toggle gewünscht ist, muss die Semantik in `A3MotionUIComponent::valueChanged(...)` angepasst werden (Press toggelt, Release ignoriert).

### 7.3 Wenn Tempoanzeige inkonsistent ist

1. Prüfen, welcher ClockMode aktiv ist
2. Bei externem Modus: kommen OSC-Daten an?
3. Bei internem Modus: kommen Tap-Zeiten rein und wird `TempoClock::tap()` aufgerufen?
4. Prüfen, ob `StatusBar::setClockMode(...)` und Beat-/BPM-Updates auf dem Message-Thread landen

## 8. Bekannte Inkonsistenzen und Pflegehinweise

1. Einige Kommentare nennen andere Button-Labels/Indizes als das aktuelle V3-Mapping; bei Debug immer den tatsächlichen `buttonMap` in V3 heranziehen.
2. Global-Settings-Kommentar in der UI spricht von Chord `00+09`, das aktive V3-Mapping nutzt jedoch derzeit `50+59` als MenuToggle.
3. Bei Änderungen an Firmware-Indexen muss sowohl Mapping als auch Team-Dokument synchron aktualisiert werden.

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
