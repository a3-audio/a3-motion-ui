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

#include <a3-motion-ui/io/OnScreenKeyboard.hh>
#include "A3MotionUIComponent.hh"

#include <chrono>
#include <fstream>
#include <iostream>

#include <a3-motion-engine/Config.hh>
#include <a3-motion-engine/PlaybackRate.hh>
#include <a3-motion-engine/RecordingSeam.hh>
#include <a3-motion-engine/TrajectoryShape.hh>
#include <a3-motion-engine/RecordingSpans.hh>
#include <a3-motion-ui/components/PatternProgressBar.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/PatternFile.hh>
#include <a3-motion-engine/PatternLibrary.hh>
#include <a3-motion-engine/UserConfig.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-engine/elevation/HeightMap.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <a3-motion-ui/Config.hh>
#include <a3-motion-ui/Helpers.hh>
#include <a3-motion-ui/components/ChannelStrip.hh>
#include <a3-motion-ui/components/ChannelUIState.hh>
#include <a3-motion-ui/components/FilterDisplay.hh>
#include <a3-motion-ui/components/LayoutHints.hh>
#include <a3-motion-ui/components/LoopLengthDisplay.hh>
#include <a3-motion-ui/components/ElevationDisplay.hh>
#include <a3-motion-ui/components/MotionComponent.hh>
#include <a3-motion-ui/components/PadRowDisplay.hh>
#include <a3-motion-ui/components/GlobalSettingsComponent.hh>
#include <a3-motion-ui/components/ClipSettingsComponent.hh>
#include <a3-motion-ui/components/StatusBar.hh>

#include <a3-motion-ui/tests/TempoEstimatorTest.hh>

#include <a3-motion-ui/io/InputOutputAdapter.hh>
#ifdef HARDWARE_INTERFACE_V2
#include <a3-motion-ui/io/InputOutputAdapterV2.hh>
#endif
#ifdef HARDWARE_INTERFACE_V3
#include <a3-motion-ui/io/InputOutputAdapterV3.hh>
#endif

#include <algorithm>
#include <array>

namespace a3
{

namespace
{
/** The modes the Automation row offers, in the order it offers them. The row
 *  hands back an index, so this list is what an index means; Read is absent on
 *  purpose — see where the row is built. */
constexpr std::array<RecMode, 3> recMenuModes{
  RecMode::Touch, RecMode::Latch, RecMode::Write
};

/** The fade in ticks. Sixteenths of a beat are what the panel offers, because
 *  that is a length a musician can hear; ticks are what the pattern counts. */
index_t
fadeTicksFor (int sixteenths)
{
  return static_cast<index_t> (std::max (0, sixteenths))
         * ticksPerFadeStep (TempoClock::getTicksPerBeat ());
}

int
recMenuIndex (RecMode mode)
{
  auto const found = std::find (recMenuModes.begin (),
                                recMenuModes.end (), mode);
  return found == recMenuModes.end ()
             ? 0
             : static_cast<int> (found - recMenuModes.begin ());
}
}


A3MotionUIComponent::A3MotionUIComponent (unsigned int const numChannels)
    : _heightMap (std::make_unique<HeightMapSphere> ()),
      _engine (numChannels, *_heightMap)
{
  setLookAndFeel (&_lookAndFeel);

  _oscMessageHandler = std::make_unique<OscMessageHandler> (_engine, *this);

  if (runsOnHardware ())
    {
      createHardwareInterface ();
    }

  // Initialize pattern library (creates system/ and user/ dirs if needed)
  // Path is configurable via "patternDir" in config.json.
  auto patternsDir = userConfig.hasProperty ("patternDir")
      ? juce::File (userConfig["patternDir"].toString ())
      : juce::File ("/home/aaa/a3-motion-ui/pattern");
  _patternLibrary = std::make_unique<PatternLibrary> (patternsDir);
  _lastLibraryFingerprint = _patternLibrary->getDirectoryFingerprint ();

  initializePatterns ();

  createChannelsUI ();
  createMainUI ();
  createPadRowDisplays ();

  // Global Settings (hidden by default). A child of MotionComponent, not of
  // this component: MotionComponent's OpenGL context is attached with
  // component painting enabled, so it draws its own children over the
  // rendered image. That is the only way anything can sit on top of the
  // sphere — and the only way the menu can be see-through and still show
  // the skin it is editing behind it.
  _globalSettings = std::make_unique<GlobalSettingsComponent> ();
  _globalSettings->setAlwaysOnTop (true);
  _motionComponent->addChildComponent (*_globalSettings);

  // The editor is a page of that menu and lives in the same place, for the
  // same reason: what it changes is mostly the sphere behind it.
  _skinEditor = std::make_unique<SkinEditorComponent> ();
  _skinEditor->setAlwaysOnTop (true);
  _motionComponent->addChildComponent (*_skinEditor);
  _skinEditor->onValueChanged = [this] { applyEditedSkin (); };
  _skinEditor->onSave = [this] { saveEditedSkin (); };
  _skinEditor->onSaveAsNew = [this] { saveSkinAsNew (); };
  _skinEditor->onRename = [this] (auto const &name) { renameEditedSkin (name); };
  _skinEditor->onReset = [this] { resetEditedSkinToDefault (); };
  _skinEditor->onDelete = [this] { deleteEditedSkin (); };

  // The keyboard is Onboard, the system's own — see io/OnScreenKeyboard.hh.
  // It types into whatever window has the focus, which is this one.
  _skinEditor->onNamingChanged = [this] (bool naming) { showKeyboard (naming); };
  _skinEditor->onColourPicked
      = [this] (auto const &path) { openColourPicker (path); };

  _colourPicker = std::make_unique<ColourPickerComponent> ();
  _colourPicker->setAlwaysOnTop (true);
  _motionComponent->addChildComponent (*_colourPicker);
  _colourPicker->onColourChanged = [this] { applyPickedColour (); };
  _colourPicker->onDone = [this] { closeColourPicker (); };

  // Clip Settings: permanent bottom panel, always visible.
  _clipSettings = std::make_unique<ClipSettingsComponent> ();
  _clipSettings->setAlwaysOnTop (true);

  // A finger reaches the same two handlers the encoders do. Tapping names
  // section and sub-element at once, which is what the Motion-Encoder's
  // scrolling and the Pot-Encoder's press reach one step at a time.
  _clipSettings->onControlTapped = [this] (int section, int sub) {
    // Section first: it resets the sub-index, so naming the sub-element
    // afterwards is what makes the tap land where it was aimed.
    selectClipSettingsSection (section);
    selectClipSettingsSubElement (sub);
  };
  _clipSettings->onControlDragged = [this] (int, int, int increment) {
    handleClipSettingsValueChange (_clipSettingsChannel, increment);
  };

  addChildComponent (*_clipSettings);
  _clipSettings->setVisible (true);
  selectClip (0, 0); // sensible default before any button has been pressed

  // Clockmode is all that is left to restore. Pot Size and the two font sizes
  // are the skin's now, and the skin brings its own.
  auto const persisted = loadSettings (getPersistedSettingsFile ());
  applyClockMode (persisted.clockMode);
  _recMode = persisted.recMode;
  _engine.setRecMode (_recMode);


  // Fast enough for the write head to move while a take runs; the directory
  // check inside keeps its old two-second pace by counting ticks.
  startTimer (50);

  _engine.addPatternStatusListener (this);
  _tickCallbackHandle = _engine.getTempoClock ().scheduleEventHandlerAddition (
      [this] (auto measure) { tickCallback (measure); },
      TempoClock::Event::Tick, TempoClock::Execution::JuceMessageThread);

  auto constexpr testTempoEstimation = false;
  if (testTempoEstimation)
    {
      _tempoEstimatorTest = std::make_unique<TempoEstimatorTest> ();
      _ioAdapter->getTapTimeMicros ().addListener (_tempoEstimatorTest.get ());
    }

  // Setup OSC Receiver from config
  int oscRecvPort = 7771; // default
  juce::String oscRecvHost = "0.0.0.0";
  if (userConfig.hasProperty ("ui"))
    {
      auto const uiConfig = userConfig["ui"];
      if (uiConfig.hasProperty ("pauseRenderingInMenu"))
        _pauseRenderingInMenu
            = static_cast<bool> (uiConfig["pauseRenderingInMenu"]);
    }

  if (userConfig.hasProperty ("oscReceiver"))
    {
      auto oscRecvConfig = userConfig["oscReceiver"];
      if (oscRecvConfig.hasProperty ("port"))
        oscRecvPort = static_cast<int> (oscRecvConfig["port"]);
      if (oscRecvConfig.hasProperty ("host"))
        oscRecvHost = oscRecvConfig["host"].toString ();
    }
  if (_oscReceiver.connect (oscRecvPort))
    {
      std::cout << "OSC Receiver listening on " << oscRecvHost << ":" << oscRecvPort << std::endl;
      _oscReceiver.addListener (this);
    }
  else
    {
      std::cerr << "ERROR: Could not bind OSC Receiver to port " << oscRecvPort << std::endl;
    }

  // Setup OSC Receiver for VU meters (separate port)
  int oscVuPort = 7772; // default
  if (userConfig.hasProperty ("oscReceiver"))
    {
      auto oscRecvConfig = userConfig["oscReceiver"];
      if (oscRecvConfig.hasProperty ("vuPort"))
        oscVuPort = static_cast<int> (oscRecvConfig["vuPort"]);
    }
  if (_oscReceiverVU.connect (oscVuPort))
    {
      std::cout << "OSC VU Receiver listening on " << oscRecvHost << ":" << oscVuPort << std::endl;
      _oscReceiverVU.addListener (this);
    }
  else
    {
      std::cerr << "ERROR: Could not bind OSC VU Receiver to port " << oscVuPort << std::endl;
    }

  // Setup OSC Receiver for the IEM EnergyVisualizer (separate port again —
  // it sends 426 floats at 9 Hz and has no business sharing a socket with the
  // beat clock).
  int oscEnergyPort = 7777; // default
  if (userConfig.hasProperty ("oscReceiver"))
    {
      auto oscRecvConfig = userConfig["oscReceiver"];
      if (oscRecvConfig.hasProperty ("energyPort"))
        oscEnergyPort = static_cast<int> (oscRecvConfig["energyPort"]);
    }
  if (_oscReceiverEnergy.connect (oscEnergyPort))
    {
      std::cout << "OSC Energy Receiver listening on " << oscRecvHost << ":" << oscEnergyPort << std::endl;
      _oscReceiverEnergy.addListener (this);
    }
  else
    {
      std::cerr << "ERROR: Could not bind OSC Energy Receiver to port " << oscEnergyPort << std::endl;
    }

  // Setup OSC Sender from config (for beatclock)
  if (userConfig.hasProperty ("oscSender"))
    {
      auto oscSendConfig = userConfig["oscSender"];
      juce::String oscSendHost = oscSendConfig["host"].toString ();
      // Use beatclockPort if specified, otherwise fall back to port
      int oscSendPort = static_cast<int> (oscSendConfig["port"]);
      if (oscSendConfig.hasProperty ("beatclockPort"))
        oscSendPort = static_cast<int> (oscSendConfig["beatclockPort"]);
      if (_oscSender.connect (oscSendHost, oscSendPort))
        std::cout << "OSC Sender for beatclock connected to " << oscSendHost << ":" << oscSendPort << std::endl;
      else
        std::cerr << "ERROR: OSC Sender failed to connect to " << oscSendHost << ":" << oscSendPort << std::endl;
      
      // Direct tap sender (same host/port, bypasses async queue for zero latency)
      if (_tapSender.connect (oscSendHost, oscSendPort))
        std::cout << "OSC Tap Sender connected to " << oscSendHost << ":" << oscSendPort << std::endl;
      else
        std::cerr << "ERROR: OSC Tap Sender failed to connect" << std::endl;
    }
}

A3MotionUIComponent::~A3MotionUIComponent ()
{
  stopTimer ();
  _oscReceiverEnergy.removeListener (this);
  _oscReceiverEnergy.disconnect ();
  _oscReceiverVU.removeListener (this);
  _oscReceiverVU.disconnect ();
  _oscReceiver.removeListener (this);
  _oscReceiver.disconnect ();
  _oscSender.disconnect ();
  _tapSender.disconnect ();

  if (runsOnHardware ())
    {
      _ioAdapter->stopThread (-1);
    }

  _engine.removePatternStatusListener (this);
  setLookAndFeel (nullptr);
}

void
A3MotionUIComponent::createChannelsUI ()
{
  auto const numChannels = _engine.getNumChannels ();

  _channelUIStates.reserve (numChannels);
  _channelStrips.reserve (numChannels);

  for (auto channel = 0u; channel < numChannels; ++channel)
    {
      auto uiState = std::make_unique<ChannelUIState> ();

      // Straight from the theme, which has already parsed the skin file. This
      // used to read `channels` out of the skin a second time, by hand, with a
      // grey fallback of its own — two parsers for one array, and only one of
      // them knew what a channel's colour is when the skin omits it. The
      // second parse went; the skin it was loading for went with it.
      if (static_cast<int> (channel) < numThemeChannels)
        uiState->colour = toColour (theme ().channel[channel]);
      else
        {
          // Fallback: generate colours from HSV
          auto const hueNorm
              = static_cast<float> (channel) / numChannels;
          auto hue = hueNorm / 360.f * 256.f;
          uiState->colour = juce::Colour::fromHSV (hue, 0.6f, 0.8f, 1.f);
        }

      auto strip = std::make_unique<ChannelStrip> (*uiState);
      addChildComponent (*strip);
      strip->setVisible (true);

      _channelStrips.push_back (std::move (strip));
      _channelUIStates.push_back (std::move (uiState));
    }

  _previewHeldPad = std::vector<int> (numChannels, -1);
}

float
A3MotionUIComponent::getPatternLengthBeats (index_t channel, index_t slot) const
{
  // What the pattern itself is, which every pattern file already states as
  // data-beats — the shipped shapes carry 4, 8, 16 and 32. Derived from the
  // tick count the way PatternFile derives it when saving; both rely on
  // MotionEngine::recordingSamplesPerTick being 1, and would need to divide it
  // out together if that ever changes.
  auto const &pattern = _patterns[channel][slot];
  if (pattern && pattern->getNumTicks () > 0)
    return static_cast<float> (pattern->getNumTicks ())
           / static_cast<float> (TempoClock::getTicksPerBeat ());

  return defaultPatternLengthBeats;
}

float
A3MotionUIComponent::getLengthBeats (index_t channel, index_t slot) const
{
  // The pattern's own length, taken at this clip's rate. Speed used to *be*
  // the length and the pattern's own was ignored, so turning the knob
  // redefined how long a take had been after the fact.
  return playbackLengthBeats (getPatternLengthBeats (channel, slot),
                              _clipUIParams[channel][slot].speedLog2);
}

void
A3MotionUIComponent::applyMotionMode (index_t channel, index_t slot)
{
  auto &pattern = _patterns[channel][slot];
  if (!pattern)
    return;

  auto const &params = _clipUIParams[channel][slot];
  pattern->setPlayDirection (params.direction == 1 ? PlayDirection::Reverse
                                                   : PlayDirection::Forward);

  // The bar's order is the engine's order; a value out of range would be a
  // list that grew in one place and not the other.
  auto const actions = { EndAction::Loop, EndAction::Stop, EndAction::Bounce,
                         EndAction::Random };
  if (params.endAction >= 0
      && params.endAction < static_cast<int> (actions.size ()))
    pattern->setEndAction (*(actions.begin () + params.endAction));
}

Measure
A3MotionUIComponent::getPlaybackLength (index_t channel, index_t slot) const
{
  auto const ticks = playbackLengthTicks (
      getLengthBeats (channel, slot),
      static_cast<index_t> (TempoClock::getTicksPerBeat ()));

  return Measure{ 0, 0, static_cast<int> (ticks) }.consolidate (
      _engine.getBeatsPerBar ());
}

void
A3MotionUIComponent::createMainUI ()
{
  _statusBar = std::make_unique<StatusBar> (_valueBPM);
  _statusBar->onKeyboardIconTapped = [this] { toggleKeyboard (); };
  addChildComponent (*_statusBar);
  _statusBar->setVisible (true);
  _statusBarCallbackHandle
      = _engine.getTempoClock ().scheduleEventHandlerAddition (
          [this] (auto measure) { _statusBar->beatCallback (measure); },
          TempoClock::Event::Beat, TempoClock::Execution::JuceMessageThread);

  _motionComponent
      = std::make_unique<MotionComponent> (_engine, _channelUIStates);
  addChildComponent (*_motionComponent);
  _motionComponent->setVisible (true);

  // Hidden: no longer part of the visible layout (see resized()), but keeps
  // receiving its normal update calls underneath.
  _filterDisplay = std::make_unique<FilterDisplay> ();
  addChildComponent (*_filterDisplay);
  _filterDisplay->setVisible (false);
  for (auto ch = 0u; ch < _channelUIStates.size () && ch < FilterDisplay::numChannels; ++ch)
    _filterDisplay->setChannelColour (static_cast<int> (ch), _channelUIStates[ch]->colour);

  // Hidden: see comment on _filterDisplay above.
  _loopLengthDisplay = std::make_unique<LoopLengthDisplay> ();
  addChildComponent (*_loopLengthDisplay);
  _loopLengthDisplay->setVisible (false);
  _loopLengthDisplay->setReferenceBeats (
      _engine.getBeatsPerBar ());
  for (auto ch = 0u; ch < _channelUIStates.size () && ch < LoopLengthDisplay::numChannels; ++ch)
    {
      _loopLengthDisplay->setChannelColour (static_cast<int> (ch), _channelUIStates[ch]->colour);
      _loopLengthDisplay->setLoopLengthBeats (static_cast<int> (ch),
                                              getLengthBeats (ch, 0));
    }

  // Hidden: see comment on _filterDisplay above.
  _elevationDisplay = std::make_unique<ElevationDisplay> ();
  addChildComponent (*_elevationDisplay);
  _elevationDisplay->setVisible (false);
  for (auto ch = 0u; ch < _channelUIStates.size () && ch < ElevationDisplay::numChannels; ++ch)
    {
      _elevationDisplay->setChannelColour (static_cast<int> (ch), _channelUIStates[ch]->colour);
      // Coverage is per-clip now (see ClipSettingsComponent), not
      // per-channel — this hidden legacy display just keeps its own
      // built-in default.
    }
}

constexpr bool
A3MotionUIComponent::runsOnHardware ()
{
#if HARDWARE_INTERFACE_ENABLED
  return true;
#else
  return false;
#endif
}

void
A3MotionUIComponent::createHardwareInterface ()
{
#if HARDWARE_INTERFACE_ENABLED
#ifdef HARDWARE_INTERFACE_V2
  _ioAdapter = std::make_unique<InputOutputAdapterV2> ();
#elif defined(HARDWARE_INTERFACE_V3)
  _ioAdapter = std::make_unique<InputOutputAdapterV3> ();
#else
#error hardware interface enabled but no implementation selected!
#endif
  _ioAdapter->getButton (Button::ClockMode).addListener (this);
  _ioAdapter->getButton (Button::Menu).addListener (this);
  _ioAdapter->getButton (Button::Record).addListener (this);
  _ioAdapter->getButton (Button::Tap).addListener (this);
  _ioAdapter->getButton (Button::Shift).addListener (this);
  _ioAdapter->getTapTimeMicros ().addListener (this);
  for (auto channel = 0u; channel < _ioAdapter->getNumChannels (); ++channel)
    {
      for (auto pad = 0u; pad < _ioAdapter->getNumPadsPerChannel (); ++pad)
        {
          _ioAdapter->getPad (channel, pad).addListener (this);
        }

      _ioAdapter->getPot (channel, 0).addListener (this);
      _ioAdapter->getPot (channel, 1).addListener (this);
      _ioAdapter->getEncoderIncrement (channel).addListener (this);
      _ioAdapter->getEncoderPress (channel).addListener (this);
      _ioAdapter->getEncoderIncrement (channel, 1).addListener (this);
      _ioAdapter->getEncoderPress (channel, 1).addListener (this);
    }

  _ioAdapter->startThread ();
  blankLEDs ();
#endif
}

void
A3MotionUIComponent::initializePatterns ()
{
  auto const numChannels = runsOnHardware ()
                               ? _ioAdapter->getNumChannels ()
                               : _engine.getNumChannels ();
  _patterns.resize (numChannels);
  _clipUIParams.resize (numChannels);

  for (auto &channelPatterns : _patterns)
    channelPatterns.resize (numClipSlots);
  for (auto &channelParams : _clipUIParams)
    channelParams.resize (numClipSlots);

  // Load patterns from the library, one per clip slot; channels share the
  // same library slot but each gets its own Pattern instance. Default
  // shape is "Square" (name lookup rather than a raw library index, so it
  // doesn't depend on alphabetical file ordering).
  auto const numLibEntries = _patternLibrary->getNumEntries (); // includes Empty at 0
  auto const defaultLibIndex = _patternLibrary->indexForName ("Square");
  for (auto channel = 0u; channel < numChannels; ++channel)
    {
      for (auto slot = 0u; slot < numClipSlots; ++slot)
        {
          auto const libIndex = defaultLibIndex + static_cast<int> (slot);
          if (libIndex > 0 && libIndex < numLibEntries)
            {
              auto p = _patternLibrary->loadPattern (libIndex);
              if (p)
                {
                  p->setChannel (channel);
                  _patterns[channel][slot] = std::move (p);
                }
            }
        }
    }
}

void
A3MotionUIComponent::blankLEDs ()
{
  _ioAdapter->getButtonLED (Button::ClockMode) = false;
  _ioAdapter->getButtonLED (Button::Record) = false;
  _ioAdapter->getButtonLED (Button::Tap) = false;
  _ioAdapter->getButtonLED (Button::Shift) = false;

  for (auto channel = 0u; channel < _ioAdapter->getNumChannels (); ++channel)
    {
      for (auto pad = 0u; pad < _ioAdapter->getNumPadsPerChannel (); ++pad)
        {
          // An LED that is off is not coloured black, it is unlit —
          // transparentBlack is how this codebase says "no colour", and the
          // hardware path reads the rgb, which is zero either way.
          _ioAdapter->getPadLED (channel, pad)
              = juce::VariantConverter<juce::Colour>::toVar (
                  juce::Colours::transparentBlack);
        }
    }
}

void
A3MotionUIComponent::paint (juce::Graphics &g)
{
  juce::ignoreUnused (g);
}

void
A3MotionUIComponent::resized ()
{
  juce::Component::resized ();

  auto bounds = getLocalBounds ();

  // Status bar at the top
  // The bar is as tall as the header size needs; the sphere gets the rest.
  auto const statusBarHeight = _statusBar->preferredHeight ();
  auto boundsStatus = bounds.removeFromTop (statusBarHeight);
  _statusBar->setBounds (boundsStatus);

  // LoopLength/Elevation/PadRow/Filter option bars are hidden (see
  // createMainUI()/createPadRowDisplays()) — they no longer get screen
  // space, but keep receiving their normal update calls under the hood.

  // Hide channel strips - no longer needed after removing width/order displays
  for (auto &strip : _channelStrips)
    strip->setVisible (false);

  // Clip Settings panel: permanent bottom quarter of the screen. Carved out
  // of MotionComponent's actual bounds (not just overlaid) because
  // MotionComponent renders via its own directly-attached OpenGLContext
  // (see MotionComponent.cc), which always composites above normal JUCE
  // components regardless of z-order/toFront()/rendering-paused state, so
  // nothing can visibly overlap it.
  // The bar asks for what its content needs at the current font and pot
  // sizes, and gets it up to a share of the screen. A fixed quarter was what
  // made Font Size inert down there: the height fixed the control boxes, and
  // the boxes capped the text.
  auto const clipSettingsHeight
      = _clipSettings != nullptr
            ? clipSettingsHeightWithin (
                  _clipSettings->preferredHeight (bounds.getWidth ()),
                  bounds.getHeight ())
            : bounds.getHeight () / 4;
  auto boundsClipSettings = bounds.removeFromBottom (clipSettingsHeight);
  if (_clipSettings)
    _clipSettings->setBounds (boundsClipSettings);

  // The menu covers the sphere and nothing else. It used to take the clip
  // settings' space as well — the bar gave up its bounds and the menu had the
  // whole screen — which meant the one thing several of its settings change
  // was invisible while they were being changed. Font and pot sizes are the
  // bar's own look; you have to see it to set it.
  //
  // The bar can stay because the menu is a child of MotionComponent and the
  // GL context composites above everything else: whatever is not that child
  // would hide behind the sphere's image, so the menu must not reach past it.

  _motionComponent->setBounds (bounds);

  if (_globalSettings)
    _globalSettings->setBounds (_motionComponent->getLocalBounds ());
  if (_skinEditor)
    _skinEditor->setBounds (_motionComponent->getLocalBounds ());
  if (_colourPicker)
    {
      // Only the lower part of the screen: the sphere above it is what the
      // colour is being chosen for, and it has to stay in sight.
      auto picker = _motionComponent->getLocalBounds ();
      _colourPicker->setBounds (
          picker.removeFromBottom (picker.getHeight () * 2 / 5)
              .reduced (picker.getWidth () / 20, 0));
    }
}

float
A3MotionUIComponent::getMinimumWidth () const
{
  return _channelStrips.size () * LayoutHints::Channels::widthMin;
}

float
A3MotionUIComponent::getMinimumHeight () const
{
  auto minimumHeight = LayoutHints::MotionComponent::heightMin;
  return minimumHeight;
}

// TODO: factor this into separate listeners so not all sources have to
// be tested exhaustively.
void
A3MotionUIComponent::valueChanged (juce::Value &value)
{
  if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::ClockMode)))
    {
      // ClockMode button is kept for backward compat but no longer cycles;
      // clock mode is changed via Global Settings.
      if (value.getValue ())
        updateControlReadout ("-- CLOCKMODE");
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::Menu)))
    {
      bool const pressed = static_cast<bool> (value.getValue ());
      if (pressed)
        {
          updateControlReadout ("-- MENU");
          // One level at a time: a name being typed, then the editor, then
          // the menu itself.
          if (_colourPickerOpen)
            closeColourPicker ();
          else if (_skinEditorOpen && _skinEditor->isNaming ())
            _skinEditor->finishNaming ();
          else if (_skinEditorOpen)
            closeSkinEditor ();
          else if (_globalSettingsOpen)
            closeGlobalSettings ();
          else
            openGlobalSettings ();
        }
      // release: ignored (toggle on press)
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::Record)))
    {
      _ioAdapter->getButtonLED (Button::Record) = value.getValue ();
      updateControlReadout (juce::String ("-- RECORD ")
                            + (value.getValue () ? "ON" : "OFF"));

      // Pressed while a take is running, this ends it. The finger no longer
      // bounds a recording — that is what makes a jump recordable — so
      // something else has to, and Record is the button that started it.
      if (static_cast<bool> (value.getValue ()) && _engine.isRecording ())
        endRecording ();

      // Recording itself is armed when a slot's Play|Pause pad is pressed
      // while this button is held — see handlePadPress().
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::Shift)))
    {
      // Pure modifier: level-checked via isButtonPressed(Button::Shift) when
      // a slot's Action pad is pressed (preview-and-fire gesture).
      _ioAdapter->getButtonLED (Button::Shift) = value.getValue ();
      updateControlReadout (juce::String ("-- SHIFT ")
                            + (value.getValue () ? "ON" : "OFF"));
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::Tap)))
    {
      if (value.getValue ())
        {
          // Button pressed - send /tap OSC immediately via DIRECT sender
          // Bypasses async queue for zero latency - time-critical!
          _ioAdapter->getButtonLED (Button::Tap) = true;
          updateControlReadout ("-- TAP");

          auto tapMsg = juce::OSCMessage ("/tap");
          tapMsg.addInt32 (1);
          _tapSender.send (tapMsg);  // Direct, synchronous send - no queue!
        }
      else
        {
          _ioAdapter->getButtonLED (Button::Tap) = false;
        }
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getTapTimeMicros ()))
    {
      if (_clockMode == 0)
        {
          auto const tapTime = juce::int64 (value.getValue ());
          auto const result = _engine.tap (tapTime);

          // FirstTap was falling through here entirely. The clock does reset
          // itself on it — that much is covered by
          // TempoClock.FirstTapResetsTheBeat — but the UI gave no sign of it,
          // and a stale BPM from the previous run stayed on the readout.
          switch (result)
            {
            case TempoClock::TapResult::TempoAvailable:
              {
                auto const bpm = _engine.getTempoBPM ();
                juce::Logger::writeToLog ("[TAP] BPM=" + juce::String (bpm));
                _valueBPM = bpm;
                break;
              }
            case TempoClock::TapResult::FirstTap:
              juce::Logger::writeToLog ("[TAP] first tap: beat reset to 1");
              updateControlReadout ("-- TAP 1");
              break;
            case TempoClock::TapResult::TempoNotAvailable:
              juce::Logger::writeToLog ("[TAP] counting, no tempo yet");
              break;
            }
        }
    }
  else
    {
      for (auto channel = 0u; channel < _ioAdapter->getNumChannels ();
           ++channel)
        {
          if (value.refersToSameSourceAs (
                  _ioAdapter->getEncoderIncrement (channel)))
            {
              // Motion-Encoder (upper): scrolls the Clip Settings panel's
              // menu items, except on channel 3 while Global Settings is
              // open (that encoder is reserved for menu navigation there).
              auto const increment = static_cast<int> (
                  _ioAdapter->getEncoderIncrement (channel).getValue ());
              if (increment != 0)
                updateControlReadout (
                    "CH" + juce::String (channel + 1) + " ENC "
                    + (increment > 0 ? "+" : "") + juce::String (increment));
              if (_colourPickerOpen && channel == 3u)
                {
                  if (increment != 0)
                    _colourPicker->navigate (increment > 0 ? 1 : -1);
                  return;
                }

              if (_skinEditorOpen && channel == 3u)
                {
                  if (increment != 0)
                    {
                      _skinEditor->navigate (increment > 0 ? 1 : -1);
                      refreshKeyboardIcon ();
                    }
                  return;
                }

              if (_globalSettingsOpen && channel == 3u)
                {
                  if (increment != 0)
                    {
                      if (_globalSettingsValueFieldSelected)
                        {
                          _globalSettings->navigateValue (increment > 0 ? 1
                                                                        : -1);

                          if (browsedMenuRow ()
                              == std::optional<MenuRow>{ MenuRow::Skin })
                            previewSkin (
                                _globalSettings->getSelectedValueIndex ());
                        }
                      else
                        {
                          _globalSettings->navigateOption (increment > 0 ? 1 : -1);
                          _globalSettingsOptionIndex = _globalSettings->getOptionIndex ();
                        }
                    }
                  return;
                }

              handleClipSettingsScroll (channel, increment);
            }
          else if (value.refersToSameSourceAs (
                       _ioAdapter->getEncoderIncrement (channel, 1)))
            {
              // Pot-Encoder (lower): changes the value of the currently
              // selected Clip Settings menu item. Suppressed on channel 3
              // while Global Settings is open, matching the Motion-Encoder.
              auto const increment = static_cast<int> (
                  _ioAdapter->getEncoderIncrement (channel, 1).getValue ());
              if (increment != 0)
                updateControlReadout (
                    "CH" + juce::String (channel + 1) + " POT-ENC "
                    + (increment > 0 ? "+" : "") + juce::String (increment));
              if (_globalSettingsOpen && channel == 3u)
                return;

              handleClipSettingsValueChange (channel, increment);
            }
          else if (value.refersToSameSourceAs (
                       _ioAdapter->getEncoderPress (channel)))
            {
              if (value.getValue ())
                {
                  updateControlReadout ("CH" + juce::String (channel + 1)
                                       + " ENC PRESS");
                  // In the skin editor the same press arms the browsed
                  // parameter and lets it go again.
                  if (_colourPickerOpen && channel == 3u)
                    {
                      _colourPicker->toggleEditing ();
                      return;
                    }

                  if (_skinEditorOpen && channel == 3u)
                    {
                      _skinEditor->toggleEditing ();
                      return;
                    }

                  // The single ch3 encoder also drives Global Settings
                  // while it's open: first press arms the currently
                  // browsed option's value field, second press confirms
                  // and applies it (menu stays open).
                  if (_globalSettingsOpen && channel == 3u)
                    {
                      if (_globalSettings->opensSubmenu (
                              _globalSettingsOptionIndex))
                        {
                          // Nothing to choose, so nothing to arm: the press
                          // that reaches the row is the press that opens it.
                          _globalSettingsValueFieldSelected = true;
                          _globalSettings->setValueFieldSelected (true);
                          confirmGlobalSettingsOption ();
                        }
                      else if (!_globalSettingsValueFieldSelected)
                        {
                          _globalSettingsValueFieldSelected = true;
                          _globalSettings->setValueFieldSelected (true);
                        }
                      else
                        {
                          confirmGlobalSettingsOption ();
                        }
                    }
                }
            }
          else if (value.refersToSameSourceAs (
                       _ioAdapter->getEncoderPress (channel, 1)))
            {
              // Pot-Encoder press: cycle the current section's sub-element
              // (e.g. Elevation: coverage <-> mode). Suppressed on channel 3
              // while Global Settings is open, matching the Motion-Encoder.
              if (value.getValue () && !(_globalSettingsOpen && channel == 3u))
                handleClipSettingsSubElementCycle (channel);
            }
          else if (value.refersToSameSourceAs (
                       _ioAdapter->getPot (channel, 0)))
            {
              jassert (value.getValue ().isDouble ());
              auto const pot1Normalized
                  = static_cast<float> (value.getValue ());
#ifdef DEBUG
              juce::Logger::writeToLog ("channel " + juce::String (channel)
                                        + " pot_1: " + juce::String (pot1Normalized));
#endif
              if (channel == 3u && _globalSettingsOpen)
                {
                  // Menu navigation is driven exclusively by the ch3
                  // rotary encoder; suppress the pot-encoder's synthetic
                  // pot values while the overlay is open.
                }
              else
                {
                  _engine.setChannelPot1 (channel, pot1Normalized);
                  _filterDisplay->setSweep (static_cast<int> (channel), pot1Normalized);
                  updateControlReadout ("CH" + juce::String (channel + 1)
                                       + " POT1 "
                                       + juce::String (pot1Normalized, 2));
                }
              return;
            }
          else if (value.refersToSameSourceAs (
                       _ioAdapter->getPot (channel, 1)))
            {
              jassert (value.getValue ().isDouble ());
              auto const pot2Normalized = static_cast<float> (value.getValue ());
#ifdef DEBUG
              juce::Logger::writeToLog ("channel " + juce::String (channel)
                                        + " pot_2: " + juce::String (pot2Normalized));
#endif
              if (channel == 3u && _globalSettingsOpen)
                {
                  // Menu navigation is driven exclusively by the ch3
                  // rotary encoder; suppress the pot-encoder's synthetic
                  // pot values while the overlay is open.
                }
              else
                {
                  _engine.setChannelPot2 (channel, pot2Normalized);
                  _filterDisplay->setQ (static_cast<int> (channel), pot2Normalized);
                  updateControlReadout ("CH" + juce::String (channel + 1)
                                       + " POT2 "
                                       + juce::String (pot2Normalized, 2));
                }
              return;
            }

          for (auto pad = 0u; pad < _ioAdapter->getNumPadsPerChannel (); ++pad)
            {
              if (value.refersToSameSourceAs (
                      _ioAdapter->getPad (channel, pad)))
                {
                  auto const slot = slotForPadIndex[pad];
                  if (value.getValue ())
                    {
                      handlePadPress (channel, pad);
                    }
                  else if (padFunctionByPadIndex[pad] == PadFunction::Action
                           && _previewHeldPad[channel]
                                  == static_cast<int> (slot))
                    {
                      // Action released → exit the Shift+Action preview
                      // gesture. OSC fires from current position, pattern
                      // keeps playing.
                      _engine.setPreviewMode (channel, false);
                      _previewHeldPad[channel] = -1;

                      if (_patterns[channel][slot])
                        {
                          _motionComponent->unsetPreviewPattern (
                              _patterns[channel][slot]);
                        }
                    }
                  return;
                }
            }
        }
    }
}

void
A3MotionUIComponent::handlePadPress (index_t channel, index_t pad)
{
  auto const function = padFunctionByPadIndex[pad];
  auto const slot = slotForPadIndex[pad];
  auto &pattern = _patterns[channel][slot];

  char const *functionName = "";
  switch (function)
    {
    case PadFunction::PlayPause: functionName = "PLAYPAUSE"; break;
    case PadFunction::Stop:      functionName = "STOP";      break;
    case PadFunction::Action:    functionName = "ACTION";    break;
    case PadFunction::Settings:  functionName = "SETTINGS";  break;
    }
  updateControlReadout ("CH" + juce::String (channel + 1) + " "
                        + functionName);

  if (isButtonPressed (Button::Record) && function == PadFunction::PlayPause)
    {
      // Stop any existing pattern at this slot
      if (pattern)
        {
          auto status = pattern->getStatus ();
          if (status == Pattern::Status::Playing
              || status == Pattern::Status::Recording)
            {
              _engine.stopPattern (pattern, TempoClock::nextDownBeat (_now));
            }
          _motionComponent->unsetPreviewPattern (pattern);
        }

      // The length set for the next take, not whatever the slot happens to
      // hold. Recording is the only moment a pattern's length is decided.
      auto const configuredLengthBeats
          = std::exp2 (static_cast<float> (
                _clipUIParams[channel][slot].recordLengthLog2))
            * _engine.getBeatsPerBar ();

      // A take runs until Record is pressed again. OneShot — the default —
      // schedules its own stop one length in, which is why recording ended by
      // itself with nobody touching the button.
      _engine.setRecordingMode (MotionEngine::RecordingMode::Loop);

      // Remembered so an empty take can be undone: the slot's pattern is
      // replaced right below, and a stray double press must not cost whatever
      // was in there.
      _recordingSlot = std::make_pair (channel, slot);
      _patternBeforeRecording = pattern;

      // Drawn faintly under the take so you can see what you are writing over.
      // MotionComponent decides whether to show it -- Write replaces the whole
      // pass, and a ghost of the old one there says nothing.
      if (_motionComponent)
        _motionComponent->setRecordingUnderlay (_patternBeforeRecording);

      // Always create a fresh Pattern for recording (user pattern)
      pattern = std::make_shared<Pattern> ();
      pattern->setChannel (channel);

      auto recordLength = Measure{
        0, static_cast<int> (std::max (1.f, configuredLengthBeats)), 0
      };
      recordLength.consolidate (_engine.getBeatsPerBar ());

      // Store the recording length in the pattern so it can be updated if encoder changes
      pattern->setPlaybackLength (recordLength);

      _engine.recordPattern (pattern, TempoClock::nextDownBeat (_now),
                             recordLength);

      // Show what is being recorded. Starting a recording on one channel while
      // the bar still displayed another one left every setting that shapes the
      // take — speed above all, which is its length — pointing at the wrong
      // clip, and the encoders with it.
      selectClip (channel, slot);
      return;
    }

  switch (function)
    {
    case PadFunction::PlayPause:
      {
        if (!pattern)
          break;
        auto const status = pattern->getStatus ();
        if (status == Pattern::Status::Idle)
          {
            pattern->setPlaybackLength (getPlaybackLength (channel, slot));
            _engine.playPattern (pattern, _now);
          }
        else if (status == Pattern::Status::Playing
                 || status == Pattern::Status::ScheduledForPlaying)
          {
            _engine.stopPattern (pattern, TempoClock::nextDownBeat (_now));
          }
        break;
      }
    case PadFunction::Stop:
      {
        if (!pattern)
          break;
        auto const status = pattern->getStatus ();
        if (status == Pattern::Status::Playing
            || status == Pattern::Status::Recording
            || status == Pattern::Status::ScheduledForPlaying)
          {
            _engine.stopPattern (pattern, TempoClock::nextDownBeat (_now));
          }
        break;
      }
    case PadFunction::Action:
      {
        // Shift+Action: preview-and-fire — play in preview mode (OSC
        // silenced) while the encoder can browse the library; releasing
        // Action exits (see valueChanged()'s pad-release branch).
        // Without Shift: reserved for the Clip Settings Browser preset
        // trigger, not implemented yet.
        if (isButtonPressed (Button::Shift) && pattern
            && pattern->getStatus () == Pattern::Status::Idle)
          {
            pattern->setPlaybackLength (getPlaybackLength (channel, slot));
            _engine.setPreviewMode (channel, true);
            _previewHeldPad[channel] = static_cast<int> (slot);
            _engine.playPattern (pattern, _now);
            setPreviewWithDisplayData (pattern);
          }
        break;
      }
    case PadFunction::Settings:
      {
        selectClip (channel, slot);
        break;
      }
    }
}

void
A3MotionUIComponent::handleMessage (juce::Message const &message)
{
  using Status = MotionEngine::PatternStatusMessage::Status;
  auto const &messagePatternStatus
      = static_cast<MotionEngine::PatternStatusMessage const &> (message);

  auto const channel = messagePatternStatus.pattern->getChannel ();
  switch (messagePatternStatus.status)
    {
    case Status::Playing:
      {
        break;
      }
    case Status::Recording:
      {
        setPreviewWithDisplayData (messagePatternStatus.pattern);
        _channelStrips[channel]->setTextColour (toColour (theme ().danger));
        break;
      }
    case Status::Stopped:
      {
        _motionComponent->unsetPreviewPattern (messagePatternStatus.pattern);
        _channelStrips[channel]->setTextColour (
            toColour (theme ().textPrimary));

        // If this was a recording that just finished, save as user pattern.
        // We use wasRecording() because the status chain is:
        //   Recording → ScheduledForIdle → Idle
        // so getLastStatus() returns ScheduledForIdle, not Recording.
        // The take has definitely stopped now, so it is safe to start it: the
        // motion was looping anyway, and ending a take stops the writing
        // rather than the movement.
        if (_playWhenRecordingStops == messagePatternStatus.pattern)
          {
            _playWhenRecordingStops.reset ();
            _engine.playPattern (messagePatternStatus.pattern, _now);
          }

        if (messagePatternStatus.pattern->wasRecording ())
          {
            // Find which clip slot this pattern belongs to
            bool found = false;
            for (size_t slot = 0; slot < _patterns[channel].size (); ++slot)
              {
                if (_patterns[channel][slot] == messagePatternStatus.pattern)
                  {
                    std::cout << "  -> found in clip slot " << slot << std::endl;
                    saveRecordedPattern (messagePatternStatus.pattern,
                                         channel,
                                         static_cast<index_t> (slot));
                    found = true;
                    break;
                  }
              }
            if (!found)
              std::cout << "  -> pattern NOT found in any clip slot!" << std::endl;
          }
        break;
      }
    }
}

bool
A3MotionUIComponent::isButtonPressed (Button button)
{
  return _ioAdapter->getButton (button).getValue ();
}

void
A3MotionUIComponent::tickCallback (Measure measure)
{
  _now = measure;

  // Send beat via OSC on every beat (only in INT mode to avoid feedback with external clock)
  // AsyncOSCSender enqueues to lock-free FIFO, safe to call from any thread
  if (_clockMode == 0 && measure.tick () == 0)
    {
      auto beatClockMsg = juce::OSCMessage ("/beat");
      beatClockMsg.addInt32 (measure.beat () + 1);  // 1-indexed beat
      beatClockMsg.addInt32 (measure.bar () + 1);   // 1-indexed bar
      beatClockMsg.addInt32 (static_cast<int> (std::round (_engine.getTempoBPM ())));
      _oscSender.send (beatClockMsg);
    }

  if (runsOnHardware ())
    {
      if (measure.beat () == 0 && measure.tick () == 0)
        {
          _stepsLED = 0;
        }

      using T =
          typename std::remove_reference<decltype (measure.tick ())>::type;
      jassert (ticksPerStepPadLEDs <= std::numeric_limits<T>::max ());
      auto const divisor = static_cast<T> (ticksPerStepPadLEDs);
      if (measure.tick () % divisor == 0)
        {
          padLEDCallback (_stepsLED++);
        }

      if (!_ioAdapter->getButton (Button::Record).getValue ())
        {
          _ioAdapter->getButtonLED (Button::Record) = _engine.isRecording ();
        }
    }

  // Throttle repaint to ~30 Hz (every 4th tick at typical tick rate)
  // The channel strips are hidden but progress values still update
  bool shouldRepaint = (measure.tick () % 4 == 0);

  auto recordingPattern = _engine.getRecordingPattern ();
  if (recordingPattern)
    {
      auto const channel = recordingPattern->getChannel ();
      _motionComponent->setBackgroundColour (
          _channelUIStates[channel]->colour.withAlpha (0.2f));

      auto const progress
          = recordingPattern->getLastUpdatedTick ()
            / static_cast<float> (recordingPattern->getNumTicks ());
      _channelUIStates[channel]->progress = progress;
      if (shouldRepaint)
        _channelStrips[channel]->repaint ();
    }
  else
    {
      // No background rather than a black one — the sphere shows through.
      _motionComponent->setBackgroundColour (juce::Colours::transparentBlack);
    }

  // Update loop length display with global playhead (INT mode only).
  // Display = 1 bar reference.  Playhead = position within the bar.
  // In EXT mode, LoopLengthDisplay interpolates from setExternalBeat().
  if (_clockMode == 0)
    {
      auto const beatsPerBar = _engine.getBeatsPerBar ();
      auto const ticksPerBeat
          = static_cast<float> (TempoClock::getTicksPerBeat ());
      auto const totalTicksPerBar
          = static_cast<float> (beatsPerBar) * ticksPerBeat;

      auto const ticksInBar
          = static_cast<float> (measure.beat ()) * ticksPerBeat
            + static_cast<float> (measure.tick ());
      auto const barPosition = ticksInBar / totalTicksPerBar;

      for (auto channel = 0u; channel < _engine.getNumChannels (); ++channel)
        {
          _loopLengthDisplay->setPlayheadPosition (
              static_cast<int> (channel), barPosition);
        }
    }

  for (auto channel = 0u; channel < _engine.getNumChannels (); ++channel)
    {
      auto playingPattern = _engine.getPlayingPattern (channel);
      if (playingPattern)
        {
          auto const playPosition = playingPattern->getPlayPosition ();
          _channelUIStates[channel]->progress = playPosition;
          if (shouldRepaint)
            _channelStrips[channel]->repaint ();
        }
      else if (!recordingPattern || recordingPattern->getChannel () != channel)
        {
          if (!juce::exactlyEqual (_channelUIStates[channel]->progress, 1.f))
            {
              _channelUIStates[channel]->progress = 1.f;
              _channelStrips[channel]->repaint ();
            }
        }
    }
}

void
A3MotionUIComponent::padLEDCallback (int step)
{
  for (auto channel = 0u; channel < _ioAdapter->getNumChannels (); ++channel)
    {
      auto const channelColour = _channelUIStates[channel]->colour;
      for (auto pad = 0u; pad < _ioAdapter->getNumPadsPerChannel (); ++pad)
        {
          // All 4 buttons of a clip slot share that slot's Pattern, so
          // their LEDs stay in sync.
          auto const slot = slotForPadIndex[pad];
          auto const status = _patterns[channel][slot]
                                   ? _patterns[channel][slot]->getStatus ()
                                   : Pattern::Status::Empty;
          auto const statusLast
              = _patterns[channel][slot]
                    ? _patterns[channel][slot]->getLastStatus ()
                    : Pattern::Status::Empty;

          // Play|Pause is the one pad that must be readable at a glance:
          // green while actually playing, channel colour otherwise (idle/
          // empty/recording), so play vs. paused/stopped is unambiguous.
          bool const isPlayingOnPlayPause
              = padFunctionByPadIndex[pad] == PadFunction::PlayPause
                && (status == Pattern::Status::Playing
                    || status == Pattern::Status::ScheduledForPlaying);
          auto const base
              = isPlayingOnPlayPause ? toColour (theme ().accent)
                                     : channelColour;

          auto const colour = channelColourForPadStatus (
              base, status, statusLast, step);
          _ioAdapter->getPadLED (channel, pad)
              = juce::VariantConverter<juce::Colour>::toVar (colour);
        }
    }
}

juce::Colour
A3MotionUIComponent::channelColourForPadStatus (juce::Colour base,
                                                Pattern::Status status,
                                                Pattern::Status statusLast,
                                                int step)
{
  switch (status)
    {
    case Pattern::Status::Empty:
      return base.darker (0.85f);
    case Pattern::Status::Idle:
      return base.darker (0.3f);
    case Pattern::Status::ScheduledForRecording:
      return step % 2 == 0 ? base : base.darker (0.6f);
    case Pattern::Status::Recording:
      return base;
    case Pattern::Status::ScheduledForPlaying:
      return step % 2 == 0 ? base : base.darker (0.6f);
    case Pattern::Status::Playing:
      return base;
    case Pattern::Status::ScheduledForIdle:
      jassert (statusLast != Pattern::Status::ScheduledForRecording
               && statusLast != Pattern::Status::Idle);
      return scheduledForIdleLEDColour (base, step, statusLast);
    }
  return base.darker (0.85f);
}

juce::Colour
A3MotionUIComponent::scheduledForIdleLEDColour (juce::Colour base, int step,
                                                Pattern::Status statusLast)
{
  // one-shot recording: don't blink when scheduled for idle
  if (_engine.getRecordingMode () == MotionEngine::RecordingMode::OneShot
      && statusLast == Pattern::Status::Recording)
    {
      return base;
    }

  if (step % 2 == 0)
    {
      return base.darker (0.85f);
    }
  else
    {
      return base.darker (0.6f);
    }
}

void
A3MotionUIComponent::createPadRowDisplays ()
{
  for (auto slot = 0u; slot < numClipSlots; ++slot)
    {
      auto display = std::make_unique<PadRowDisplay> (static_cast<int> (slot));
      for (auto ch = 0u; ch < _channelUIStates.size ()
                         && ch < PadRowDisplay::numChannels;
           ++ch)
        {
          display->setChannelColour (static_cast<int> (ch),
                                     _channelUIStates[ch]->colour);
        }
      addChildComponent (*display);
      // Hidden: no longer part of the visible layout (see resized()), but
      // keeps receiving its normal update calls underneath.
      display->setVisible (false);
      _padRowDisplays.push_back (std::move (display));
    }

  // Set initial trajectory icons now that all displays exist
  for (auto slot = 0u; slot < numClipSlots; ++slot)
    {
      for (auto ch = 0u; ch < _engine.getNumChannels (); ++ch)
        {
          if (slot < _patterns[ch].size () && _patterns[ch][slot])
            {
              updatePadRowLabel (ch, slot);
              registerPatternDisplayData (_patterns[ch][slot]);
            }
        }
    }

  // Set initial row highlight on LoopLength row
  for (auto ch = 0u; ch < _engine.getNumChannels (); ++ch)
    {
      _loopLengthDisplay->setRowHighlighted (static_cast<int> (ch), true);
    }
}

void
A3MotionUIComponent::updatePadRowLabel (index_t channel, index_t slot)
{
  if (slot >= numClipSlots)
    return;

  if (slot >= _padRowDisplays.size ())
    return;

  if (_patterns[channel][slot])
    {
      auto const &name = _patterns[channel][slot]->getName ();
      auto libIndex = _patternLibrary->indexForName (name);

      if (libIndex > 0)
        {
          // Use SVG path from the library entry for the icon
          auto const &entry = _patternLibrary->getEntry (libIndex);
          auto iconPath = svgDToPath (entry.svgPathData);
          if (!iconPath.isEmpty () || entry.hasJumpDots)
            {
              _padRowDisplays[slot]->setIconPath (
                  static_cast<int> (channel),
                  iconPath,
                  entry.jumpDots);
            }
          else
            {
              _padRowDisplays[slot]->setTickData (
                  static_cast<int> (channel), entry.ticks);
            }
          // Show pattern length in beats
          _padRowDisplays[slot]->setLengthBeats (
              static_cast<int> (channel), entry.lengthBeats);
          // Category prefix: "S" for system, "U" for user
          _padRowDisplays[slot]->setCategoryPrefix (
              static_cast<int> (channel),
              entry.category == PatternLibrary::Category::System ? "S" : "U");
        }
      else
        {
          // Pattern not in library (e.g. newly recorded, not yet saved)
          // Generate icon from the pattern's own tick data
          auto ticks = _patterns[channel][slot]->getTicks ();
          _padRowDisplays[slot]->setTickData (static_cast<int> (channel),
                                              ticks.positions);
          // Compute beats from tick count
          auto numTicks = _patterns[channel][slot]->getNumTicks ();
          auto ticksPerBeat = TempoClock::getTicksPerBeat ();
          int beats = ticksPerBeat > 0
                          ? static_cast<int> (numTicks / ticksPerBeat)
                          : 0;
          _padRowDisplays[slot]->setLengthBeats (
              static_cast<int> (channel), beats);
          _padRowDisplays[slot]->setCategoryPrefix (
              static_cast<int> (channel), "U");
        }
    }
  else
    {
      // Empty: clear tick data and set Empty type
      _padRowDisplays[slot]->setTickData (static_cast<int> (channel), {});
      _padRowDisplays[slot]->setTrajectoryType (
          static_cast<int> (channel), PadRowDisplay::TrajectoryType::Empty);
      _padRowDisplays[slot]->setLengthBeats (
          static_cast<int> (channel), 0);
      // Use the category from the library slot (slot+1) for the prefix,
      // even when the current channel has no pattern loaded
      auto const libSlotIndex = static_cast<int> (slot) + 1;
      if (libSlotIndex < _patternLibrary->getNumEntries ())
        {
          auto const &slotEntry = _patternLibrary->getEntry (libSlotIndex);
          _padRowDisplays[slot]->setCategoryPrefix (
              static_cast<int> (channel),
              slotEntry.category == PatternLibrary::Category::System ? "S" : "U");
        }
      else
        {
          _padRowDisplays[slot]->setCategoryPrefix (
              static_cast<int> (channel), "");
        }
    }
}

void
A3MotionUIComponent::setPreviewWithDisplayData (
    std::shared_ptr<Pattern> const &pattern)
{
  if (!pattern)
    return;

  auto const &name = pattern->getName ();
  auto libIndex = _patternLibrary->indexForName (name);

  if (libIndex > 0)
    {
      auto const &entry = _patternLibrary->getEntry (libIndex);
      auto displayPath = svgDToPath (entry.svgPathData);
      _motionComponent->setPreviewPattern (pattern, displayPath,
                                           entry.jumpDots);
    }
  else
    {
      // No library entry (e.g. live recording) — no display path
      _motionComponent->setPreviewPattern (pattern);
    }
}

void
A3MotionUIComponent::refreshPatternDisplayFromTicks (
    std::shared_ptr<Pattern> const &pattern)
{
  if (!pattern || !_motionComponent)
    return;

  // Cut at teleports as well as at gaps, the same way the take's own trail is
  // drawn, so a jump the clip still has is not bridged by a line.
  juce::Path path;
  for (auto const &segment :
       trajectorySegments (pattern->getTicks ().positions))
    {
      path.startNewSubPath (segment.front ().x (), segment.front ().y ());
      for (size_t i = 1; i < segment.size (); ++i)
        path.lineTo (segment[i].x (), segment[i].y ());
    }

  _motionComponent->setPatternDisplayData (pattern, path, {});
}

void
A3MotionUIComponent::registerPatternDisplayData (
    std::shared_ptr<Pattern> const &pattern)
{
  if (!pattern)
    return;

  auto const &name = pattern->getName ();
  auto libIndex = _patternLibrary->indexForName (name);

  // A shape made of dots has no line to draw, and its dots are only in the
  // file: keep taking those from the library.
  if (libIndex > 0)
    {
      auto const &entry = _patternLibrary->getEntry (libIndex);
      if (entry.hasJumpDots && svgDToPath (entry.svgPathData).isEmpty ())
        {
          _motionComponent->setPatternDisplayData (pattern, {},
                                                   entry.jumpDots);
          return;
        }
    }

  // Everything else is drawn from the ticks, because that is what plays.
  //
  // Taking the line from the file was right while the file was a picture of
  // the pattern. It stopped being one when the take started going to disk as
  // it was played, with the closing move a setting laid over it: the blob
  // followed the ending the fade gives it and the line showed a take whose
  // ends do not meet.
  refreshPatternDisplayFromTicks (pattern);
}

int
A3MotionUIComponent::trajectoryNameToIndex (std::string const &name) const
{
  return _patternLibrary->indexForName (name);
}

std::shared_ptr<Pattern>
A3MotionUIComponent::createPatternForIndex (int index, index_t channel)
{
  auto p = _patternLibrary->loadPattern (index);
  if (p)
    p->setChannel (channel);
  return p;
}

void
A3MotionUIComponent::endRecording ()
{
  auto pattern = _engine.getRecordingPattern ();
  if (!pattern || !_recordingSlot.has_value ())
    return;

  auto const channel = _recordingSlot->first;
  auto const slot = _recordingSlot->second;

  auto const written = pattern->writtenTicks ();
  auto const anyWritten
      = std::any_of (written.begin (), written.end (),
                     [] (bool isWritten) { return isWritten; });

  // Where the write head stands is where the take stops, and that edge -- the
  // last tick of the freshest pass against the previous pass still sitting
  // after it -- is what breaks visibly. Asked before stopping, because the
  // engine forgets it the moment it does.
  auto const progress = _engine.getRecordingProgress ();
  auto const stopTick
      = progress >= 0.f
            ? std::optional<index_t>{ static_cast<index_t> (
                  progress * static_cast<float> (pattern->getNumTicks ())) }
            : std::nullopt;

  if (anyWritten)
    {
      // How long the closing move wants to be, so that it travels at the
      // speed the take was played at. Written into the bar as the value the
      // fade starts from: a fixed setting is as likely to crawl as to race,
      // because how long the move needs comes out of the take rather than out
      // of a preference.
      if (stopTick)
        {
          auto const natural
              = naturalFadeTicks (pattern->getTicks ().positions, *stopTick);
          if (natural > 0)
            _clipUIParams[channel][slot].fadeSixteenths = std::clamp (
                static_cast<int> (std::lround (
                    static_cast<double> (natural)
                    / ticksPerFadeStep (TempoClock::getTicksPerBeat ()))),
                1, 16);
        }

      // Before stopping, because the save happens on the Stopped message and
      // has to carry the filled stretches with it.
      closeRecordingSeams (
          *pattern,
          fadeTicksFor (_clipUIParams[channel][slot].fadeSixteenths),
          stopTick);
    }

  _engine.stopPattern (pattern, _now);

  if (anyWritten)
    {
      // Started from the Stopped message rather than here. Stopping is
      // asynchronous, so playing straight after it left the pattern in a state
      // the Play pad does not know — and the first press on it did nothing.
      pattern->setPlaybackLength (getPlaybackLength (channel, slot));
      _playWhenRecordingStops = pattern;
    }
  else
    {
      // Nothing was ever played into it. Put back what the slot held rather
      // than leaving a clip made of nothing.
      _patterns[channel][slot] = _patternBeforeRecording;
      updateControlReadout ("recording discarded - nothing played");
    }

  _recordingSlot.reset ();
  _patternBeforeRecording.reset ();
  if (_motionComponent)
    _motionComponent->setRecordingUnderlay (nullptr);
  selectClip (channel, slot);
}

void
A3MotionUIComponent::saveRecordedPattern (
    std::shared_ptr<Pattern> const &pattern, index_t channel, index_t slot)
{
  if (!pattern || pattern->getNumTicks () == 0)
    return;

  // Name the recording with a timestamp
  auto now = juce::Time::getCurrentTime ();
  auto name = "Rec_" + now.formatted ("%H%M%S").toStdString ();
  pattern->setName (name);

  // Save to user directory
  auto newIndex = _patternLibrary->saveUserPattern (pattern);
  std::cout << "saveRecordedPattern: channel=" << channel
            << " slot=" << slot
            << " ticks=" << pattern->getNumTicks ()
            << " name=" << name
            << " newIndex=" << newIndex << std::endl;
  if (newIndex > 0)
    {
      std::cout << "Recording saved as user pattern '" << name
                << "' at library index " << newIndex << std::endl;

      // The take stays in the slot. It used to be swapped for a fresh load of
      // the file it had just been written to, for the sake of the display data
      // that came with it -- and the line is drawn from the ticks now, so
      // there is nothing left to fetch.
      //
      // The round trip cost more than it gave: loading resamples the shape by
      // arc length, so every tick moves. The tick where the take stopped means
      // nothing afterwards, so the closing move landed somewhere else, and the
      // stretch between two subpaths came back as a hole -- 45 ticks of one on
      // the take that showed it.
      registerPatternDisplayData (pattern);

      // Update fingerprint so the timer doesn't re-trigger for this save
      _lastLibraryFingerprint = _patternLibrary->getDirectoryFingerprint ();

      // Refresh the pad cell display to show the new recording
      updatePadRowLabel (channel, slot);

      // And the clip settings bar, if it happens to be showing this slot.
      // It reads _patterns[channel][slot], which was just replaced — without
      // this it went on showing the pattern that was there before, until the
      // next encoder turn happened to refresh it for another reason.
      if (channel == _clipSettingsChannel && slot == _clipSettingsSlot)
        updateClipSettingsDisplay ();
    }
}

void
A3MotionUIComponent::refreshAllPadRowLabels ()
{
  auto const numChannels = _engine.getNumChannels ();
  for (index_t ch = 0; ch < numChannels; ++ch)
    {
      for (index_t slot = 0; slot < numClipSlots; ++slot)
        {
          updatePadRowLabel (ch, slot);
        }
    }
}

void
A3MotionUIComponent::timerCallback ()
{
  // While a take runs, the bar under the pictogram fills and its write head
  // moves. Only then — the rest of the time nothing here changes on its own.
  if (_engine.isRecording ())
    updateClipSettingsDisplay ();

  // Every fortieth tick, which is the two seconds this used to run at.
  if (++_timerTick % 40 != 0)
    return;

  // Periodically check if pattern directories have changed
  auto fp = _patternLibrary->getDirectoryFingerprint ();
  if (fp != _lastLibraryFingerprint)
    {
      _lastLibraryFingerprint = fp;
      std::cout << "PatternLibrary: directory change detected, refreshing..."
                << std::endl;
      _patternLibrary->refresh ();
      refreshAllPadRowLabels ();
    }
}

void
A3MotionUIComponent::oscBundleReceived (const juce::OSCBundle &bundle)
{
  for (auto &element : bundle)
    {
      if (element.isMessage ())
        oscMessageReceived (element.getMessage ());
      else if (element.isBundle ())
        oscBundleReceived (element.getBundle ());
    }
}

void
A3MotionUIComponent::oscMessageReceived (const juce::OSCMessage &message)
{
  _oscMessageHandler->handleMessage (message, _clockMode);
}

void
A3MotionUIComponent::onChannelVU (int channel, float peak, float rms)
{
  if (static_cast<size_t> (channel) < _channelUIStates.size ())
    {
      _channelUIStates[static_cast<size_t> (channel)]->vuPeak = peak;
      _channelUIStates[static_cast<size_t> (channel)]->vuLevel = rms;
    }
}

void
A3MotionUIComponent::onSubwooferVU (float peak, float rms)
{
  _motionComponent->setSphereGlow (peak, rms);
}

void
A3MotionUIComponent::onEnergyGrid (float const *values, int count)
{
  _motionComponent->setEnergyGrid (values, count);
}

void
A3MotionUIComponent::onSpeakerVU (int speakerIndex, float peak, float rms)
{
  _motionComponent->setSpeakerLight (speakerIndex, peak, rms);
}

void
A3MotionUIComponent::onExternalBeatClock (int beat, int bar, float bpm)
{
  _statusBar->setExternalBPM (bpm);
  _statusBar->setBeatClock (beat, bar);
}

void
A3MotionUIComponent::onExternalBeatSync (int beat, int beatsPerBar)
{
  _loopLengthDisplay->setExternalBeat (beat, beatsPerBar);
}

// ── Global Settings helpers ──────────────────────────────────────────────────────

void
A3MotionUIComponent::openGlobalSettings ()
{
  if (_globalSettingsOpen)
    return;

  _globalSettingsOpen = true;
  _globalSettingsValueFieldSelected = false;
  _globalSettingsOptionIndex = 0;

  rebuildGlobalSettingsOptions ();

  // Reuse the Clip Settings panel's safe zone (real screen space carved out
  // of MotionComponent's bounds in resized(), not overlapping its OpenGL
  // context) so the menu has room to show its option rows properly.
  if (_clipSettings)
    _globalSettings->setBounds (_clipSettings->getBounds ());

  _globalSettings->setVisible (true);
  _globalSettings->toFront (true);
  resized (); // the menu takes the whole window, the sphere gives up its bounds

  // Pausing here was a concession to the RPi4's GPU. The rig runs on an Intel
  // NUC now and the sphere is meant to carry on behind the menu, but the option
  // stays for a machine that needs it again.
  if (_motionComponent && _pauseRenderingInMenu)
    _motionComponent->setRenderingPaused (true);
}

void
A3MotionUIComponent::rebuildGlobalSettingsOptions ()
{
  // Built fresh rather than patched: the list of skins changes underneath it
  // whenever the editor saves, renames or deletes one.
  std::vector<GlobalSettingsComponent::Option> options;
  _menuRowOrder.clear ();

  // Each row is added with what it does, so the two cannot drift apart.
  auto const add = [&] (MenuRow row, GlobalSettingsComponent::Option option) {
    _menuRowOrder.push_back (row);
    options.push_back (std::move (option));
  };

  add (MenuRow::ClockMode,
       { "Clockmode", { { "INT" }, { "EXT" }, { "PIO" } }, _clockMode });

  // Built from what is actually in config/skins, so a skin added on the
  // device shows up without a rebuild.
  _skinNames = availableSkins (getConfigFile ().getParentDirectory ());
  auto const active = activeSkinName (getConfigFile ());
  _skinIndex = juce::jmax (0, _skinNames.indexOf (active));

  std::vector<GlobalSettingsComponent::ValueItem> skinValues;
  for (auto const &name : _skinNames)
    skinValues.push_back ({ name });

  add (MenuRow::Skin, { "Skin", std::move (skinValues), _skinIndex });
  add (MenuRow::SkinEditor, { "Skin Editor", { { "open" } }, 0, true });
  add (MenuRow::Network, { "Network", { { "open" } }, 0, true });
  add (MenuRow::ButtonLeds, { "Button LEDs", { { "open" } }, 0, true });
  add (MenuRow::PatternFolder, { "Pattern Folder", { { "open" } }, 0, true });
  add (MenuRow::SphereInMenu,
       { "Sphere in Menu", { { "off" }, { "on" } },
         _pauseRenderingInMenu ? 0 : 1 });
  _globalSettings->setOptions (std::move (options));
  _globalSettings->setOptionIndex (_globalSettingsOptionIndex);
  _globalSettings->setValueFieldSelected (false);
}

void
A3MotionUIComponent::closeGlobalSettings ()
{
  if (!_globalSettingsOpen)
    return;

  // The editor is a page of this menu, so closing the menu leaves it first —
  // that is also what saves the edited skin.
  closeSkinEditor ();

  _globalSettingsOpen = false;
  _globalSettingsValueFieldSelected = false;
  _globalSettings->setVisible (false);
  _globalSettings->setValueFieldSelected (false);
  resized (); // sphere and clip settings get their bounds back
  if (_motionComponent)
    _motionComponent->setRenderingPaused (false);
}

std::optional<A3MotionUIComponent::MenuRow>
A3MotionUIComponent::browsedMenuRow () const
{
  if (_globalSettingsOptionIndex < 0
      || _globalSettingsOptionIndex >= (int)_menuRowOrder.size ())
    return {};

  return _menuRowOrder[(size_t)_globalSettingsOptionIndex];
}

void
A3MotionUIComponent::confirmGlobalSettingsOption ()
{
  if (!_globalSettingsOpen || !_globalSettingsValueFieldSelected)
    return;

  int const chosen = _globalSettings->getSelectedValueIndex ();

  auto const row = browsedMenuRow ();
  if (!row.has_value ())
    return;

  switch (row.value ())
    {
    case MenuRow::ClockMode: applyClockMode (chosen); break;
    case MenuRow::Skin: applySkin (chosen); break;
    case MenuRow::SkinEditor: openSkinEditor (); break;
    case MenuRow::Network:
      openConfigPage ("Network", { "oscSender", "oscReceiver" });
      break;
    case MenuRow::ButtonLeds:
      openConfigPage ("Button LEDs", { "buttonLeds" });
      break;
    case MenuRow::PatternFolder:
      openConfigPage ("Pattern Folder", { "patternDir" });
      break;
    case MenuRow::SphereInMenu: applyPauseRendering (chosen == 0); break;
    }

  _globalSettings->setActiveValueIndex (_globalSettingsOptionIndex, chosen);

  _globalSettingsValueFieldSelected = false;
  _globalSettings->setValueFieldSelected (false);
}

void
A3MotionUIComponent::applyRecMode (int index)
{
  if (index < 0 || index >= static_cast<int> (recMenuModes.size ()))
    return;

  _recMode = recMenuModes[static_cast<size_t> (index)];
  _engine.setRecMode (_recMode);

  saveSettings (getPersistedSettingsFile (),
                AppSettings{ _clockMode, _recMode });
}

void
A3MotionUIComponent::applyClockMode (int mode)
{
  if (mode == _clockMode)
    return;

  _clockMode = mode;

  if (_clockMode != 0)
    {
      if (std::abs (_internalBPM) < 0.0001f)
        _internalBPM = _engine.getTempoBPM ();
      _engine.resetTempo ();
    }
  else
    {
      if (_internalBPM > 0.f)
        {
          _engine.setTempoBPM (_internalBPM);
          _valueBPM = static_cast<double> (_internalBPM);
        }
    }

  if (runsOnHardware ())
    _ioAdapter->getButtonLED (Button::ClockMode) = (_clockMode != 0);

  _statusBar->setClockMode (_clockMode);
  _loopLengthDisplay->setClockMode (_clockMode);

  auto clockModeMsg = juce::OSCMessage ("/clockmode");
  clockModeMsg.addInt32 (_clockMode);
  _oscSender.send (clockModeMsg);

  saveSettings (getPersistedSettingsFile (),
               AppSettings{ _clockMode, _recMode });
}




void
A3MotionUIComponent::resetEditedSkinToDefault ()
{
  auto const source = skinFile (getConfigFile ().getParentDirectory (),
                                protectedSkinName);
  auto const restored = juce::JSON::parse (source.loadFileAsString ());
  if (!restored.isObject ())
    return;

  // Under the name it already had: the skin is put right, not replaced. What
  // is on screen changes at once; the file follows when the editor is left,
  // like every other edit here.
  _skinEditor->setSkin (restored, _skinEditor->getSkinName ());
  applyEditedSkin ();
}

void
A3MotionUIComponent::applySkinNamed (juce::String const &name)
{
  _skinNames = availableSkins (getConfigFile ().getParentDirectory ());
  auto const index = _skinNames.indexOf (name);
  if (index < 0)
    return;

  applySkin (index);
}

void
A3MotionUIComponent::openSkinEditor ()
{
  if (_skinEditorOpen)
    return;

  auto const file = skinFile (getConfigFile ().getParentDirectory (),
                              _skinNames[juce::jlimit (
                                  0, juce::jmax (0, _skinNames.size () - 1),
                                  _skinIndex)]);

  // Through the rename, like every other read of a skin: a file still carrying
  // the old spellings would otherwise show them here while the app runs on the
  // new ones, and the first save would write a mixture.
  _skinEditor->setSkin (migrateSkinNames (juce::JSON::parse (file.loadFileAsString ())),
                        file.getFileNameWithoutExtension ());
  _skinEditorOpen = true;
  _globalSettings->setVisible (false);
  _skinEditor->setVisible (true);
  _skinEditor->toFront (true);
}

void
A3MotionUIComponent::openConfigPage (juce::String const &title,
                                     juce::StringArray const &keys)
{
  // A slice of config.json rather than the whole file: a page with one thing
  // on it is a page somebody can read. The slice is written back key by key,
  // so the rest of the file is untouched by a visit here.
  auto const config = juce::JSON::parse (getConfigFile ().loadFileAsString ());

  auto *slice = new juce::DynamicObject ();
  for (auto const &key : keys)
    {
      auto const identifier = juce::Identifier (key);
      if (config.hasProperty (identifier))
        slice->setProperty (identifier, config[identifier]);
    }

  _configPageKeys = keys;
  _skinEditor->setDocument (juce::var (slice), title, false,
                            SkinEditorComponent::Numbers::Typed);
  _skinEditorOpen = true;
  _globalSettings->setVisible (false);
  _skinEditor->setVisible (true);
  _skinEditor->toFront (true);
}

void
A3MotionUIComponent::applyEditedConfigPage ()
{
  // The hardware reads the running configuration, not the file, so a colour
  // being picked has to go in there straight away — otherwise the LEDs only
  // catch up when the page is left and the file watcher comes round, which
  // reads as a picker that does nothing.
  auto const edited = _skinEditor->getSkin ();
  auto const keys = _configPageKeys;

  juce::MessageManager::callAsync ([edited, keys] {
    userConfig = withKeysReplaced (userConfig, edited, keys);
  });
}

void
A3MotionUIComponent::saveConfigPage ()
{
  if (_configPageKeys.isEmpty ())
    return;

  auto config = juce::JSON::parse (getConfigFile ().loadFileAsString ());
  auto *object = config.getDynamicObject ();
  if (object == nullptr)
    return;

  config = withKeysReplaced (config, _skinEditor->getSkin (), _configPageKeys);

  getConfigFile ().replaceWithText (
      juce::JSON::toString (config, false) + "\n", false, false, "\n");
  _configPageKeys.clear ();

  // Ports and hosts are read when a socket opens, so they take effect at the
  // next start rather than here. Saying so beats a setting that looks live
  // and is not.
  updateControlReadout ("network saved - restart to apply");
}

void
A3MotionUIComponent::applyPauseRendering (bool paused)
{
  _pauseRenderingInMenu = paused;

  auto config = juce::JSON::parse (getConfigFile ().loadFileAsString ());
  if (auto *object = config.getDynamicObject ())
    {
      auto *ui = config["ui"].getDynamicObject ();
      if (ui != nullptr)
        {
          ui->setProperty ("pauseRenderingInMenu", paused);
          object->setProperty ("ui", config["ui"]);
          getConfigFile ().replaceWithText (
              juce::JSON::toString (config, false) + "\n", false, false,
              "\n");
        }
    }

  if (_motionComponent)
    _motionComponent->setRenderingPaused (paused);
}

void
A3MotionUIComponent::applyTheme ()
{
  // A channel's colour was read once, at construction, and kept in
  // ChannelUIState — so editing it in the skin changed the file and the
  // theme and nothing on the screen. Every blob, pad and frame is drawn
  // from this copy, which is why it has to be refreshed here.
  auto const numChannels = _engine.getNumChannels ();
  for (index_t channel = 0;
       channel < numChannels && channel < (index_t)numThemeChannels; ++channel)
    if (_channelUIStates[channel] != nullptr)
      _channelUIStates[channel]->colour = toColour (theme ().channel[channel]);

  // The clip settings bar was handed its channel's colour by value too.
  if (_clipSettings && _channelUIStates[_clipSettingsChannel] != nullptr)
    _clipSettings->setTarget (static_cast<int> (_clipSettingsChannel),
                              static_cast<int> (_clipSettingsSlot),
                              _channelUIStates[_clipSettingsChannel]->colour);
}

void
A3MotionUIComponent::openColourPicker (juce::String const &path)
{
  auto const document = _skinEditor->getSkin ();
  auto const channel = [&document, &path] (char const *name) {
    return (juce::uint8)juce::jlimit (
        0, 255, (int)skinValue (document, path + "." + name));
  };

  _colourPath = path;
  _colourPicker->setColour (
      juce::Colour (channel ("r"), channel ("g"), channel ("b")), path);
  _colourPickerOpen = true;
  _skinEditor->setVisible (false);
  _colourPicker->setVisible (true);
  _colourPicker->toFront (true);
}

void
A3MotionUIComponent::closeColourPicker ()
{
  if (!_colourPickerOpen)
    return;

  _colourPickerOpen = false;
  _colourPath = {};
  _colourPicker->setVisible (false);
  _skinEditor->setVisible (true);
  _skinEditor->toFront (true);
}

void
A3MotionUIComponent::applyPickedColour ()
{
  if (_colourPath.isEmpty ())
    return;

  // Back into the document as r/g/b: the file keeps saying what it always
  // said, and HSL is only how a person reaches the number.
  auto document = _skinEditor->getSkin ();
  auto const colour = _colourPicker->getColour ();

  setSkinValue (document, _colourPath + ".r", colour.getRed (), true);
  setSkinValue (document, _colourPath + ".g", colour.getGreen (), true);
  setSkinValue (document, _colourPath + ".b", colour.getBlue (), true);

  applyEditedSkin ();
}

void
A3MotionUIComponent::showKeyboard (bool shown)
{
  shown ? onScreenKeyboard::show () : onScreenKeyboard::hide ();
  refreshKeyboardIcon ();
}

void
A3MotionUIComponent::toggleKeyboard ()
{
  // Always available, whatever is on screen: it is the system's keyboard and
  // it types into whatever has the focus.
  auto const wasShown = onScreenKeyboard::isShown ();
  showKeyboard (!wasShown);

  // Showing it over a row that can be typed says what it is for. A row that
  // is only turned stays that way; the keyboard is then simply up.
  if (!wasShown && _skinEditorOpen && !_skinEditor->isNaming ())
    _skinEditor->beginTypingBrowsedRow ();
}

void
A3MotionUIComponent::refreshKeyboardIcon ()
{
  if (!_statusBar)
    return;

  using State = StatusBar::KeyboardState;

  auto const state = onScreenKeyboard::isShown () ? State::Shown
                                                 : State::Available;

  _statusBar->setKeyboardState (state);
}

void
A3MotionUIComponent::saveSkinAsNew ()
{
  auto const configDir = getConfigFile ().getParentDirectory ();
  auto const name = nextFreeSkinName (configDir, _skinEditor->getSkinName ());

  // The edited state is what gets copied — "save as new" on a skin that has
  // been turned about is meant to keep what is on the screen, not what was
  // last written.
  skinFile (configDir, name)
      .replaceWithText (juce::JSON::toString (_skinEditor->getSkin (), false)
                            + "\n",
                        false, false, "\n");

  writeActiveSkin (getConfigFile (), name);
  reopenEditorOn (name);
}

void
A3MotionUIComponent::renameEditedSkin (juce::String const &name)
{
  auto const configDir = getConfigFile ().getParentDirectory ();

  // Written first: a rename moves the file, and the edits would be left in
  // the old one.
  saveEditedSkin ();

  if (renameSkin (configDir, _skinEditor->getSkinName (), name))
    reopenEditorOn (name);
}

void
A3MotionUIComponent::deleteEditedSkin ()
{
  auto const configDir = getConfigFile ().getParentDirectory ();

  if (!deleteSkin (configDir, _skinEditor->getSkinName ()))
    return; // the last one stays — see deleteSkin

  reopenEditorOn (activeSkinName (getConfigFile ()));
}

void
A3MotionUIComponent::reopenEditorOn (juce::String const &name)
{
  auto const file = skinFile (getConfigFile ().getParentDirectory (), name);

  _skinEditor->setSkin (migrateSkinNames (juce::JSON::parse (file.loadFileAsString ())),
                        file.getFileNameWithoutExtension ());

  // The menu underneath is showing a list of skins that just changed.
  rebuildGlobalSettingsOptions ();
  applyEditedSkin ();
}

void
A3MotionUIComponent::closeSkinEditor ()
{
  if (!_skinEditorOpen)
    return;

  // Written on the way out rather than on every detent: turning an encoder
  // produces a value per tick, and a file save per tick would spend the
  // session writing to disk and waking the file watcher.
  if (_configPageKeys.isEmpty ())
    saveEditedSkin ();
  else
    saveConfigPage ();

  closeColourPicker ();
  showKeyboard (false);
  _skinEditorOpen = false;
  _skinEditor->setVisible (false);
  _globalSettings->setVisible (true);
  _globalSettings->toFront (true);
}

void
A3MotionUIComponent::applyEditedSkin ()
{
  if (!_configPageKeys.isEmpty ())
    {
      applyEditedConfigPage ();
      return;
    }

  // Straight to the theme, so the change is visible on the sphere behind the
  // editor while the encoder is still turning. The file follows on close.
  auto const edited = _skinEditor->getSkin ();

  // Everything the sphere reads from a skin, handed over the way the file
  // watcher hands it over — corona, glow, speaker light, the energy net, blob
  // and sphere size, the recording underlay. Only sphereScale used to come
  // through here, so every other one of those values sat unchanged while its
  // encoder turned and only appeared once the editor was closed. A value you
  // cannot see while you set it is a value you are setting blind.
  if (_motionComponent != nullptr)
    _motionComponent->applyVisualConfig (edited);

  juce::Component::SafePointer<A3MotionUIComponent> safeThis{ this };
  auto const loaded = loadTheme (edited);
  juce::MessageManager::callAsync ([safeThis, loaded] {
    if (safeThis != nullptr)
      applyThemeEverywhere (loaded, *safeThis);
  });
}

void
A3MotionUIComponent::saveEditedSkin ()
{
  auto const edited = _skinEditor->getSkinName ();
  auto const target = skinNameToWriteTo (edited);

  auto const file
      = skinFile (getConfigFile ().getParentDirectory (), target);

  // Rewritten whole, unlike config.json: a skin file is this editor's own
  // output, and its shape is generated rather than hand-arranged.
  file.replaceWithText (
      juce::JSON::toString (_skinEditor->getSkin (), false) + "\n", false,
      false, "\n");

  // The edits branched off the default, so the skin they landed in is the one
  // that should now be in force -- otherwise they would be written and then
  // immediately not shown.
  if (target != edited)
    applySkinNamed (target);
}




void
A3MotionUIComponent::previewSkin (int index)
{
  if (index < 0 || index >= _skinNames.size ())
    return;

  // Shown while the encoder is still turning, so a skin is chosen by
  // looking at it rather than by reading its name. Nothing is written —
  // the press is what makes it the one that is running.
  auto const file = skinFile (getConfigFile ().getParentDirectory (),
                              _skinNames[index]);
  auto const loaded = loadTheme (migrateSkinNames (juce::JSON::parse (file.loadFileAsString ())));

  juce::Component::SafePointer<A3MotionUIComponent> safeThis{ this };
  juce::MessageManager::callAsync ([safeThis, loaded] {
    if (safeThis != nullptr)
      applyThemeEverywhere (loaded, *safeThis);
  });
}

void
A3MotionUIComponent::applySkin (int index)
{
  if (index < 0 || index >= _skinNames.size ())
    return;

  _skinIndex = index;

  // Written to config.json rather than kept in a variable: which skin is
  // running is operation, and config.json is where operation lives. A hand
  // edit and this menu then say the same thing in the same place.
  writeActiveSkin (getConfigFile (), _skinNames[index]);

  // The watcher that normally picks that up runs inside the GL render loop,
  // and the sphere is not rendering while the menu covers it — so the half of
  // the reload that is pure message-thread work happens here. The sphere's own
  // tuning follows from the watcher as soon as it draws again, which is when
  // the menu closes and it is visible in the first place.
  auto const file = skinFile (getConfigFile ().getParentDirectory (),
                              _skinNames[index]);
  // Queued rather than applied on the spot, so that a reload the render
  // thread dispatched a moment ago runs first and this one has the last
  // word. Applying directly let a callback that was already in flight put
  // the previous skin back.
  auto const loaded = loadTheme (migrateSkinNames (juce::JSON::parse (file.loadFileAsString ())));
  juce::Component::SafePointer<A3MotionUIComponent> safeThis{ this };
  juce::MessageManager::callAsync ([safeThis, loaded] {
    if (safeThis != nullptr)
      applyThemeEverywhere (loaded, *safeThis);
  });
}

void
A3MotionUIComponent::refreshFonts ()
{
  // The status bar's height and the keyboard's follow the font sizes, so
  // this is a layout change and not only a repaint — and the layout has to come first: the
  // bar sizes its own text against the height it holds, so giving it the new
  // height is what lets the text follow.
  resized ();

  if (auto *root = getTopLevelComponent ())
    root->repaint ();

  saveSettings (getPersistedSettingsFile (),
               AppSettings{ _clockMode, _recMode });
}

juce::File
A3MotionUIComponent::getConfigFile () const
{
  return juce::File::getCurrentWorkingDirectory ().getChildFile (
      "config/config.json");
}

juce::File
A3MotionUIComponent::getPersistedSettingsFile () const
{
  return juce::File::getCurrentWorkingDirectory ()
      .getChildFile ("config/ui_state.json");
}

void
A3MotionUIComponent::selectClip (index_t channel, index_t slot)
{
  _clipSettingsChannel = channel;
  _clipSettingsSlot = slot;
  _clipSettingsMenuIndex = 0;
  _clipSettingsSubIndex = 0;
  _clipSettings->setTarget (static_cast<int> (channel),
                                   static_cast<int> (slot),
                                   _channelUIStates[channel]->colour);
  updateClipSettingsDisplay ();
}

void
A3MotionUIComponent::handleClipSettingsScroll (index_t channel, int increment)
{
  if (channel != _clipSettingsChannel || increment == 0)
    return;

  static constexpr int numMenuItems = ClipSettingsComponent::numParameters;
  selectClipSettingsSection (
      (_clipSettingsMenuIndex + increment % numMenuItems + numMenuItems)
      % numMenuItems);
}

void
A3MotionUIComponent::selectClipSettingsSection (int index)
{
  // Sets rather than cycles: a finger names the section it wants outright,
  // where the encoder has to turn past the ones in between.
  if (index < 0 || index >= ClipSettingsComponent::numParameters)
    return;

  _clipSettingsMenuIndex = index;
  _clipSettingsSubIndex = 0;
  updateClipSettingsDisplay ();
}

void
A3MotionUIComponent::selectClipSettingsSubElement (int index)
{
  if (index < 0 || index >= numSubElementsForSection (_clipSettingsMenuIndex))
    return;

  _clipSettingsSubIndex = index;
  updateClipSettingsDisplay ();
}

int
A3MotionUIComponent::numSubElementsForSection (int menuIndex) const
{
  if (menuIndex == ClipSettingsComponent::elevationIndex)
    return 6;
  if (menuIndex == ClipSettingsComponent::motionIndex)
    return 4; // speed, direction, end-action, seam
  if (menuIndex == ClipSettingsComponent::filterIndex)
    return 2;
  if (menuIndex == ClipSettingsComponent::trajectoryIndex)
    return 2; // the shape itself, and the length of the next take
  return 1;
}

void
A3MotionUIComponent::handleClipSettingsSubElementCycle (index_t channel)
{
  if (channel != _clipSettingsChannel)
    return;

  auto const numSub = numSubElementsForSection (_clipSettingsMenuIndex);
  selectClipSettingsSubElement ((_clipSettingsSubIndex + 1) % numSub);
}

void
A3MotionUIComponent::handleClipSettingsValueChange (index_t channel,
                                                    int increment)
{
  if (channel != _clipSettingsChannel || increment == 0)
    return;

  auto const slot = _clipSettingsSlot;
  auto &params = _clipUIParams[channel][slot];

  switch (_clipSettingsMenuIndex)
    {
    case 0: // Trajectory Shape — the shape itself (0), or the length the next
            // take will have (1)
      {
        if (_clipSettingsSubIndex == 1)
          {
            // A setting for the next recording, not a property of what is in
            // the slot: an existing pattern's length is its tick count, and
            // changing that would throw its data away.
            params.recordLengthLog2
                = std::clamp (params.recordLengthLog2 + increment,
                              speedLog2Min, speedLog2Max);
            break;
          }

        auto &pattern = _patterns[channel][slot];

        int currentIndex = 0;
        if (pattern)
          currentIndex = trajectoryNameToIndex (pattern->getName ());

        auto const numLibEntries = _patternLibrary->getNumEntries ();
        int newIndex = currentIndex + increment;
        if (newIndex < 0)
          newIndex = numLibEntries - 1;
        else if (newIndex >= numLibEntries)
          newIndex = 0;

        bool const wasPlaying
            = pattern
              && (pattern->getStatus () == Pattern::Status::Playing
                  || pattern->getStatus ()
                         == Pattern::Status::ScheduledForPlaying);

        if (pattern)
          {
            auto status = pattern->getStatus ();
            if (status == Pattern::Status::Playing
                || status == Pattern::Status::Recording)
              _engine.stopPattern (pattern, _now);
            _motionComponent->unsetPreviewPattern (pattern);
            _motionComponent->removePatternDisplayData (pattern);
          }

        if (newIndex == 0)
          {
            pattern = nullptr;
          }
        else
          {
            pattern = createPatternForIndex (newIndex, channel);
            registerPatternDisplayData (pattern);

            if (wasPlaying && pattern)
              {
                pattern->setPlaybackLength (
                    getPlaybackLength (channel, slot));
                _engine.playPattern (pattern, _now);
              }
          }

        updatePadRowLabel (channel, slot);
        break;
      }
    case 1: // Elevation — reach (0), clip-top (1), clip-bottom (2),
            // mirror-south (3), flat (4), or flat-elevation (5)
      {
        auto &pattern = _patterns[channel][slot];
        if (!pattern)
          break;

        switch (_clipSettingsSubIndex)
          {
          case 0:
            pattern->setReach (pattern->getReach () + increment * 0.05f);
            break;
          case 1:
            pattern->setClipTop (pattern->getClipTop () + increment * 0.05f);
            break;
          case 2:
            pattern->setClipBottom (pattern->getClipBottom ()
                                    + increment * 0.05f);
            break;
          case 3:
            // Toggle: turning right selects South, left selects North —
            // tied to physical direction rather than pulse-counting, so
            // it can't desync/flicker from missed encoder ticks.
            pattern->setMirrorSouth (increment > 0);
            break;
          case 4:
            pattern->setFlat (increment > 0);
            break;
          default:
            pattern->setFlatElevation (pattern->getFlatElevation ()
                                       + increment * 0.05f);
            break;
          }
        break;
      }
    case 2: // Motion — speed (0), direction (1), or end-action (2)
      switch (_clipSettingsSubIndex)
        {
        case 0:
          {
            // Turning right (increment > 0) should move the knob right,
            // i.e. toward speedLog2Min (the fast/1-128th end) — subtract,
            // not add, since speedLog2 runs the opposite way from the
            // knob's visual left-to-right sweep (see speedLog2Min/Max).
            params.speedLog2 = std::clamp (params.speedLog2 - increment,
                                           speedLog2Min, speedLog2Max);
            // Playback length IS speed here — a full pattern cycle spans
            // this many beats, so halving/doubling it halves/doubles how
            // fast the ball moves, quantized to musical note values.
            auto &pattern = _patterns[channel][slot];
            if (pattern)
              {
                pattern->setPlaybackLength (
                    getPlaybackLength (channel, slot));
              }
            break;
          }
        case 1:
          params.direction = (params.direction + increment % 2 + 2) % 2;
          applyMotionMode (channel, slot);
          break;
        case 2:
          params.endAction = (params.endAction + increment % 4 + 4) % 4;
          applyMotionMode (channel, slot);
          break;
        default:
          {
            // How long the take's closing move lasts. A playback setting: it
            // takes effect on whatever is in the slot, at once, and is
            // recomputed from the take as played, so it can be turned down
            // again as freely as up.
            params.fadeSixteenths
                = std::clamp (params.fadeSixteenths + increment, 0, 16);

            auto &pattern = _patterns[channel][slot];
            if (pattern)
              {
                applyFade (*pattern, fadeTicksFor (params.fadeSixteenths));

                // The drawn line comes from the library's file, which still
                // shows the ending the clip had before the fade touched it.
                refreshPatternDisplayFromTicks (pattern);

              }
            break;
          }
        }
      break;
    case 3: // Filter — sweep/Pot1 (0) or Q/Pot2 (1)
      if (_clipSettingsSubIndex == 0)
        {
          auto const newVal = std::clamp (
              _engine.getChannelPot1 (channel) + increment * 0.02f, 0.0f, 1.0f);
          _engine.setChannelPot1 (channel, newVal);
          _filterDisplay->setSweep (static_cast<int> (channel), newVal);
        }
      else
        {
          auto const newVal = std::clamp (
              _engine.getChannelPot2 (channel) + increment * 0.02f, 0.0f, 1.0f);
          _engine.setChannelPot2 (channel, newVal);
          _filterDisplay->setQ (static_cast<int> (channel), newVal);
        }
      break;

    case ClipSettingsComponent::globalIndex:
      {
        // Nothing in _clipUIParams changes here: the strip holds one setting
        // that every channel shares, which is why it sits outside their
        // sections rather than inside each of them.
        auto const count = static_cast<int> (recMenuModes.size ());
        applyRecMode ((recMenuIndex (_recMode) + increment % count + count)
                      % count);
      }
      break;
    }

  updateClipSettingsDisplay ();
}

void
A3MotionUIComponent::updateClipSettingsDisplay ()
{
  auto const channel = _clipSettingsChannel;
  auto const slot = _clipSettingsSlot;
  auto const &params = _clipUIParams[channel][slot];
  auto const &pattern = _patterns[channel][slot];

  // Trajectory Shape: pictogram + name, built the same way as the (hidden)
  // PadRowDisplay rows — prefer the library's SVG icon, fall back to the
  // pattern's own tick data if it's not (yet) saved to the library.
  if (pattern)
    {
      auto const &name = pattern->getName ();
      auto const libIndex = _patternLibrary->indexForName (name);
      if (libIndex > 0)
        {
          auto const &entry = _patternLibrary->getEntry (libIndex);
          auto iconPath = svgDToPath (entry.svgPathData);
          if (!iconPath.isEmpty () || entry.hasJumpDots)
            _clipSettings->setTrajectoryIcon (
                trajectoryIconFromPath (iconPath, entry.jumpDots));
          else
            _clipSettings->setTrajectoryIcon (
                trajectoryIconFromTicks (entry.ticks));
        }
      else
        {
          _clipSettings->setTrajectoryIcon (
              trajectoryIconFromTicks (pattern->getTicks ().positions));
        }
      _clipSettings->setTrajectoryName (juce::String (name));
    }
  else
    {
      _clipSettings->setTrajectoryIcon (TrajectoryIconData{});
      _clipSettings->setTrajectoryName ("Empty");
    }

  _clipSettings->setRecMode (_recMode);
  _clipSettings->setElevationSubIndex (_clipSettingsSubIndex);
  _clipSettings->setElevationReach (pattern ? pattern->getReach () : 0.5f);
  _clipSettings->setElevationMirrorSouth (pattern
                                          && pattern->getMirrorSouth ());
  _clipSettings->setElevationClipTop (pattern ? pattern->getClipTop ()
                                              : 0.0f);
  _clipSettings->setElevationClipBottom (
      pattern ? pattern->getClipBottom () : 0.0f);
  _clipSettings->setElevationFlat (pattern && pattern->getFlat ());
  _clipSettings->setElevationFlatElevation (
      pattern ? pattern->getFlatElevation () : 0.5f);

  // Motion/Filter: all sub-controls visible in parallel, like Elevation —
  // _clipSettingsSubIndex only picks which one is highlighted, and only
  // means anything while that section is actually selected.
  //
  // Speed is passed as an already-normalized knob fraction + a formatted
  // musical label (e.g. "1/4", "2") rather than the raw speedLog2 value,
  // so ClipSettingsComponent doesn't need to know speedLog2Min/Max.
  // Inverted against the raw range: far left (frac 0) = speedLog2Max
  // ("16", slowest), far right (frac 1) = speedLog2Min ("1/128", fastest).
  auto const speedRange
      = static_cast<float> (speedLog2Max - speedLog2Min);
  auto const speedFrac
      = speedRange > 0.f
            ? (speedLog2Max - params.speedLog2) / speedRange
            : 0.f;
  auto const speedLabel
      = params.speedLog2 >= 0
            ? juce::String (static_cast<int> (std::exp2 (params.speedLog2)))
            : "1/"
                  + juce::String (
                      static_cast<int> (std::exp2 (-params.speedLog2)));
  _clipSettings->setMotionSpeed (speedFrac, speedLabel);
  // Read back off the pattern rather than from this table. The pattern is
  // where the engine looks and what the file carries, so a clip that came from
  // disk brings its own settings -- and the bar has to show those, not the
  // ones the last clip happened to leave in the table.
  if (pattern)
    {
      auto &editable = _clipUIParams[channel][slot];
      editable.direction
          = pattern->getPlayDirection () == PlayDirection::Reverse ? 1 : 0;

      switch (pattern->getEndAction ())
        {
        case EndAction::Loop: editable.endAction = 0; break;
        case EndAction::Stop: editable.endAction = 1; break;
        case EndAction::Bounce: editable.endAction = 2; break;
        case EndAction::Random: editable.endAction = 3; break;
        }
    }

  _clipSettings->setMotionDirection (_clipUIParams[channel][slot].direction);
  _clipSettings->setMotionEndAction (_clipUIParams[channel][slot].endAction);
  _clipSettings->setMotionFade (params.fadeSixteenths);

  // Worded like Speed is, because it is the same kind of number: bars as a
  // power of two, "2" for two bars, "1/4" for a quarter of one.
  _clipSettings->setRecordLength (
      params.recordLengthLog2 >= 0
          ? juce::String (static_cast<int> (std::exp2 (params.recordLengthLog2)))
          : "1/"
                + juce::String (static_cast<int> (
                    std::exp2 (-params.recordLengthLog2))));
  // How far the running take has got, as a line under the tick indicator in
  // this channel's own colour — over the sphere, where the eye already is.
  {
    auto const &pattern = _patterns[channel][slot];
    auto const recording = _engine.getRecordingPattern ();
    auto const isRecordingThis = recording != nullptr && recording == pattern
                                 && _engine.isRecording ();

    if (_statusBar)
      _statusBar->setRecordingProgress (
          isRecordingThis ? _engine.getRecordingProgress () : -1.f,
          _channelUIStates[channel]->colour);
  }

  _clipSettings->setTrajectorySubIndex (
      _clipSettingsMenuIndex == ClipSettingsComponent::trajectoryIndex
          ? _clipSettingsSubIndex
          : 0);
  _clipSettings->setMotionSubIndex (
      _clipSettingsMenuIndex == ClipSettingsComponent::motionIndex
          ? _clipSettingsSubIndex
          : 0);

  _clipSettings->setFilterSweep (_engine.getChannelPot1 (channel));
  _clipSettings->setFilterQ (_engine.getChannelPot2 (channel));
  _clipSettings->setFilterSubIndex (
      _clipSettingsMenuIndex == ClipSettingsComponent::filterIndex
          ? _clipSettingsSubIndex
          : 0);

  _clipSettings->setSelectedParameterIndex (_clipSettingsMenuIndex);
}

void
A3MotionUIComponent::updateControlReadout (juce::String const &text)
{
  if (_clipSettings)
    _clipSettings->setLastControlReadout (text);
}

}
