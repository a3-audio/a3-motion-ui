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

#include "A3MotionUIComponent.hh"

#include <chrono>
#include <fstream>
#include <iostream>

#include <a3-motion-engine/Config.hh>
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

namespace a3
{

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

  // Global Settings (hidden by default, shown on top of the settings area)
  _globalSettings = std::make_unique<GlobalSettingsComponent> ();
  _globalSettings->setAlwaysOnTop (true);
  addChildComponent (*_globalSettings);

  // Clip Settings: permanent bottom panel, always visible.
  _clipSettings = std::make_unique<ClipSettingsComponent> ();
  _clipSettings->setAlwaysOnTop (true);
  addChildComponent (*_clipSettings);
  _clipSettings->setVisible (true);
  selectClip (0, 0); // sensible default before any button has been pressed

  // Restore Clockmode/Pot Size/Font Size from the last session, if any.
  {
    // loadSettings() deliberately returns raw parsed values; clamping stays
    // here because it guards indexing into potSizeScales/fontSizeScales —
    // same clamp the old loadPersistedSettings() applied. Dropping it would
    // make a hand-edited/corrupt ui_state.json an out-of-bounds read.
    auto constexpr numPotSizes
        = static_cast<int> (sizeof (potSizeScales) / sizeof (potSizeScales[0]));
    auto constexpr numFontSizes = static_cast<int> (
        numFontScales);

    auto const settings = loadSettings (getPersistedSettingsFile ());
    applyClockMode (settings.clockMode);
    applyPotSize (std::clamp (settings.potSizeIndex, 0, numPotSizes - 1));
    applyFontSize (std::clamp (settings.fontSizeIndex, 0, numFontSizes - 1));
  }

  // Start directory monitor: check for new/changed SVG files every 2 seconds
  startTimer (2000);

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

  // Read optional per-channel colours from config
  // Channel colours are part of the look, so they live in the skin.
  auto const skinVar = loadActiveSkinVar (
      juce::File::getCurrentWorkingDirectory ().getChildFile (
          "config/config.json"),
      userConfig);
  auto const &channelsCfg = skinVar["channels"];

  for (auto channel = 0u; channel < numChannels; ++channel)
    {
      auto uiState = std::make_unique<ChannelUIState> ();

      if (channelsCfg.isArray ()
          && static_cast<int> (channel) < channelsCfg.size ())
        {
          auto const &chCfg = channelsCfg[static_cast<int> (channel)];
          int r = chCfg.hasProperty ("r") ? static_cast<int> (chCfg["r"]) : 128;
          int g = chCfg.hasProperty ("g") ? static_cast<int> (chCfg["g"]) : 128;
          int b = chCfg.hasProperty ("b") ? static_cast<int> (chCfg["b"]) : 128;
          uiState->colour = juce::Colour (static_cast<juce::uint8> (r),
                                           static_cast<juce::uint8> (g),
                                           static_cast<juce::uint8> (b));
        }
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
A3MotionUIComponent::getLengthBeats (index_t channel, index_t slot) const
{
  auto const lengthBars
      = std::exp2 (_clipUIParams[channel][slot].speedLog2);
  auto const lengthBeats
      = lengthBars * _engine.getBeatsPerBar ();
  return static_cast<float> (lengthBeats);
}

void
A3MotionUIComponent::createMainUI ()
{
  _statusBar = std::make_unique<StatusBar> (_valueBPM);
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
          _ioAdapter->getPadLED (channel, pad)
              = juce::VariantConverter<juce::Colour>::toVar (
                  juce::Colours::black);
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
  auto constexpr statusBarHeight = StatusBar::getMinimumHeight ();
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
  auto const clipSettingsHeight = bounds.getHeight () / 4;
  auto boundsClipSettings = bounds.removeFromBottom (clipSettingsHeight);
  if (_clipSettings)
    _clipSettings->setBounds (boundsClipSettings);

  _motionComponent->setBounds (bounds);

  // Global Settings (Clockmode/Elevation Map) shares the same safe zone as
  // the Clip Settings panel — it's real screen space carved out of
  // MotionComponent's bounds above, so ordinary JUCE z-order/toFront() is
  // enough to show it on top while open (no GL composite-order issue here).
  if (_globalSettings)
    _globalSettings->setBounds (boundsClipSettings);
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
          if (_globalSettingsOpen)
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
              if (_globalSettingsOpen && channel == 3u)
                {
                  if (increment != 0)
                    {
                      if (_globalSettingsValueFieldSelected)
                        _globalSettings->navigateValue (increment > 0 ? 1 : -1);
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
                  // The single ch3 encoder also drives Global Settings
                  // while it's open: first press arms the currently
                  // browsed option's value field, second press confirms
                  // and applies it (menu stays open).
                  if (_globalSettingsOpen && channel == 3u)
                    {
                      if (!_globalSettingsValueFieldSelected)
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

      // Always create a fresh Pattern for recording (user pattern)
      pattern = std::make_shared<Pattern> ();
      pattern->setChannel (channel);

      auto recordLength = Measure{ 0, static_cast<int> (
          std::max (1.f, getLengthBeats (channel, slot))), 0 };
      recordLength.consolidate (_engine.getBeatsPerBar ());

      // Store the recording length in the pattern so it can be updated if encoder changes
      pattern->setPlaybackLength (recordLength);

      _engine.recordPattern (pattern, TempoClock::nextDownBeat (_now),
                             recordLength);
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
            auto playbackLength = Measure{ 0, static_cast<int> (
                std::max (1.f, getLengthBeats (channel, slot))), 0 };
            pattern->setPlaybackLength (playbackLength);
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
            auto playbackLength = Measure{ 0, static_cast<int> (
                std::max (1.f, getLengthBeats (channel, slot))), 0 };
            pattern->setPlaybackLength (playbackLength);
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
        _channelStrips[channel]->setTextColour (juce::Colours::red);
        break;
      }
    case Status::Stopped:
      {
        _motionComponent->unsetPreviewPattern (messagePatternStatus.pattern);
        _channelStrips[channel]->setTextColour (juce::Colours::white);

        // If this was a recording that just finished, save as user pattern.
        // We use wasRecording() because the status chain is:
        //   Recording → ScheduledForIdle → Idle
        // so getLastStatus() returns ScheduledForIdle, not Recording.
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
      _motionComponent->setBackgroundColour (
          juce::Colours::black.withAlpha (0.f));
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
              = isPlayingOnPlayPause ? juce::Colours::limegreen : channelColour;

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
A3MotionUIComponent::registerPatternDisplayData (
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
      _motionComponent->setPatternDisplayData (pattern, displayPath,
                                               entry.jumpDots);
    }
  else
    {
      // No library entry — register with empty display data
      // (will use raw tick data fallback in drawPlayingTrajectory)
      _motionComponent->setPatternDisplayData (pattern);
    }
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

      // Re-load from library so the pattern gets proper SVG display data
      auto reloaded = _patternLibrary->loadPattern (newIndex);
      if (reloaded)
        {
          reloaded->setChannel (channel);
          _patterns[channel][slot] = reloaded;
          registerPatternDisplayData (reloaded);
        }

      // Update fingerprint so the timer doesn't re-trigger for this save
      _lastLibraryFingerprint = _patternLibrary->getDirectoryFingerprint ();

      // Refresh the pad cell display to show the new recording
      updatePadRowLabel (channel, slot);
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

  // Build or rebuild options with current mode as each option's active value
  std::vector<GlobalSettingsComponent::Option> options{
    { "Clockmode",
      { { "INT" },
        { "EXT" },
        { "PIO" } },
      _clockMode },
    { "Pot Size",
      { { potSizeLabels[0] },
        { potSizeLabels[1] },
        { potSizeLabels[2] },
        { potSizeLabels[3] },
        { potSizeLabels[4] } },
      _potSizeIndex },
    { "Font Size",
      { { fontSizeLabels[0] },
        { fontSizeLabels[1] },
        { fontSizeLabels[2] },
        { fontSizeLabels[3] },
        { fontSizeLabels[4] } },
      _fontSizeIndex },
  };
  _globalSettings->setOptions (std::move (options));
  _globalSettings->setOptionIndex (_globalSettingsOptionIndex);
  _globalSettings->setValueFieldSelected (false);

  // Reuse the Clip Settings panel's safe zone (real screen space carved out
  // of MotionComponent's bounds in resized(), not overlapping its OpenGL
  // context) so the menu has room to show its option rows properly.
  if (_clipSettings)
    _globalSettings->setBounds (_clipSettings->getBounds ());

  _globalSettings->setVisible (true);
  _globalSettings->toFront (true);

  // Pausing here was a concession to the RPi4's GPU. The rig runs on an Intel
  // NUC now and the sphere is meant to carry on behind the menu, but the option
  // stays for a machine that needs it again.
  if (_motionComponent && _pauseRenderingInMenu)
    _motionComponent->setRenderingPaused (true);
}

void
A3MotionUIComponent::closeGlobalSettings ()
{
  if (!_globalSettingsOpen)
    return;

  _globalSettingsOpen = false;
  _globalSettingsValueFieldSelected = false;
  _globalSettings->setVisible (false);
  _globalSettings->setValueFieldSelected (false);
  if (_motionComponent)
    _motionComponent->setRenderingPaused (false);
}

void
A3MotionUIComponent::confirmGlobalSettingsOption ()
{
  if (!_globalSettingsOpen || !_globalSettingsValueFieldSelected)
    return;

  int const chosen = _globalSettings->getSelectedValueIndex ();

  if (_globalSettingsOptionIndex == 0)
    applyClockMode (chosen);
  else if (_globalSettingsOptionIndex == 1)
    applyPotSize (chosen);
  else
    applyFontSize (chosen);

  _globalSettings->setActiveValueIndex (_globalSettingsOptionIndex, chosen);

  _globalSettingsValueFieldSelected = false;
  _globalSettings->setValueFieldSelected (false);
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
               AppSettings{ _clockMode, _potSizeIndex, _fontSizeIndex });
}

void
A3MotionUIComponent::applyPotSize (int index)
{
  if (index == _potSizeIndex)
    return;

  _potSizeIndex = index;
  if (_clipSettings)
    _clipSettings->setPotSizeScale (potSizeScales[index]);

  saveSettings (getPersistedSettingsFile (),
               AppSettings{ _clockMode, _potSizeIndex, _fontSizeIndex });
}

void
A3MotionUIComponent::applyFontSize (int index)
{
  // No early return, and no single receiver. The old version did both, and
  // between them they were the whole bug: a saved index equal to the startup
  // default jumped straight out, and the one component it did reach was the
  // only thing that ever grew. The factor now sits at the theme, where every
  // component reads it, so the work here is a float and a repaint.
  _fontSizeIndex = juce::jlimit (0, numFontScales - 1, index);
  setFontScale (fontScaleForIndex (_fontSizeIndex));

  if (auto *root = getTopLevelComponent ())
    root->repaint ();

  saveSettings (getPersistedSettingsFile (),
               AppSettings{ _clockMode, _potSizeIndex, _fontSizeIndex });
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
  _clipSettingsMenuIndex
      = (_clipSettingsMenuIndex + increment % numMenuItems + numMenuItems)
        % numMenuItems;
  _clipSettingsSubIndex = 0;
  updateClipSettingsDisplay ();
}

int
A3MotionUIComponent::numSubElementsForSection (int menuIndex) const
{
  if (menuIndex == ClipSettingsComponent::elevationIndex)
    return 6;
  if (menuIndex == ClipSettingsComponent::motionIndex)
    return 3;
  if (menuIndex == ClipSettingsComponent::filterIndex)
    return 2;
  return 1;
}

void
A3MotionUIComponent::handleClipSettingsSubElementCycle (index_t channel)
{
  if (channel != _clipSettingsChannel)
    return;

  auto const numSub = numSubElementsForSection (_clipSettingsMenuIndex);
  _clipSettingsSubIndex = (_clipSettingsSubIndex + 1) % numSub;
  updateClipSettingsDisplay ();
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
    case 0: // Trajectory Shape — cycle through the pattern library
      {
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
                auto playbackLength = Measure{ 0, static_cast<int> (
                    std::max (1.f, getLengthBeats (channel, slot))), 0 };
                pattern->setPlaybackLength (playbackLength);
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
                auto const lengthBeats = static_cast<int> (
                    std::max (1.f, getLengthBeats (channel, slot)));
                auto const playbackLength
                    = Measure{ 0, lengthBeats, 0 }.consolidate (
                        _engine.getBeatsPerBar ());
                pattern->setPlaybackLength (playbackLength);
              }
            break;
          }
        case 1:
          params.direction = (params.direction + increment % 3 + 3) % 3;
          break;
        default:
          params.endAction = (params.endAction + increment % 3 + 3) % 3;
          break;
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
  _clipSettings->setMotionDirection (params.direction);
  _clipSettings->setMotionEndAction (params.endAction);
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
