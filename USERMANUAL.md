# A3 Motion — Benutzerhandbuch

Der A3-Motion-Controller zeichnet Bewegungstrajektorien für bis zu vier Audiokanäle auf
und spielt sie zurück — als Azimut/Elevation-Daten für Ihr räumliches Audiosystem. Dieses
Handbuch beschreibt Bildschirm, Bedienelemente und alle Clip-Parameter.

*Firmware v03.2 · 4 Kanäle · 2 Clip-Slots pro Kanal*

## Inhalt

1. [Einführung](#1-einführung)
2. [Geräteüberblick](#2-geräteüberblick)
3. [Die Sphäre](#3-die-sphäre)
4. [Kanäle & Clips](#4-kanäle--clips)
5. [Eine Trajektorie aufnehmen](#5-eine-trajektorie-aufnehmen)
6. [Wiedergabe](#6-wiedergabe)
7. [Clip-Einstellungen](#7-clip-einstellungen)
   - [7.1 Shape](#71-shape)
   - [7.2 Elevation](#72-elevation)
   - [7.3 Motion](#73-motion)
   - [7.4 Filter](#74-filter)
8. [Globale Einstellungen](#8-globale-einstellungen)
   - [8.1 Werte eingeben](#81-werte-eingeben)
   - [8.2 Bildschirmtastatur](#82-bildschirmtastatur)
9. [Takt & Tempo](#9-takt--tempo)
10. [Parameterreferenz](#10-parameterreferenz)
11. [Problembehandlung](#11-problembehandlung)

---

## 1. Einführung

A3 Motion ist der Bewegungs-Controller im A3-Audio-System. Für jeden der vier Kanäle
zeichnen Sie eine **Trajektorie** auf — einen Pfad, den der Kanal in Azimut (Winkel um
den Hörer) und Elevation (Höhe über/unter dem Hörer) durchläuft — und spielen sie
taktsynchron zurück. Das Ergebnis wird laufend als Azimut/Elevation-Werte per OSC an
Ihre 3D-Audio-Software gesendet.

Jeder Kanal verwaltet zwei **Clip-Slots**, zwischen denen Sie live umschalten können —
etwa eine ruhige Grundbewegung in Slot 1 und eine Break-Variante in Slot 2. Form,
Höhenverhalten, Geschwindigkeit und Filter jedes Clips lassen sich unabhängig
einstellen, direkt am Gerät, ohne Computer.

> **Für wen:** Dieses Handbuch richtet sich an Personen, die das Gerät im Livebetrieb
> bedienen. Für Entwicklung, Firmware und Hardware-Mapping siehe die technische
> Dokumentation im Repository (`team.md`, `CLAUDE.md`).

## 2. Geräteüberblick

Der Bildschirm ist von oben nach unten in drei Bereiche gegliedert:

- **Statuszeile** — BPM, Taktanzeige, aktueller Clock-Modus und das Symbol für
  die Bildschirmtastatur (Kapitel 8.2).
- **Sphäre** — die zentrale Bewegungsansicht (siehe Kapitel 3).
- **Clip-Einstellungen** — das ständig sichtbare Panel am unteren Rand mit den vier
  Sektionen Shape, Elevation, Motion, Filter (Kapitel 7).

### Pro Kanal

Jeder der vier Kanäle hat identische Bedienelemente:

| Element | Funktion |
|---|---|
| 8 Pads | Play/Pause, Action, Stop, Settings — je zweimal, eines pro Clip-Slot (Kapitel 4). |
| Motion-Encoder | Oberer Drehregler. Blättert durch die vier Sektionen im Clip-Einstellungen-Panel. |
| Pot-Encoder | Unterer Drehregler. Ändert den Wert des gerade markierten Reglers; Drücken wechselt zwischen mehreren Reglern innerhalb einer Sektion. |

### Global

| Taste | Funktion |
|---|---|
| Shift | Modifikator, wird gehalten — z. B. für Preview-and-Fire (Kapitel 6). |
| Record | Modifikator, wird gehalten, während ein Play/Pause-Pad gedrückt wird, um eine neue Aufnahme zu starten (Kapitel 5). |
| Tap | Tap-Tempo im internen Clock-Modus; sendet zusätzlich immer eine OSC-`/tap`-Nachricht. |
| Menu | Öffnet/schließt die globalen Einstellungen (Kapitel 8). |

Die vier Kanäle sind farblich unterscheidbar — dieselben Farben erscheinen als Blobs
auf der Sphäre und als Rahmen des Clip-Einstellungen-Panels (Kanal 1–4).

## 3. Die Sphäre

Die Sphäre zeigt den Raum von außen: der Hörer sitzt im Zentrum, die vier
Lautsprecher-Symbole markieren die physischen Lautsprecherpositionen. Jeder Kanal ist
ein farbiger Blob auf der Kugeloberfläche.

- **Zenit (oben)** entspricht 100 % Elevation — direkt über dem Hörer.
- **Äquator** entspricht 50 % Elevation — Horizont-Höhe.
- **Nadir (unten)** entspricht 0 % Elevation — direkt unter dem Hörer.

Ein Blob lässt sich mit dem Finger direkt greifen und ziehen, um die Position eines
Kanals live zu verändern — nützlich zum Antesten oder als manuelle Übersteuerung,
solange kein Clip abgespielt wird.

Während der Bearbeitung wird die aktuell im Clip-Einstellungen-Panel gewählte
Trajektorie immer in voller Deckkraft dargestellt, unabhängig davon, auf welcher Seite
der Sphäre sie gerade liegt. Andere, nur mitlaufende Clips werden dünner und — auf der
sphärenabgewandten (unteren) Hälfte — etwas abgedunkelt gezeichnet, damit die aktuelle
Bearbeitung immer klar erkennbar bleibt.

## 4. Kanäle & Clips

Jeder Kanal hat zwei Clip-Slots. Die acht Pads eines Kanals sind in zwei Blöcke zu je
vier Pads aufgeteilt — ein Block pro Slot, jeweils mit denselben vier Funktionen:

| Pad | Funktion |
|---|---|
| Play/Pause | Idle → Wiedergabe (startet auf den nächsten Takt-Downbeat). Während der Wiedergabe stoppt ein erneuter Druck den Clip. |
| Action | Nur in Kombination mit **Shift** aktiv — löst Preview-and-Fire aus (Kapitel 6). Ohne Shift derzeit ohne Funktion. |
| Stop | Stoppt den Clip in diesem Slot (aus Wiedergabe, Aufnahme oder geplantem Zustand). |
| Settings | Wählt diesen Slot im Clip-Einstellungen-Panel aus (Kapitel 7). |

Ein leerer Slot zeigt im Clip-Einstellungen-Panel „Empty" und hat keine Wirkung, bis
eine Form geladen (Kapitel 7.1) oder eine neue Trajektorie aufgenommen wird (Kapitel 5).

## 5. Eine Trajektorie aufnehmen

Halten Sie **Record** gedrückt und drücken Sie das Play/Pause-Pad des gewünschten
Slots. Eine neue, leere Aufnahme ersetzt sofort den bisherigen Inhalt dieses Slots und
startet auf den nächsten Takt-Downbeat.

Während der Aufnahme läuft, bewegen Sie den Kanal-Blob auf der Sphäre (siehe Kapitel
3) — die Aufnahme läuft in einer Schleife über die aktuell eingestellte **Speed**-Länge
(Kapitel 7.3) und zeichnet dabei kontinuierlich auf.

> **Tipp:** Stellen Sie Speed *vor* der Aufnahme ein — sie bestimmt, wie viele Takte
> bzw. welcher Notenwert einem vollen Durchlauf entspricht.

## 6. Wiedergabe

Play/Pause und Stop starten bzw. beenden die Wiedergabe eines Slots, jeweils
quantisiert auf den nächsten Downbeat (Kapitel 4).

### Preview-and-Fire

Halten Sie **Shift** und drücken Sie das Action-Pad eines idlen Slots: Der Clip spielt
sofort probeweise ab, ohne OSC-Ausgabe zu senden — Sie können währenddessen mit dem
Motion-Encoder durch andere Formen blättern (Kapitel 7.1), um eine passende zu finden.
Lassen Sie Action los, um die Vorschau zu beenden.

## 7. Clip-Einstellungen

Das Panel am unteren Bildschirmrand zeigt immer die Einstellungen des zuletzt per
Settings-Pad gewählten Slots, in vier Sektionen: **Shape**, **Elevation**, **Motion**,
**Filter**. Der Rahmen des Panels ist in der Farbe des aktiven Kanals gehalten.

- Der **Motion-Encoder** blättert zwischen den vier Sektionen.
- Der **Pot-Encoder** ändert den Wert des markierten Reglers; Drücken wechselt zum
  nächsten Regler innerhalb der Sektion (bei Elevation, Motion und Filter gibt es
  mehrere).

### 7.1 Shape

Zeigt ein Piktogramm und den Namen der geladenen Trajektorie. Drehen Sie den
Pot-Encoder, um durch die Formen-Bibliothek zu blättern — mitgelieferte Systemformen
und Ihre eigenen Aufnahmen, alphabetisch sortiert. Der erste Eintrag ist „Empty"
(leerer Slot).

### 7.2 Elevation

Bestimmt, wie sich die aufgenommene 2D-Form auf Höhe abbildet. Eine Seitenansicht der
Sphäre zeigt die Wirkung grafisch: graue Balken markieren durch Clip-Top/Clip-Bottom
ausgeschlossene Bereiche, eine durchgezogene Linie den äußeren Rand der Trajektorie.

Im Normalfall (Flat aus) liegt die Mitte der aufgenommenen Form immer auf einem Pol,
ihr äußerer Rand reicht bis zu dem von **Reach** festgelegten Punkt. **Pole** legt
fest, welcher Pol das ist. Clip-Top und Clip-Bottom sind harte, absolute Grenzen von
den jeweiligen Polen aus — sie werden nie überschritten, egal wie Reach eingestellt
ist.

| Regler | Bereich | Beschreibung |
|---|---|---|
| Reach | 0.05 – 1.0 | Wie weit der äußere Rand der Form vom Pol Richtung Gegenpol reicht. 0.5 = bis zum Horizont, 1.0 = bis zum Gegenpol. |
| Clip-Top | 0 – 1 | Schließt diesen Anteil des Bereichs vom oberen Pol (Norden) aus. |
| Clip-Bottom | 0 – 1 | Schließt diesen Anteil des Bereichs vom unteren Pol (Süden) aus. |
| Pole | Nord / Süd | Welcher Pol die Mitte der Form bildet. |
| Flat | Aus / An | Wenn an: die gesamte Form liegt auf einer festen Elevation (Flat-Elv) — Reach/Pole werden ignoriert, nur der Azimut (Grundriss der Form) bleibt erhalten. |
| Flat-Elv | 0 – 1 | Feste Elevation für den Flat-Modus (0 = Süd-, 1 = Nordpol, 0.5 = Horizont). |

### 7.3 Motion

| Regler | Bereich | Beschreibung |
|---|---|---|
| Speed | 1/128 – 16 | Länge eines vollen Durchlaufs in Takten bzw. Notenwerten — ganz links am langsamsten (16 Takte), ganz rechts am schnellsten (1/128-Notenwert). Der aktuelle Wert wird über dem Regler angezeigt. Wirkt sofort auf den gerade gewählten Clip, auch während der Wiedergabe. |
| Direction | Forward / Reverse / PingPong | Abspielrichtung. *Derzeit ohne Funktion.* |
| End-Action | Loop / Stop / Bounce | Verhalten am Ende eines Durchlaufs. *Derzeit ohne Funktion.* |

### 7.4 Filter

| Regler | Bereich | Beschreibung |
|---|---|---|
| Sweep | 0 – 1 | Filter-Sweep für diesen Kanal. |
| Q | 0 – 1 | Filter-Resonanz für diesen Kanal. |

Sweep und Q haben keine eigenen Potis — beide werden ausschließlich über den
Pot-Encoder dieser Sektion eingestellt, wie alle anderen Regler auch.

## 8. Globale Einstellungen

Die Menu-Taste öffnet bzw. schließt ein Overlay mit geräteweiten Einstellungen.
Solange es offen ist, übernehmen die beiden Encoder von Kanal 4 die Navigation: der
Motion-Encoder wählt den Menüpunkt, der Pot-Encoder ändert dessen Wert — einmal
drücken, um den Wert zu bearbeiten, ein zweites Mal, um zu bestätigen.

| Einstellung | Werte | Beschreibung |
|---|---|---|
| Clockmode | INT / EXT / PIO | INT: Der Controller gibt das Tempo selbst vor (Tap-Tempo). EXT/PIO: folgt einer eingehenden OSC-Beatclock. |
| Skin | *(vorhandene Skins)* | Farb- und Formschema. Das Bild wechselt schon beim Drehen; erst der Druck macht den Skin zum laufenden. |
| Skin Editor | open | Jeder Wert des laufenden Skins, einzeln editierbar — dazu Speichern, Als neu speichern, Umbenennen, Löschen. |
| Network | open | OSC-Hosts und -Ports. Wirken erst nach einem Neustart, weil sie beim Öffnen der Sockets gelesen werden. |
| Button LEDs | open | Farben der Funktionstasten, inklusive `idle` — der Farbe, in der sie leuchten, **ohne** gedrückt zu sein. |
| Pattern Folder | open | Verzeichnis, aus dem Trajektorien geladen werden. |
| Sphere in Menu | an / aus | Ob die Sphäre hinter dem geöffneten Menü sichtbar bleibt. |

Die Einstellungen bleiben nach einem Neustart erhalten.

**Größen und Schriften stehen nicht hier, sondern im Skin.** Sphärengröße,
Reglergröße und die beiden Schriftgrößen sind Teil des Aussehens und wandern
deshalb mit dem Skin mit — zu finden im **Skin Editor** als `sphereScale`,
`potSize`, `fontHeader` und `fontBody`. Sie standen einmal als Prozentstufen in
diesem Menü, während ihre Basiswerte schon im Skin lagen; ein Skinwechsel
verschob dann die eine Hälfte und ließ die andere stehen.

### 8.1 Werte eingeben

Zeilen mit einem **Text**- oder **Zahlenwert** — Hosts, Ports, das Pattern-
Verzeichnis, ein Skin-Name — öffnen beim Druck auf den Encoder ein Eingabefeld
und blenden dazu die Bildschirmtastatur ein. Ein Port ist damit direkt
eintippbar, statt über zehntausend Werte gedreht werden zu müssen.

Die Werte **im Skin Editor** verhalten sich bewusst anders: sie werden gedreht,
nicht getippt. Sie werden eingestellt, während man die Sphäre dabei ansieht —
das ist der Grund, einen Skin überhaupt am Gerät zu bearbeiten. Wer dort doch
tippen will, erreicht das Eingabefeld über das Tastatur-Symbol.

**Farbzeilen** öffnen statt eines Eingabefeldes einen Farbwähler mit HSL-Feldern.

### 8.2 Bildschirmtastatur

Das Gerät zeichnet keine eigene Tastatur. Es benutzt **Onboard**, die Tastatur
des Systems, und blendet sie über D-Bus ein und aus; getippt wird in das Fenster,
das gerade den Fokus hat. Das Symbol rechts in der Statusleiste blendet sie
jederzeit ein und aus — unabhängig davon, was gerade auf dem Schirm ist.

Onboard muss dafür installiert sein (Paket `onboard`, siehe `README.md`). Fehlt
es, tut das Symbol nichts, und jedes Feld bleibt weiterhin über den Encoder
erreichbar.

## 9. Takt & Tempo

Die Statuszeile zeigt links die BPM, mittig die Taktanzeige und rechts den
aktuellen Clock-Modus farbcodiert (INT grün, EXT orange, PIO cyan) sowie das
Symbol für die Bildschirmtastatur.

Im Modus **INT** tippen Sie das Tempo mehrfach im Takt auf die Tap-Taste — je
gleichmäßiger, desto präziser das Ergebnis. In **EXT**/**PIO** übernimmt der
Controller Tempo und Taktphase aus der eingehenden OSC-Beatclock; die Tap-Taste sendet
weiterhin eine `/tap`-Nachricht, beeinflusst aber nicht mehr das eigene Tempo.

## 10. Parameterreferenz

Kompakte Übersicht aller in den Kapiteln 7–8 beschriebenen Parameter, mit
Standardwert.

| Sektion | Parameter | Bereich | Standard |
|---|---|---|---|
| Shape | Trajectory | Bibliothek | Square |
| Elevation | Reach | 0.05 – 1.0 | 0.5 |
| Elevation | Clip-Top | 0 – 1 | 0 |
| Elevation | Clip-Bottom | 0 – 1 | 0 |
| Elevation | Pole | Nord / Süd | Nord |
| Elevation | Flat | Aus / An | Aus |
| Elevation | Flat-Elv | 0 – 1 | 0.5 |
| Motion | Speed | 1/128 – 16 | 1 Takt |
| Motion | Direction | Fwd/Rev/PingPong | Forward |
| Motion | End-Action | Loop/Stop/Bounce | Loop |
| Filter | Sweep | 0 – 1 | 0 |
| Filter | Q | 0 – 1 | 0 |
| Global | Clockmode | INT/EXT/PIO | INT |
| Skin | potSize | Faktor | 1.0 |
| Skin | fontHeader | px | 18 |
| Skin | fontBody | px | 15 |
| Skin | sphereScale | Faktor | 0.62 |

## 11. Problembehandlung

| Problem | Lösung |
|---|---|
| Ein Regler reagiert nicht | Prüfen Sie, ob der richtige Slot ausgewählt ist (Settings-Pad drücken) und ob der Pot-Encoder auf den gewünschten Regler zeigt (mehrfach drücken, um durchzuschalten). |
| Clip startet nicht sofort | Wiedergabe/Aufnahme starten immer auf den nächsten Takt-Downbeat — kein Fehlverhalten. |
| Trajektorie wirkt verzerrt | Reach, Clip-Top/-Bottom und Flat prüfen (Kapitel 7.2) — insbesondere ein sehr niedriger Reach-Wert staucht die Form stark zusammen. |
| Tempo folgt nicht der Band | Clockmode in den globalen Einstellungen prüfen — INT ignoriert eingehende Beatclock-Daten (Kapitel 9). |

---

*A3 Motion — Benutzerhandbuch · A3 Audio*
