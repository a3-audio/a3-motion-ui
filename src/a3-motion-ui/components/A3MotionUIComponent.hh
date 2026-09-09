/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#pragma once

#include "a3-motion-engine/tempo/TempoClock.hh"
#include <JuceHeader.h>

#include <atomic>
#include <mutex>

#include <array>
#include <string>
#include <vector>

#include <a3-motion-engine/RecMode.hh>
#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/ClipLocks.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/PatternLibrary.hh>
#include <a3-motion-ui/components/SphereProjection.hh>

#include <a3-motion-ui/SettingsPersistence.hh>
#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/components/ColourPickerComponent.hh>
#include <a3-motion-ui/components/OverlayButtons.hh>
#include <a3-motion-ui/components/OverlaySideStrips.hh>
#include <a3-motion-ui/SessionFile.hh>
#include <a3-motion-ui/components/BrowserComponent.hh>
#include <a3-motion-ui/components/LibraryKeys.hh>
#include <a3-motion-ui/components/LibraryList.hh>
#include <a3-motion-ui/components/ActionComponent.hh>
#include <a3-motion-ui/components/ControllerComponent.hh>
#include <a3-motion-ui/components/MixerComponent.hh>
#include <a3-motion-ui/components/MixerStripComponent.hh>
#include <a3-motion-ui/theme/ThemedComponent.hh>
#include <a3-motion-ui/components/SkinEditorComponent.hh>
#include <a3-motion-ui/io/AsyncOSCSender.hh>
#include <a3-motion-ui/io/InputOutputAdapter.hh>
#include <a3-motion-ui/osc/OscMessageHandler.hh>

namespace a3
{
class TempoEstimator;
class TempoEstimatorTest;

class MotionComponent;

class LoopLengthDisplay;
class ElevationDisplay;
class PadRowDisplay;
class StatusBar;
class ChannelStrip;
class GlobalSettingsComponent;
class ClipSettingsComponent;
class ChannelUIState;
class Pattern;
class HeightMapSphere;

class A3MotionUIComponent : public ThemedComponent,
                            public juce::Component,
                            public juce::Value::Listener,
                            public juce::MessageListener,
                            public juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>,
                            public OscMessageHandler::Listener,
                            private juce::Timer

{
public:
  A3MotionUIComponent (unsigned int const numChannels);
  ~A3MotionUIComponent ();

  void paint (juce::Graphics &g) override;
  void resized () override;

  float getMinimumWidth () const;
  float getMinimumHeight () const;

  void valueChanged (juce::Value &value) override;
  void handleMessage (juce::Message const &message) override;
  
  // OSC Receiver
  void oscMessageReceived (const juce::OSCMessage &message) override;
  void oscBundleReceived (const juce::OSCBundle &bundle) override;

  // OscMessageHandler::Listener
  void onChannelVU (int channel, float peak, float rms) override;
  void onSubwooferVU (float peak, float rms) override;
  void onEnergyGrid (float const *values, int count) override;
  void onSpeakerVU (int speakerIndex, float peak, float rms) override;
  void onExternalBeatClock (int beat, int bar, float bpm) override;
  void onExternalBeatSync (int beat, int beatsPerBar) override;

private:
  std::unique_ptr<HeightMapSphere> _heightMap;
  MotionEngine _engine;
  std::unique_ptr<OscMessageHandler> _oscMessageHandler;

  void tickCallback (Measure measure);
  /** How long the panel's TAP key stays lit on a beat. Longer than the
   *  screen's wash, which is a repaint; this is a serial write, and one that
   *  came and went inside a frame would read as noise on the link. */
  static constexpr int tapBeatLedMillis = 90;

  /** Push the channel grid's three rows, 3d with the accent laid over it. */
  void refreshChannelValues ();

  /** Write every function key's LED from the one rule the strip paints by. */
  void updateFunctionKeyLEDs ();
  /** Blink the panel's TAP key with the beat — at full, unlike the screen's
   *  wash: an LED in a dark booth needs its whole colour to read as a beat. */
  void pulseTapLED ();
  void padLEDCallback (int step);
  juce::Colour channelColourForPadStatus (juce::Colour base,
                                          Pattern::Status status,
                                          Pattern::Status statusLast,
                                          int step);

  Measure _now;
  juce::Value _valueBPM;
  TempoClock::PointerT _tickCallbackHandle;
  TempoClock::PointerT _padLEDCallbackHandle;
  static auto constexpr stepsPerBeatPadLEDs = 4;
  static auto constexpr ticksPerStepPadLEDs
      = TempoClock::getTicksPerBeat () / stepsPerBeatPadLEDs;
  int _stepsLED = 0;

  std::unique_ptr<TempoEstimatorTest> _tempoEstimatorTest;

  LookAndFeel_A3 _lookAndFeel;

  void createChannelsUI ();
  std::vector<std::unique_ptr<ChannelStrip> > _channelStrips;
  std::vector<std::unique_ptr<ChannelUIState> > _channelUIStates;

  // Speed (Motion section): per-clip, quantized to musical note-value
  // fractions/multiples of a bar (see ClipUIParams::speedLog2) rather than
  // a free-running multiplier — playback length is literally how many
  // beats one full pattern cycle spans, so this doubles/halves that.
  /** What the pattern in this slot is, in beats — its own length. */
  float getPatternLengthBeats (index_t channel, index_t slot) const;

  /** How long one traversal takes when played, at this clip's rate. */
  float getLengthBeats (index_t channel, index_t slot) const;

  /** The same length as a Measure, counted in ticks.
   *
   *  Every caller used to build it from a whole number of beats clamped up to
   *  one, which cannot say "half a beat" and so could not say anything faster
   *  than one beat per traversal either. */
  Measure getPlaybackLength (index_t channel, index_t slot) const;

  /** Push the clip's direction and end action into the pattern, which is where
   *  the engine reads them. */
  void applyMotionMode (index_t channel, index_t slot);
  // Speed knob: far left = speedLog2Max (16 bars, slowest), far right =
  // speedLog2Min (1/128 bar, fastest) — see updateClipSettingsDisplay()'s
  // speedFrac, which is deliberately inverted against these bounds.
  static constexpr auto speedLog2Min = -7; // 2^-7 bar = a 128th note
  static constexpr auto speedLog2Max = 4;  // 2^4 bar = 16 bars

  void createMainUI ();
  std::unique_ptr<MotionComponent> _motionComponent;
  std::unique_ptr<StatusBar> _statusBar;
  TempoClock::PointerT _statusBarCallbackHandle;
  std::unique_ptr<LoopLengthDisplay> _loopLengthDisplay;
  std::unique_ptr<ElevationDisplay> _elevationDisplay;

  using Button = InputOutputAdapter::Button;
  constexpr bool runsOnHardware ();
  void createHardwareInterface ();
  void blankLEDs ();
  /** Show one of the bar's two pages, and put the screen's modifiers down. */
  void showBarPage (BarPage page);
  /** Record's page gesture: to the take's face, or back off it. */
  void toggleRecordPage ();
  /** Push the shown clip's envelope and act mode to the ACTION page. */
  /** Give the chosen field's slot an action, or take its action away. */
  void assignActionEntry (int row);

  /** Point a slot's ACT key at an action file, reading it in. An empty file,
   *  or one that will not read, leaves the slot firing the accent alone. */
  void setSlotAction (index_t channel, index_t slot, juce::File const &file);

  /** Put what is in the editor back into the slot's script file. */
  void writeSlotActionScript ();
  /** Keep the chosen slot's settings as a new action clip. */
  juce::String saveSlotAsAction ();
  /** Write what is on show back over the file it came from. Per tab: the
   *  slot's own clip, the slot's own action, the set that is loaded. */
  void saveChosen ();
  /** Write it to a new file instead, leaving the one it came from alone. */
  void saveAsChosen ();
  /** Whether there is a file to write back over -- what lights Save, as
   *  against Save as, which only needs something to write. */
  bool canSaveInPlace () const;
  /** The shown clip copied to a clip file of its own, the slot pointed at the
   *  copy. The same thing Save does to a factory clip, asked for outright. */
  juce::String saveSlotClipAsCopy ();
  /** The shown clip's settings written over the slot's own action file. */
  void saveSlotActionInPlace ();
  /** The arrangement written over the set it was loaded from. */
  void saveSessionInPlace ();
  /** Give the chosen row another name, and carry across everything that named
   *  the old one. What that means follows the folder the list is on -- see
   *  renameChosenAction/Set/Clip. */
  void renameChosenEntry (juce::String const &name);
  /** Throw the chosen row's file away. Asked twice -- see `_deleteArmed`. */
  void deleteChosenEntry ();
  /** Whether the chosen row is one that can be renamed or thrown away: it has
   *  to stand for a file, and row zero of the clips and actions lists stands
   *  for "nothing chosen". */
  bool chosenEntryHasAFile () const;
  bool chosenEntryIsShipped () const;
  LibraryKeyStates currentLibraryKeys () const;

  /** The browser's four lists as four objects rather than four branches at
   *  every decision. Nested so they reach what they delegate to without any
   *  of it being opened up; see components/LibraryList.hh for why they exist
   *  at all. */
  class LibraryBackedList;
  class LibraryBackedList;
  class ClipsList;
  class ShapesList;
  class ActionsList;
  class SetsList;
  std::array<std::unique_ptr<LibraryList>, 4> _lists;

  void createBrowserLists ();

  /** The one the tab is showing. */
  LibraryList &currentList () const;
  void saveSlotShapeInPlace ();
  juce::String saveSlotShapeAsCopy ();
  /** What the arm step of a delete says beyond the word: how much else goes
   *  with the file, in sets that name it. Empty when nothing does. */
  juce::String costOfRemovingLibraryEntry (int index) const;

  void renameChosenAction (juce::String const &name);
  void deleteChosenAction ();
  void renameChosenSet (juce::String const &name);
  void deleteChosenSet ();
  void renameChosenClip (juce::String const &name);
  void deleteChosenClip ();

  /** The action file the browser's chosen row stands for, or nothing: row
   *  zero is "no action" and has no file behind it. */
  juce::File chosenActionFile () const;
  /** The set file the browser's chosen row stands for, or nothing. */
  juce::File chosenSetFile () const;
  /** The next library entry of the same kind as `from`, wrapping. The Shape
   *  section's two controls walk one kind each: the picture the figures, the
   *  field under it the settings presets. -1 when there are none of that
   *  kind. */
  int stepThroughLibrary (int from, int increment, bool settings) const;

  /** The library entry the browser's chosen row stands for, or -1. */
  int chosenLibraryIndex () const;

  /** Row numbers and library indexes are two different things once a list can
   *  be narrowed. Both translators ask the list object, which built the
   *  mapping in the same pass that built its rows; -1 means "not on the list
   *  as it is narrowed". */
  int browserRowForLibrary (int entry) const;
  int libraryForBrowserRow (int row) const;

  /** What the clips list is narrowed to.
   *
   *  The three the library can actually tell apart: the instrument's own
   *  shapes, everything the performer recorded or dialled, and both. There is
   *  no such split in the actions or the sets -- shipped and hand-written land
   *  in one folder there with nothing marking which is which -- so the key is
   *  dark on those two tabs rather than offering a choice it cannot make. */

  /** How many saved sets name `patternName` in one of their slots. */
  int setsNaming (juce::String const &patternName) const;
  /** Write `to` wherever the sets say `from`, and answer how many sets were
   *  rewritten. A set names its shapes rather than carrying them, so a shape
   *  renamed without this leaves every set that used it holding empty slots --
   *  silently, because a name a set cannot resolve is not an error there, it
   *  is a slot nobody filled. */
  int renameInSets (juce::String const &from, juce::String const &to);
  void updateActionPage ();
  void applyActionControl (int control, int increment);
  void resetActionControl (int control);
  static juce::String actionReadoutFor (int control,
                                        Pattern const &pattern);

  /** The browser page: what the eight fields hold, and what dropping a
   *  library row on the chosen one does. */
  /** Fill a slot from the library, or empty it with index 0.
   *
   *  The one door: three places used to assign _patterns[c][s] directly, and
   *  each of them would have had to remember the clip file too. One that
   *  forgot would leave a slot that could never be saved and never showed as
   *  drifted -- silently, because nothing about it would look wrong. */
  void fillSlotFromLibrary (index_t channel, index_t slot, int libIndex);
  /** Apply a clip that names no shape: the values change, the movement in the
   *  slot stays. Does nothing on an empty slot -- see the definition. */
  /** Put a whole clip into a slot: the figure it names and every value it
   *  carries. A clip written before a clip had to name a figure leaves the
   *  slot's own alone and lands only its values. */
  void applyClip (index_t channel, index_t slot, int index);
  /** Read direction and end action back out of the pattern into the strip.
   *  Both live in two places, and the pattern is the one a clip writes. */
  void syncClipUIParamsFromPattern (index_t channel, index_t slot);

  /** Whether this slot has drifted from the clip it was filled from. Asked,
   *  not remembered -- see clipHasDrifted(). */
  bool slotHasDrifted (index_t channel, index_t slot) const;

  /** Write this slot's settings back into its clip.
   *
   *  On a factory clip it makes a copy instead and points the slot at it. Not
   *  an error and not a disabled key: a locked control that explains why it is
   *  locked has already cost you the reach. */
  void saveSlotClip (index_t channel, index_t slot);

  /** The device's eight slots and four channels, as a session. */
  Session buildSession ();

  /** Where the named sessions live, beside the takes they refer to. */
  juce::File sessionsDir () const;
  /** Where action clips live, beside the clips and the sessions. */
  juce::File actionsDir () const;

  /** Put the current arrangement away under a free name, and fetch one back.
   *
   *  Loading stops everything: all eight slots get new contents, and a clip
   *  still running while its slot holds something else is exactly the state
   *  that dropping a clip already avoids. A set is changed between sets.
   *
   *  The running arrangement is written to current.json first -- not asked
   *  about, written -- so the previous state is never gone even if nobody
   *  thought to save it. */
  juce::String saveCurrentSession ();
  void loadSessionNamed (juce::String const &name);

  void refreshBrowser ();
  void assignBrowserEntry (int index);

  void handlePadPress (index_t channel, index_t pad);
  /** The other half of a pad gesture. Shift+Action previews for as long as it
   *  is held, so a press without a release leaves the channel previewing. */
  void handlePadRelease (index_t channel, index_t pad);
  bool isButtonPressed (Button button);
  std::unique_ptr<InputOutputAdapter> _ioAdapter;

  void initializePatterns ();

  /** Where the set lives: `setFile` in config.json, or `set.json` beside the
   *  pattern folder. A folder with a set and the takes it names is a gig on a
   *  stick, which is the whole reason it is a file of its own. */
  juce::File setFilePath () const;
  /** Put a loaded set in force: which take sits where, the length the next
   *  take into each slot gets, and where the channels are parked. */
  void applySet ();
  void applySet (juce::File const &file);
  /** Write the set out shortly. Debounced: a drag on the grid is dozens of
   *  changes and one arrangement. */
  void scheduleSetSave ();
  void writeSet ();
  // _patterns[channel][slot]: 2 clip slots per channel (see numClipSlots).
  std::vector<std::vector<std::shared_ptr<Pattern> > > _patterns;

  // Pattern library: manages system/ and user/ pattern files
  std::unique_ptr<PatternLibrary> _patternLibrary;

  // Pad row display (trajectory option bar) — one row per clip slot.
  static auto constexpr numClipSlots = 2u;
  void createPadRowDisplays ();
  std::vector<std::unique_ptr<PadRowDisplay> > _padRowDisplays;

  void updatePadRowLabel (index_t channel, index_t slot);
  void setPreviewWithDisplayData (std::shared_ptr<Pattern> const &pattern);
  /** Register display data with MotionComponent for playing-trajectory rendering. */
  void registerPatternDisplayData (std::shared_ptr<Pattern> const &pattern);

  /** Rebuild a pattern's drawn trajectory from its own ticks.
   *
   *  registerPatternDisplayData() takes the shape from the library's file,
   *  which is right while the pattern is what the file says. Once the fade has
   *  rewritten the ticks, the file is a picture of what the clip used to be:
   *  the blob follows the new ending and the line still shows the old one. */
  void refreshPatternDisplayFromTicks (
      std::shared_ptr<Pattern> const &pattern);
  int trajectoryNameToIndex (std::string const &name) const;
  std::shared_ptr<Pattern> createPatternForIndex (int index, index_t channel);
  void saveRecordedPattern (std::shared_ptr<Pattern> const &pattern,
                            index_t channel, index_t slot);

  // OSC Receiver for beat clock (port 7771)
  /** The addresses this device speaks, read from config.json. Pushed on to
   *  the engine and the message handler whenever the config is reloaded —
   *  see applyOscAddresses(). */
  OscAddresses _oscAddresses;
  void applyOscAddresses (juce::var const &config);

  /** The beat address again, for the tempo-clock thread.
   *
   *  tickCallback() runs there, and juce::String is reference counted — so
   *  reading _oscAddresses.beat from it while the message thread replaces
   *  the struct is a race. Handed over the same way the send backend gets
   *  its addresses: stored under the lock, picked up at the top of the tick
   *  where the flag costs one atomic load. */
  juce::String _beatAddress{ "/beat" };
  std::mutex _beatAddressMutex;
  juce::String _pendingBeatAddress;
  std::atomic<bool> _beatAddressPending{ false };
  void applyPendingBeatAddress ();

  juce::OSCReceiver _oscReceiver;
  // OSC Receiver for VU meters (port 7772)
  juce::OSCReceiver _oscReceiverVU;
  juce::OSCReceiver _oscReceiverEnergy;

  // Whether the sphere stops rendering while the settings menu is open. Off:
  // it was a concession to the RPi4's GPU, and the rig is on an Intel NUC.
  bool _pauseRenderingInMenu = false;
  
  // Async OSC Sender for beatclock output (non-blocking, dedicated thread)
  AsyncOSCSender _oscSender;
  
  // Direct OSC Sender for time-critical tap messages (bypasses async queue)
  juce::OSCSender _tapSender;
  
  // ClockMode toggle state: 0 = INT, 1 = EXT, 2 = PIO
  int _clockMode = 0;
  float _internalBPM = 0.f;  // saved INT tempo for restore after EXT mode

  // Global Settings: device-wide menu (Clockmode, Pot Size, Font Size —
  // Elevation Map moved to being a per-clip setting, see
  // ClipSettingsComponent), opened by simultaneous press of buttons 50+59
  // (Button::Menu). Shares the bottom-quarter settings area with
  // ClipSettingsComponent.
  std::unique_ptr<GlobalSettingsComponent> _globalSettings;
  std::unique_ptr<OverlayButtons> _overlayButtons;
  std::unique_ptr<OverlaySideStrips> _overlayStrips;

  /** The software mixer, over the sphere, reached from the MIX key in the
   *  status bar rather than from a tab in the settings bar: it has nothing to
   *  do with the clip that bar describes. */
  std::unique_ptr<MixerComponent> _mixer;
  /** The same strip for one channel, as the settings bar's MIX page. The
   *  overlay above is the whole mixer when you want it; this is the reach to
   *  the channel of the clip the bar is already describing. Both draw from
   *  the same MixerState and both gestures land in the same two handlers. */
  std::unique_ptr<MixerStripComponent> _mixerStrip;
  /** The values it shows, and the one place that knows a value has been
   *  touched and therefore has to go out on the wire. Held by this component
   *  rather than by the overlay, so what Core is told does not depend on
   *  whether anybody is looking at it. */
  MixerState _mixerState;
  /** The levels its meters read. Beside _mixerState rather than inside it:
   *  MixerState is what a finger has set and what therefore goes out on the
   *  wire, and a meter is the opposite -- what came back, and never sent.
   *
   *  Written by all three VU callbacks *in addition to* what they already do:
   *  the channels keep filling _channelUIStates for the sphere's coronas, the
   *  subwoofer keeps reaching setSphereGlow and the speakers setSpeakerLight.
   *  Those three have no other source, so a redirected callback would take
   *  the sphere its glow and the speaker lights their light. */
  VuLevels _vuLevels;
  bool _mixerOpen = false;
  void toggleMixer ();
  /** Open or close it and tell everything that shows the state -- the key in
   *  the status bar and the overlay's own buttons. One place, because Back,
   *  Close and the key itself all reach it. */
  void showMixer (bool open);
  bool  _globalSettingsOpen        = false;
  bool  _globalSettingsValueFieldSelected = false;
  // 0 = Clockmode, 1 = Pot Size, 2 = Font Size
  int   _globalSettingsOptionIndex = 0;
  void  openGlobalSettings ();
  void  closeGlobalSettings ();
  void  confirmGlobalSettingsOption ();
  void  applyClockMode (int mode);
  void  applyPotSize (int index);
  void  applyHeaderSize (int index);
  /** Push a changed font size out to everything that cannot re-read it on
   *  its own, then persist. */
  void  refreshFonts ();
  void  applyBodySize (int index);
  /** The sphere's share of the shorter side, written into the active skin
   *  where it lives. */
  /** Point config.json at another skin; the file watcher reloads it. */
  /** Show a skin without making it the one that runs. */
  void  previewSkin (int index);
  void  applySkin (int index);
  /** Open the skin editor as a page of the settings menu, close it again
   *  (which is when the edited skin is written), put an edit in force, and
   *  save. */
  void  openSkinEditor ();
  void  closeSkinEditor ();
  void  applyEditedSkin ();
  void  saveEditedSkin ();
  void  saveSkinAsNew ();
  void  renameEditedSkin (juce::String const &name);
  void  deleteEditedSkin ();
  /** Load `name` into the editor and refresh the menu's list of skins. */
  void  reopenEditorOn (juce::String const &name);
  /** Show or hide the on-screen keyboard, and light the status bar's icon
   *  to match. */
  /** Open a slice of config.json as an editor page, and write it back on
   *  the way out. */
  void  openConfigPage (juce::String const &title,
                        juce::StringArray const &keys);
  /** Put an edited config page into the running configuration at once,
   *  so the hardware follows the picker instead of the file watcher. */
  void applyEditedConfigPage ();
  void  saveConfigPage ();
  void  applyPauseRendering (bool paused);

  /** Takes the menu row's index, not a mode: the row offers a list, and the
   *  list's order is the only thing that turns one into the other. */
  void  applyRecMode (int index);

  /** Put a skin in force by name, refreshing the list first — a skin that was
   *  only just written is not in it yet. */
  void  applySkinNamed (juce::String const &name);

  /** Put every value of the skin being edited back to the shipped default,
   *  keeping its name. */
  void  resetEditedSkinToDefault ();
  /** The picker behind a colour row: HSL to reach it, r/g/b in the file.
   *  `colour` is resolved (file value or theme default) by the caller, so
   *  the picker never re-reads the raw document and opens on black for a
   *  role the skin does not name. */
  void  openColourPicker (juce::String const &path, juce::Colour colour);
  void  closeColourPicker ();
  void  applyPickedColour ();
  /** Re-read what this component caches from the theme: the channel
   *  colours, which every blob, pad and frame is drawn in. */
  void  applyTheme () override;
  void  showKeyboard (bool shown);
  void  toggleKeyboard ();
  /** Light the status bar's icon for what the browsed row allows. */
  void  refreshKeyboardIcon ();
  void  rebuildGlobalSettingsOptions ();

  /** The menu's rows, by name. They were numbered by hand, and inserting one
   *  in the middle silently moved every row after it — the encoder then set
   *  the wrong thing, which is exactly what happened when the font size was
   *  split into two. The order here and the order they are pushed in
   *  rebuildGlobalSettingsOptions() are the same list. */
  /** What each menu row does. The order is not the contract — see
   *  _menuRowOrder, which is filled next to the rows themselves. Two parallel
   *  lists that had to agree by position is what broke the menu once: three
   *  rows were removed and their entries here were not, so every row below the
   *  first was read as a different one and the presses did nothing. */
  enum class MenuRow
  {
    Skin,
    SkinEditor,
    Network,
    ButtonLeds,
    PatternFolder,
    SphereInMenu,
  };

  /** What the browsed menu position does, or nothing if it is out of range. */
  std::optional<MenuRow> browsedMenuRow () const;

  /** The row each menu position stands for, filled where the rows are built.
   *  A row added or removed in one place is now impossible to forget here. */
  std::vector<MenuRow> _menuRowOrder;

  // Index into the theme's scale table; the factor itself lives there, because
  // a saved index has to become a factor at startup, before any menu exists.
  /** What a recording pass writes where the finger is not. A device setting,
   *  so it is persisted beside the clock mode rather than in the skin. */
  RecMode _recMode = RecMode::Touch;

  int _headerSizeIndex = 1; // default 100%
  int _bodySizeIndex = 1;   // default 100%
  std::unique_ptr<SkinEditorComponent> _skinEditor;
  std::unique_ptr<ColourPickerComponent> _colourPicker;
  bool _colourPickerOpen = false;
  juce::String _colourPath;
  bool _skinEditorOpen = false;
  /** Empty while a skin is being edited; the keys of the slice otherwise. */
  juce::StringArray _configPageKeys;
  juce::StringArray _skinNames;
  int _skinIndex = 0;
  /** The sphere's share of the shorter side. 0.62 is what the tuned look
   *  ships with and sits in the middle. */


  // Persisted UI preferences (Clockmode, Pot Size, Font Size) — a small
  // JSON file alongside config.json (see StandaloneApp.cc's config.json
  // load — same getCurrentWorkingDirectory()-relative resolution), read
  // once at startup and rewritten whenever one of those actually changes,
  // so they survive app restarts instead of resetting to defaults.
  juce::File getConfigFile () const;
  juce::File getPersistedSettingsFile () const;

  // Pattern directory monitoring
  void timerCallback () override;
  void refreshAllPadRowLabels ();
  juce::int64 _lastLibraryFingerprint = 0;

  // Preview-and-fire: holding Shift + Action for a channel's clip slot plays
  // it in preview mode (OSC output silenced); releasing Action exits preview
  // (pattern keeps playing). -1 means no preview active on that channel.
  std::vector<int> _previewHeldPad; // holds the SLOT index, or -1
  /** Which slot, if any, the Action key is currently holding on each channel
   *  -- the clip that has to stop the moment the finger lifts. -1 for none. */
  std::array<int, numChannelsInitial> _actHeldSlot{ -1, -1, -1, -1 };

  /** Shift held on the screen rather than on the panel, folded into
   *  isButtonPressed() so nothing downstream has to know which of the two a
   *  hand is on. Record needs no twin: the global strip's REC button already
   *  records into the shown clip. */
  std::unique_ptr<ControllerComponent> _controller;
  std::unique_ptr<ActionComponent> _action;
  /** Which clip each slot was filled from, [channel][slot]. Empty for a slot
   *  holding nothing, or one holding a shape that has no clip. */
  std::vector<std::vector<juce::File> > _slotClipFile;
  /** The action clip the ACT key fires on this slot, if any. Per slot,
   *  not per clip: two slots on one clip can act differently.
   *
   *  The file and its contents together, in one struct rather than two
   *  vectors, because they have to agree: the file is what the browser
   *  highlights and the ACTION page names, and the settings are what ACT
   *  actually fires. Read once, when the action is assigned -- a press has to
   *  land on the beat, and a press that opened a file would not. */
  struct SlotAction
  {
    juce::File file;
    /** The script as written, for the page to show and later to edit. */
    juce::String source;
    /** What it worked out to when it was chosen. Empty when the slot fires
     *  nothing, or when the file would not read. */
    std::optional<ClipSettings> settings;
    /** What the script got wrong, line by line. Shown rather than swallowed:
     *  the only compiler here is the one that just ran. */
    juce::StringArray errors;
  };
  std::vector<std::vector<SlotAction> > _slotAction;
  /** Which of the bar's three sections is being held. The device's, not a
   *  clip's: a stance taken while playing and dropped again, so it is in
   *  neither the clip file nor the set. See ClipLocks. */
  ClipLocks _clipLocks;

  ClipFilter _clipFilter = ClipFilter::All;
  /** Which library entry each row of the browser stands for. The list is a
   *  window onto the library once it can be narrowed, so a row's number and an
   *  entry's number are no longer the same thing -- and everything that acts
   *  on a chosen row goes through here. */

  /** Whether the browser's delete key has been pressed once already. A file
   *  thrown away in front of a room does not come back, so it takes two
   *  presses -- and anything else that happens disarms it, because an armed
   *  key you have forgotten about is worse than no key. */
  bool _deleteArmed = false;

  /** Which slot each channel's face stands for. Per channel rather than one
   *  shared setting: the two slot keys used to be shared, so choosing slot 2
   *  chose it for whichever channel you happened to be on, and comparing the
   *  same slot across two channels took two moves instead of one. */
  std::array<index_t, numChannelColumns> _channelSlot{};
  /** Whether the shown channel's accent was running at the last timer tick,
   *  so that the one after it still redraws. See timerCallback(). */
  bool _accentWasActive = false;

  std::unique_ptr<BrowserComponent> _browser;
  /** What the browser's list is showing: the clips you can put in a slot, the
   *  actions the ACT key can fire on one, or the sessions you can put in all
   *  eight. */
  BrowserList _browserList = BrowserList::Clips;
  /** The session that was loaded, shown over the eight slots it filled. */
  juce::String _sessionName;
  /** Counts up on every scheduled save so a later one supersedes an earlier:
   *  a drag on the grid is dozens of changes and one arrangement. */
  int _setSaveGeneration = 0;

  /** Which page the bar is on. Kept here as well as in the bar because a
   *  value's *meaning* can depend on it — Shape's knob is the rotation on one
   *  face and the take's fade on the other — and the handler that changes
   *  values lives here. */
  BarPage _barPage = BarPage::Clip;

  bool _screenShiftHeld = false;

  // Clip Settings: permanent bottom panel showing the last-selected clip's
  // settings. Selected by a slot's Settings button; the Motion-
  // Encoder (upper, per channel) scrolls its 4 menu items, the Pot-Encoder
  // (lower) changes the value of whichever (sub-)item is selected.
  // Trajectory Shape/Sweep/Q/Speed are real (pattern selection / audio
  // filter / playback length, see ClipUIParams::speedLog2); Direction/
  // End-Action are still UI-only placeholders (no engine parameter exists
  // for them yet).
  std::unique_ptr<ClipSettingsComponent> _clipSettings;
  index_t _clipSettingsChannel = 0;
  index_t _clipSettingsSlot = 0;
  int _clipSettingsMenuIndex = 0; // 0..3, see ClipSettingsComponent
  // Which sub-element of the current section the Pot-Encoder edits, cycled
  // by pressing it (e.g. Elevation: 0 = reach, 1 = clip-top, 2 = clip-
  // bottom, 3 = mirror-south, 4 = flat, 5 = flat-elevation; Motion: 0 =
  // speed, 1 = direction, 2 = end-action; Filter: 0 = sweep, 1 = Q).
  // Reset to 0 whenever the section changes; Shape only has one.
  int _clipSettingsSubIndex = 0;
  void selectClip (index_t channel, index_t slot);
  void updateControlReadout (juce::String const &text);
  void handleClipSettingsScroll (index_t channel, int increment);
  /** Select outright, where the handlers above cycle — what a tap does. */
  void selectClipSettingsSection (int index);
  void selectClipSettingsSubElement (int index);

  /** One channel's freq/Q/3d, from the global section's grid. Named by
   *  channel, not by what the bar is showing. */
  void handleChannelValueChange (index_t channel, int row, int increment);

  /** Arm and start a take on this slot. Reached from the hardware (Record
   *  held while a Play|Pause pad is pressed) and from the bar's Rec button,
   *  which has no pad to name a slot and so uses the one on show. */
  void startRecording (index_t channel, index_t slot);

  /** What the bar's three action buttons do. Each is what the matching
   *  hardware key does, minus what only a key can do — Record is a modifier
   *  there, and a finger cannot hold it while pressing a pad. */
  void toggleGlobalSettings ();
  /** Out of every overlay at once, however deep. The Menu key can only step
   *  back one at a time; the close button is the way out. */
  void closeAllOverlays ();
  /** Shows or hides the back/close pair according to whether anything is
   *  open, and puts it in the corner. */
  void updateOverlayButtons ();
  void toggleRecordingOnShownClip ();
  void handleScreenTap ();
  /** A tap, from the hardware key or from the bar's button. */
  void handleTapAt (juce::int64 tapTimeMicros);
  /** Change one control's value by an increment. Which control is named
   *  explicitly rather than read off the current selection, so two fingers
   *  on two controls change two values instead of both driving the one that
   *  was selected last. The encoder path passes the selection it drives. */
  /** Two taps on a knob: back to the middle of its range. */
  void handleClipSettingsReset (index_t channel, int section, int sub);
  void handleClipSettingsValueChange (index_t channel, int section, int sub,
                                      int increment);

  /** Flip a two-state control. Separate from the increment path because a
   *  tap carries no direction — see ClipSettingsLayout's tapTogglesValue(). */
  void handleClipSettingsToggle (index_t channel, int section, int sub);
  void handleClipSettingsSubElementCycle (index_t channel);
  int numSubElementsForSection (int menuIndex) const;
  void updateClipSettingsDisplay ();
  void updateStatusBarPlayheads ();
  /** Where the sphere is being looked at from, or straight down if there is
   *  no sphere yet -- this runs while the interface is still being built. */
  SphereCamera sphereCamera () const;

  /** What is left of the per-slot table. Speed and fade moved to the Pattern:
   *  the engine reads them and they have to survive being saved, which a table
   *  living only in the screen cannot do. */
  struct ClipUIParams
  {
    int direction = 0;   // 0=Forward, 1=Reverse
    int endAction = 0;   // 0=Loop, 1=Stop, 2=Bounce, 3=Random
    // 0=Glide, 1=Hard — what happens to the stretches a recording never
    // wrote, including the one across the loop point. See closeRecordingSeams.
    // log2 of the length the NEXT take will have, in bars. A setting, not a
    // property of what is in the slot: an existing pattern's length is its
    // tick count, and changing that would throw its data away.
    int recordLengthLog2 = 0;
  };
  // [channel][slot], sized alongside _patterns in initializePatterns().
  std::vector<std::vector<ClipUIParams> > _clipUIParams;

  /** Which slot the running recording belongs to, and what that slot held
   *  before it started. The slot's pattern is replaced the moment recording
   *  begins, so a take that turns out empty can only be undone by putting the
   *  old one back. */
  std::optional<std::pair<index_t, index_t> > _recordingSlot;
  std::shared_ptr<Pattern> _patternBeforeRecording;
  /** Set when a take ends and started once its Stopped message arrives —
   *  stopping is asynchronous, and playing before it lands leaves the pattern
   *  in a state the Play pad does not recognise. */
  std::shared_ptr<Pattern> _playWhenRecordingStops;
  /** Counts the 50 ms timer, so the directory check keeps its old pace. */
  int _timerTick = 0;

  /** Finish the running recording: close what it never wrote, keep it or throw
   *  it away, and carry on playing. */
  void endRecording ();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (A3MotionUIComponent)
};

}
