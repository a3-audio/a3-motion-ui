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

#include <array>
#include <string>
#include <vector>

#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/PatternLibrary.hh>

#include <a3-motion-ui/SettingsPersistence.hh>
#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/components/KeyboardComponent.hh>
#include <a3-motion-ui/components/SkinEditorComponent.hh>
#include <a3-motion-ui/io/AsyncOSCSender.hh>
#include <a3-motion-ui/io/InputOutputAdapter.hh>
#include <a3-motion-ui/osc/OscMessageHandler.hh>

namespace a3
{
class TempoEstimator;
class TempoEstimatorTest;

class MotionComponent;

class FilterDisplay;
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

class A3MotionUIComponent : public juce::Component,
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
  void padLEDCallback (int step);
  juce::Colour channelColourForPadStatus (juce::Colour base,
                                          Pattern::Status status,
                                          Pattern::Status statusLast,
                                          int step);
  juce::Colour scheduledForIdleLEDColour (juce::Colour base, int step,
                                          Pattern::Status statusLast);

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
  float getLengthBeats (index_t channel, index_t slot) const;
  // Speed knob: far left = speedLog2Max (16 bars, slowest), far right =
  // speedLog2Min (1/128 bar, fastest) — see updateClipSettingsDisplay()'s
  // speedFrac, which is deliberately inverted against these bounds.
  static constexpr auto speedLog2Min = -7; // 2^-7 bar = a 128th note
  static constexpr auto speedLog2Max = 4;  // 2^4 bar = 16 bars

  void createMainUI ();
  std::unique_ptr<MotionComponent> _motionComponent;
  std::unique_ptr<StatusBar> _statusBar;
  TempoClock::PointerT _statusBarCallbackHandle;
  std::unique_ptr<FilterDisplay> _filterDisplay;
  std::unique_ptr<LoopLengthDisplay> _loopLengthDisplay;
  std::unique_ptr<ElevationDisplay> _elevationDisplay;

  using Button = InputOutputAdapter::Button;
  constexpr bool runsOnHardware ();
  void createHardwareInterface ();
  void blankLEDs ();
  void handlePadPress (index_t channel, index_t pad);
  bool isButtonPressed (Button button);
  std::unique_ptr<InputOutputAdapter> _ioAdapter;

  void initializePatterns ();
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
  int trajectoryNameToIndex (std::string const &name) const;
  std::shared_ptr<Pattern> createPatternForIndex (int index, index_t channel);
  void saveRecordedPattern (std::shared_ptr<Pattern> const &pattern,
                            index_t channel, index_t slot);

  // OSC Receiver for beat clock (port 7771)
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
  /** Point config.json at another skin; the file watcher reloads it. */
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
  void  showKeyboard (bool shown);
  void  toggleKeyboard ();
  void  rebuildGlobalSettingsOptions ();
  // Index into potSizeScales/potSizeLabels — scales every knob/toggle in
  // ClipSettingsComponent uniformly (see ClipSettingsComponent::
  // setPotSizeScale()), adjustable live from the Global Settings menu.
  int _potSizeIndex = 1; // default 100%
  static constexpr float potSizeScales[] = { 0.75f, 1.0f, 1.25f, 1.5f,
                                             1.75f };
  static constexpr char const *potSizeLabels[] = { "75%", "100%", "125%",
                                                    "150%", "175%" };
  // Index into the theme's scale table; the factor itself lives there, because
  // a saved index has to become a factor at startup, before any menu exists.
  int _headerSizeIndex = 1; // default 100%
  int _bodySizeIndex = 1;   // default 100%
  std::unique_ptr<SkinEditorComponent> _skinEditor;
  std::unique_ptr<KeyboardComponent> _keyboard;
  bool _skinEditorOpen = false;
  juce::StringArray _skinNames;
  int _skinIndex = 0;
  static constexpr char const *fontSizeLabels[] = { "75%", "100%", "125%",
                                                     "150%", "175%" };

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
  void handleClipSettingsValueChange (index_t channel, int increment);
  void handleClipSettingsSubElementCycle (index_t channel);
  int numSubElementsForSection (int menuIndex) const;
  void updateClipSettingsDisplay ();

  struct ClipUIParams
  {
    // log2 of the pattern's playback length in bars — see speedLog2Min/Max.
    // 0 = 1 bar (native tempo), negative = faster (note-value fraction of
    // a bar), positive = slower (multiple bars per cycle).
    int speedLog2 = 0;
    int direction = 0;   // 0=Forward, 1=Reverse, 2=PingPong
    int endAction = 0;   // 0=Loop, 1=Stop, 2=Bounce
  };
  // [channel][slot], sized alongside _patterns in initializePatterns().
  std::vector<std::vector<ClipUIParams> > _clipUIParams;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (A3MotionUIComponent)
};

}
