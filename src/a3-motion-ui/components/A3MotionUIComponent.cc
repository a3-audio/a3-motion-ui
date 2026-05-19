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
#include <a3-motion-engine/elevation/HeightMap.hh>
#include <a3-motion-engine/elevation/HeightMapFlat.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <a3-motion-ui/Config.hh>
#include <a3-motion-ui/Helpers.hh>
#include <a3-motion-ui/components/ChannelHeader.hh>
#include <a3-motion-ui/components/ChannelStrip.hh>
#include <a3-motion-ui/components/ChannelUIState.hh>
#include <a3-motion-ui/components/FilterDisplay.hh>
#include <a3-motion-ui/components/LayoutHints.hh>
#include <a3-motion-ui/components/LoopLengthDisplay.hh>
#include <a3-motion-ui/components/ElevationDisplay.hh>
#include <a3-motion-ui/components/MotionComponent.hh>
#include <a3-motion-ui/components/PadRowDisplay.hh>
#include <a3-motion-ui/components/OverlayMenuComponent.hh>
#include <a3-motion-ui/components/StatusBar.hh>

#include <a3-motion-ui/tests/TempoEstimatorTest.hh>

#include <a3-motion-ui/io/InputOutputAdapter.hh>
#include <a3-motion-ui/io/LEDColours.hh>
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

  // Initialize encoder state — start on LoopLength row (top)
  _encoderLevel.fill (EncoderLevel::RowSelect);
  _encoderSelectedRow.fill (loopLengthRowIndex);

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

  // Overlay menu (hidden by default, covers full window)
  _overlayMenu = std::make_unique<OverlayMenuComponent> ();
  addChildComponent (*_overlayMenu);

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
  auto const &channelsCfg = userConfig["channels"];

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

  _lengthsBarLog2 = std::vector<int> (numChannels, 0);
  _previewHeldPad = std::vector<int> (numChannels, -1);
  _padPressTime = std::vector<juce::int64> (numChannels, 0);
  _padPatternChanged = std::vector<bool> (numChannels, false);
}

void
A3MotionUIComponent::handleLengthIncrement (index_t channel, int increment)
{
  jassert (increment == -1 || increment == 1);
  if (!_engine.isRecording ()
      || _engine.getRecordingPattern ()->getChannel () != channel)
    {
      if (increment == 1)
        {
          ++_lengthsBarLog2[channel];
          _lengthsBarLog2[channel] = std::clamp (
              _lengthsBarLog2[channel], lengthBarMinLog2, lengthBarMaxLog2);
        }
      else if (increment == -1)
        {
          --_lengthsBarLog2[channel];
          _lengthsBarLog2[channel] = std::clamp (
              _lengthsBarLog2[channel], lengthBarMinLog2, lengthBarMaxLog2);
        }

      auto const lengthBars = std::exp2 (_lengthsBarLog2[channel]);
      if (_lengthsBarLog2[channel] >= 0)
        {
          _channelStrips[channel]->setTextBarsLabel (
              juce::String (lengthBars));
        }
      else
        {
          _channelStrips[channel]->setTextBarsLabel (
              "1/" + juce::String (int (1.f / lengthBars)));
        }

      // Update loop length display
      _loopLengthDisplay->setLoopLengthBeats (static_cast<int> (channel),
                                              getLengthBeats (channel));

      auto playingPattern = _engine.getPlayingPattern (channel);
      if (playingPattern)
        {
          auto const lengthBeats = static_cast<int> (
              std::max (1.f, getLengthBeats (channel)));
          auto playbackLength
              = Measure{ 0, lengthBeats, 0 }.consolidate (
                  _engine.getTempoClock ().getBeatsPerBar ());
#ifdef DEBUG
          juce::Logger::writeToLog ("setting playback length: "
                                    + toString (playbackLength));
#endif
          playingPattern->setPlaybackLength (playbackLength);
        }
      
      // Also update recording pattern length if it's in this channel
      auto recordingPattern = _engine.getRecordingPattern ();
      if (recordingPattern && recordingPattern->getChannel () == channel)
        {
          auto const lengthBeats = static_cast<int> (
              std::max (1.f, getLengthBeats (channel)));
          auto recordingLength
              = Measure{ 0, lengthBeats, 0 }.consolidate (
                  _engine.getTempoClock ().getBeatsPerBar ());
#ifdef DEBUG
          juce::Logger::writeToLog ("updating recording pattern length: "
                                    + toString (recordingLength));
#endif
          recordingPattern->setPlaybackLength (recordingLength);
        }

      // Also update scheduled recording pattern length if it's in this channel
      auto scheduledRecordingPattern = _engine.getScheduledForRecordingPattern ();
      if (scheduledRecordingPattern && scheduledRecordingPattern->getChannel () == channel)
        {
          auto const lengthBeats = static_cast<int> (
              std::max (1.f, getLengthBeats (channel)));
          auto recordingLength
              = Measure{ 0, lengthBeats, 0 }.consolidate (
                  _engine.getTempoClock ().getBeatsPerBar ());
#ifdef DEBUG
          juce::Logger::writeToLog ("updating scheduled recording pattern length: "
                                    + toString (recordingLength));
#endif
          scheduledRecordingPattern->setPlaybackLength (recordingLength);
        }
    }
}

float
A3MotionUIComponent::getLengthBeats (index_t channel) const
{
  auto const lengthBars = std::exp2 (_lengthsBarLog2[channel]);
  auto const lengthBeats
      = lengthBars * _engine.getTempoClock ().getBeatsPerBar ();
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

  _filterDisplay = std::make_unique<FilterDisplay> ();
  addChildComponent (*_filterDisplay);
  _filterDisplay->setVisible (true);
  for (auto ch = 0u; ch < _channelUIStates.size () && ch < FilterDisplay::numChannels; ++ch)
    _filterDisplay->setChannelColour (static_cast<int> (ch), _channelUIStates[ch]->colour);

  _loopLengthDisplay = std::make_unique<LoopLengthDisplay> ();
  addChildComponent (*_loopLengthDisplay);
  _loopLengthDisplay->setVisible (true);
  _loopLengthDisplay->setReferenceBeats (
      _engine.getTempoClock ().getBeatsPerBar ());
  for (auto ch = 0u; ch < _channelUIStates.size () && ch < LoopLengthDisplay::numChannels; ++ch)
    {
      _loopLengthDisplay->setChannelColour (static_cast<int> (ch), _channelUIStates[ch]->colour);
      _loopLengthDisplay->setLoopLengthBeats (static_cast<int> (ch), getLengthBeats (ch));
    }

  _elevationDisplay = std::make_unique<ElevationDisplay> ();
  addChildComponent (*_elevationDisplay);
  _elevationDisplay->setVisible (true);
  for (auto ch = 0u; ch < _channelUIStates.size () && ch < ElevationDisplay::numChannels; ++ch)
    {
      _elevationDisplay->setChannelColour (static_cast<int> (ch), _channelUIStates[ch]->colour);
      _elevationDisplay->setCoverage (static_cast<int> (ch),
                                      _engine.getChannelCoverage (ch));
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
    }
  // Pot encoder press on channel 3 (upper-right): used for menu confirm
  _ioAdapter->getEncoderPress (3, 1).addListener (this);

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

  auto numPatternsPerChannel = runsOnHardware ()
                                   ? numPages * _ioAdapter->getNumPadsPerChannel ()
                                   : numPages * numPadRows;
  for (auto &channelPatterns : _patterns)
    channelPatterns.resize (numPatternsPerChannel);

  // Load patterns from the library into the first page (4 rows).
  // Each row gets one pattern per channel; channels share the same
  // library slot but each gets its own Pattern instance.
  auto const numLibEntries = _patternLibrary->getNumEntries (); // includes Empty at 0
  for (auto channel = 0u; channel < numChannels; ++channel)
    {
      for (auto row = 0u; row < numPadRows; ++row)
        {
          auto const libIndex = static_cast<int> (row) + 1; // 1-based
          if (libIndex < numLibEntries)
            {
              auto p = _patternLibrary->loadPattern (libIndex);
              if (p)
                {
                  p->setChannel (channel);
                  _patterns[channel][row] = std::move (p);
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

  // Loop length display below status bar
  auto constexpr loopLengthDisplayHeight = LoopLengthDisplay::getMinimumHeight ();
  auto boundsLoopLength = bounds.removeFromTop (loopLengthDisplayHeight);
  _loopLengthDisplay->setBounds (boundsLoopLength);

  // Elevation display below loop length
  auto constexpr elevationDisplayHeight = ElevationDisplay::getMinimumHeight ();
  auto boundsElevation = bounds.removeFromTop (elevationDisplayHeight);
  _elevationDisplay->setBounds (boundsElevation);

  // Pad row displays below elevation display
  for (auto &padRow : _padRowDisplays)
    {
      auto constexpr padRowHeight = PadRowDisplay::getMinimumHeight ();
      auto boundsPadRow = bounds.removeFromTop (padRowHeight);
      padRow->setBounds (boundsPadRow);
    }

  // Filter display at the bottom
  auto constexpr filterDisplayHeight = FilterDisplay::getMinimumHeight ();
  auto boundsFilter = bounds.removeFromBottom (filterDisplayHeight);
  _filterDisplay->setBounds (boundsFilter);

  // Hide channel strips - no longer needed after removing width/order displays
  for (auto &strip : _channelStrips)
    strip->setVisible (false);

  _motionComponent->setBounds (bounds);

  // Overlay covers the whole component
  if (_overlayMenu)
    _overlayMenu->setBounds (getLocalBounds ());
}

float
A3MotionUIComponent::getMinimumWidth () const
{
  return _channelStrips.size () * LayoutHints::Channels::widthMin;
}

float
A3MotionUIComponent::getMinimumHeight () const
{
  // auto minimumHeight = ChannelStrip::getMinimumHeight ()
  //                      + LayoutHints::MotionComponent::heightMin;
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
      // clock mode is changed via the overlay menu.
      (void) value;
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::Menu)))
    {
      bool const pressed = static_cast<bool> (value.getValue ());
      if (pressed)
        openMenu ();
      else
        closeMenu (false); // releasing the chord → cancel without applying
    }
  else if (runsOnHardware () &&
           value.refersToSameSourceAs (_ioAdapter->getEncoderPress (3, 1)))
    {
      // Pot encoder ch3 press: confirm menu selection
      bool const pressed = static_cast<bool> (value.getValue ());
      if (pressed && _menuOpen)
        closeMenu (true);
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::Record)))
    {
      if (value.getValue ())
        {
          // Button pressed - track for long press detection
          _recordButtonPressTime = juce::Time::currentTimeMillis ();
          _recordButtonLongPress = false;
          _ioAdapter->getButtonLED (Button::Record) = true;
        }
      else
        {
          // Button released
          _ioAdapter->getButtonLED (Button::Record) = false;
          // Long press handling is done when pad is pressed
        }
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::Tap)))
    {
      if (value.getValue ())
        {
          // Button pressed - send /tap OSC immediately via DIRECT sender
          // Bypasses async queue for zero latency - time-critical!
          _ioAdapter->getButtonLED (Button::Tap) = true;

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
          auto const result = _engine.getTempoClock ().tap (tapTime);

          if (result == TempoClock::TapResult::TempoAvailable)
            {
              auto const bpm = _engine.getTempoClock ().getTempoBPM ();
              std::cout << "[TAP] BPM=" << bpm << std::endl;
              _valueBPM = bpm;
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
              auto const increment = static_cast<int> (
                  _ioAdapter->getEncoderIncrement (channel).getValue ());
              handleEncoderIncrement (channel, increment);
            }
          else if (value.refersToSameSourceAs (
                       _ioAdapter->getEncoderPress (channel)))
            {
              if (value.getValue ())
                handleEncoderPress (channel);
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
              if (channel == 3u && _menuOpen)
                {
                  // Use pot-encoder rotation on ch3 to navigate the overlay menu
                  float delta = pot1Normalized - _menuNavLastPot;
                  if (std::abs (delta) > 0.001f)
                    {
                      _overlayMenu->navigate (delta > 0.f ? 1 : -1);
                      _menuNavLastPot = pot1Normalized;
                    }
                }
              else
                {
                  _engine.setChannelPot1 (channel, pot1Normalized);
                  _filterDisplay->setSweep (static_cast<int> (channel), pot1Normalized);
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
              _engine.setChannelPot2 (channel, pot2Normalized);
              _filterDisplay->setQ (static_cast<int> (channel), pot2Normalized);
              return;
            }

          for (auto pad = 0u; pad < _ioAdapter->getNumPadsPerChannel (); ++pad)
            {
              if (value.refersToSameSourceAs (
                      _ioAdapter->getPad (channel, pad)))
                {
                  if (value.getValue ())
                    {
                      handlePadPress (channel, pad);
                    }
                  else
                    {
                      // Pad released
                      if (_previewHeldPad[channel] == static_cast<int> (pad))
                        {
                          // Disable preview → OSC fires from current position.
                          // Pattern keeps playing (both short and long press).
                          // If user changed pattern via encoder and then
                          // exits with encoder-press, that path handles
                          // save & stop separately.
                          _engine.setPreviewMode (channel, false);
                          _previewHeldPad[channel] = -1;
                          _padPatternChanged[channel] = false;

                          if (_patterns[channel][pad])
                            {
                              _motionComponent->unsetPreviewPattern (
                                  _patterns[channel][pad]);
                            }
                        }
                      else if (_patterns[channel][pad])
                        {
                          _motionComponent->unsetPreviewPattern (
                              _patterns[channel][pad]);
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
  if (isButtonPressed (Button::Record))
    {
      // Mark as long press since we're using Record for recording
      _recordButtonLongPress = true;

      // Stop any existing pattern at this slot
      if (_patterns[channel][pad])
        {
          auto status = _patterns[channel][pad]->getStatus ();
          if (status == Pattern::Status::Playing
              || status == Pattern::Status::Recording)
            {
              _engine.stopPattern (_patterns[channel][pad],
                                   TempoClock::nextDownBeat (_now));
            }
          _motionComponent->unsetPreviewPattern (_patterns[channel][pad]);
        }

      // Always create a fresh Pattern for recording (user pattern)
      _patterns[channel][pad] = std::make_shared<Pattern> ();
      _patterns[channel][pad]->setChannel (channel);

      auto recordLength = Measure{ 0, static_cast<int> (
          std::max (1.f, getLengthBeats (channel))), 0 };
      recordLength.consolidate (_engine.getTempoClock ().getBeatsPerBar ());
#ifdef DEBUG
      juce::Logger::writeToLog ("recording with length: "
                                + toString (recordLength));
#endif
      
      // Store the recording length in the pattern so it can be updated if encoder changes
      _patterns[channel][pad]->setPlaybackLength (recordLength);
      
      _engine.recordPattern (_patterns[channel][pad],
                             TempoClock::nextDownBeat (_now), recordLength);
    }
  else if (_patterns[channel][pad])
    {
      auto const status = _patterns[channel][pad]->getStatus ();
      switch (status)
        {
        case Pattern::Status::Empty:
          {
            break;
          }
        case Pattern::Status::Idle:
          {
            auto playbackLength = Measure{ 0, static_cast<int> (
                std::max (1.f, getLengthBeats (channel))), 0 };
            _patterns[channel][pad]->setPlaybackLength (playbackLength);

            // Start playback in preview mode (no OSC output yet).
            // On pad release the hold time decides:
            //   short press → preview off, pattern keeps playing (fire)
            //   long press  → preview off, pattern stops (preview only)
            _padPressTime[channel] = juce::Time::currentTimeMillis ();
            _engine.setPreviewMode (channel, true);
            _previewHeldPad[channel] = static_cast<int> (pad);
            _engine.playPattern (_patterns[channel][pad], _now);
            setPreviewWithDisplayData (_patterns[channel][pad]);
            break;
          }
        case Pattern::Status::Playing:
        case Pattern::Status::Recording:
          {
            _engine.stopPattern (_patterns[channel][pad],
                                 TempoClock::nextDownBeat (_now));
            break;
          }
        case Pattern::Status::ScheduledForPlaying:
          {
            // needs more thought. requires us to purge previously
            // enqueued start/stop messages to be implemented
            // consistently. std::priority_queue doesn't support that so
            // we have to find an alternative such as implementing our
            // own max heap.
            // _engine.playPattern (_patterns[channel][pad], _now);
            break;
          }
        case Pattern::Status::ScheduledForRecording:
          {
            // _engine.recordPattern (_patterns[channel][pad], _now,
            //                        recordLength);
          }
        case Pattern::Status::ScheduledForIdle:
          {
            // _engine.stopPattern (_patterns[channel][pad], _now);
            break;
          }
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
            // Find which pad slot this pattern belongs to
            bool found = false;
            for (size_t pad = 0; pad < _patterns[channel].size (); ++pad)
              {
                if (_patterns[channel][pad] == messagePatternStatus.pattern)
                  {
                    std::cout << "  -> found in pad slot " << pad << std::endl;
                    saveRecordedPattern (messagePatternStatus.pattern,
                                         channel,
                                         static_cast<index_t> (pad));
                    found = true;
                    break;
                  }
              }
            if (!found)
              std::cout << "  -> pattern NOT found in any pad slot!" << std::endl;
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
      beatClockMsg.addInt32 (static_cast<int> (std::round (_engine.getTempoClock ().getTempoBPM ())));
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
      auto const beatsPerBar = _engine.getTempoClock ().getBeatsPerBar ();
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
      for (auto pad = 0u; pad < _ioAdapter->getNumPadsPerChannel (); ++pad)
        {
          auto colour = LEDColours::empty;
          if (_patterns[channel][pad])
            {
              auto const status = _patterns[channel][pad]->getStatus ();
              auto const statusLast
                  = _patterns[channel][pad]->getLastStatus ();
              // juce::Logger::writeToLog (
              //     juce::String ("(") + juce::String (channel)
              //     + juce::String (".") + juce::String (pad)
              //     + juce::String (") : ")
              //     + juce::String (static_cast<int> (status)));

              switch (status)
                {
                case Pattern::Status::Empty:
                  {
                    colour = LEDColours::empty;
                    break;
                  }
                case Pattern::Status::Idle:
                  {
                    colour = LEDColours::idle;
                    break;
                  }
                case Pattern::Status::ScheduledForRecording:
                  {
                    if (step % 2 == 0)
                      colour = LEDColours::recording;
                    else
                      colour = LEDColours::scheduledForRecording;
                    break;
                  }
                case Pattern::Status::Recording:
                  {
                    colour = LEDColours::recording;
                    break;
                  }
                case Pattern::Status::ScheduledForPlaying:
                  {
                    if (step % 2 == 0)
                      colour = LEDColours::playing;
                    else
                      colour = LEDColours::scheduledForPlaying;
                    break;
                  }
                case Pattern::Status::Playing:
                  {
                    colour = LEDColours::playing;
                    break;
                  }
                case Pattern::Status::ScheduledForIdle:
                  {
                    jassert (statusLast
                                 != Pattern::Status::ScheduledForRecording
                             && statusLast != Pattern::Status::Idle);
                    colour = scheduledForIdleLEDColour (step, statusLast);
                  }
                }
              _ioAdapter->getPadLED (channel, pad)
                  = juce::VariantConverter<juce::Colour>::toVar (colour);
            }
        }
    }
}

juce::Colour
A3MotionUIComponent::scheduledForIdleLEDColour (int step,
                                                Pattern::Status statusLast)
{
  // one-shot recording: don't blink when scheduled for idle
  if (_engine.getRecordingMode () == MotionEngine::RecordingMode::OneShot
      && statusLast == Pattern::Status::Recording)
    {
      return LEDColours::recording;
    }

  if (step % 2 == 0)
    {
      return LEDColours::scheduledForIdle;
    }
  else
    {
      if (statusLast == Pattern::Status::Playing || //
          statusLast == Pattern::Status::ScheduledForPlaying)
        {
          return LEDColours::scheduledForPlaying;
        }
      else
        {
          return LEDColours::scheduledForRecording;
        }
    }
}

void
A3MotionUIComponent::createPadRowDisplays ()
{
  for (auto row = 0u; row < numPadRows; ++row)
    {
      auto display = std::make_unique<PadRowDisplay> (static_cast<int> (row));
      for (auto ch = 0u; ch < _channelUIStates.size ()
                         && ch < PadRowDisplay::numChannels;
           ++ch)
        {
          display->setChannelColour (static_cast<int> (ch),
                                     _channelUIStates[ch]->colour);
        }
      addChildComponent (*display);
      display->setVisible (true);
      _padRowDisplays.push_back (std::move (display));
    }

  // Set initial trajectory icons now that all displays exist
  for (auto row = 0u; row < numPadRows; ++row)
    {
      for (auto ch = 0u; ch < _engine.getNumChannels (); ++ch)
        {
          if (row < _patterns[ch].size () && _patterns[ch][row])
            {
              updatePadRowLabel (ch, row);
              registerPatternDisplayData (_patterns[ch][row]);
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
A3MotionUIComponent::handleEncoderIncrement (index_t channel, int increment)
{
  if (_encoderLevel[channel] == EncoderLevel::RowSelect)
    {
      // Navigate between LoopLength (-2), Elevation (-1), pad rows (0..3)
      auto oldRow = _encoderSelectedRow[channel];
      auto newRow = oldRow + increment;
      newRow = std::clamp (newRow, loopLengthRowIndex,
                           static_cast<int> (numPadRows) - 1);

      if (newRow != oldRow)
        {
          // Remove old highlight
          if (oldRow == loopLengthRowIndex)
            _loopLengthDisplay->setRowHighlighted (
                static_cast<int> (channel), false);
          else if (oldRow == elevationRowIndex)
            _elevationDisplay->setRowHighlighted (
                static_cast<int> (channel), false);
          else if (static_cast<size_t> (oldRow) < _padRowDisplays.size ())
            _padRowDisplays[static_cast<size_t> (oldRow)]
                ->setRowHighlighted (static_cast<int> (channel), false);

          // Remove trajectory preview from old row
          if (oldRow == elevationRowIndex)
            {
              auto playing = _engine.getPlayingPattern (channel);
              if (playing)
                _motionComponent->unsetPreviewPattern (playing);
            }
          else if (oldRow >= 0
                   && static_cast<size_t> (oldRow) < _padRowDisplays.size ())
            {
              auto padIdx = static_cast<index_t> (oldRow);
              if (_patterns[channel][padIdx])
                _motionComponent->unsetPreviewPattern (
                    _patterns[channel][padIdx]);
            }

          // Set new highlight
          if (newRow == loopLengthRowIndex)
            _loopLengthDisplay->setRowHighlighted (
                static_cast<int> (channel), true);
          else if (newRow == elevationRowIndex)
            _elevationDisplay->setRowHighlighted (
                static_cast<int> (channel), true);
          else if (static_cast<size_t> (newRow) < _padRowDisplays.size ())
            _padRowDisplays[static_cast<size_t> (newRow)]
                ->setRowHighlighted (static_cast<int> (channel), true);

          // Show trajectory preview for new row
          if (newRow == elevationRowIndex)
            {
              auto playing = _engine.getPlayingPattern (channel);
              if (playing)
                setPreviewWithDisplayData (playing);
            }
          else if (newRow >= 0
                   && static_cast<size_t> (newRow) < _padRowDisplays.size ())
            {
              auto padIdx = static_cast<index_t> (newRow);
              if (_patterns[channel][padIdx])
                setPreviewWithDisplayData (_patterns[channel][padIdx]);
            }

          _encoderSelectedRow[channel] = newRow;
        }
    }
  else if (_encoderLevel[channel] == EncoderLevel::OptionEdit)
    {
      auto const row = _encoderSelectedRow[channel];

      // LoopLength row: encoder adjusts loop length
      if (row == loopLengthRowIndex)
        {
          handleLengthIncrement (channel, increment);
          return;
        }

      // Elevation row: encoder adjusts elevation coverage for this channel
      if (row == elevationRowIndex)
        {
          handleElevationIncrement (channel, increment);
          return;
        }

      // Pad rows: cycle trajectory type for the selected pad
      auto const padIndex = static_cast<index_t> (row);

      // Get current trajectory index
      int currentIndex = 0;
      if (_patterns[channel][padIndex])
        currentIndex = trajectoryNameToIndex (_patterns[channel][padIndex]->getName ());

      // Cycle: 0..numLibEntries-1 where 0=empty, 1..N=patterns (system + user)
      auto const numLibEntries = _patternLibrary->getNumEntries ();
      int newIndex = currentIndex + increment;
      if (newIndex < 0)
        newIndex = numLibEntries - 1;
      else if (newIndex >= numLibEntries)
        newIndex = 0;

      // Was the old pattern playing? If so, the new one should
      // auto-play (seamless switch).  Also track preview-held state.
      bool const wasPlaying = _patterns[channel][padIndex]
          && (_patterns[channel][padIndex]->getStatus () == Pattern::Status::Playing
              || _patterns[channel][padIndex]->getStatus () == Pattern::Status::ScheduledForPlaying);
      bool const isPreviewHeld
          = _previewHeldPad[channel] == static_cast<int> (padIndex);

      // Stop & clean up the old pattern
      if (_patterns[channel][padIndex])
        {
          auto status = _patterns[channel][padIndex]->getStatus ();
          if (status == Pattern::Status::Playing
              || status == Pattern::Status::Recording)
            {
              _engine.stopPattern (_patterns[channel][padIndex], _now);
            }
          _motionComponent->unsetPreviewPattern (
              _patterns[channel][padIndex]);
          _motionComponent->removePatternDisplayData (
              _patterns[channel][padIndex]);
        }

      // Create new pattern or clear
      if (newIndex == 0)
        {
          _patterns[channel][padIndex] = nullptr;
          if (isPreviewHeld)
            {
              _engine.setPreviewMode (channel, false);
              _previewHeldPad[channel] = -1;
            }
        }
      else
        {
          _patterns[channel][padIndex]
              = createPatternForIndex (newIndex, channel);
          registerPatternDisplayData (_patterns[channel][padIndex]);

          // If old pattern was playing (or being previewed), start new
          // pattern playing immediately so the switch is seamless.
          if (wasPlaying && _patterns[channel][padIndex])
            {
              auto playbackLength = Measure{ 0, static_cast<int> (
                  std::max (1.f, getLengthBeats (channel))), 0 };
              _patterns[channel][padIndex]->setPlaybackLength (playbackLength);
              _engine.playPattern (_patterns[channel][padIndex], _now);

              // Update preview display if pad is still held
              if (isPreviewHeld)
                setPreviewWithDisplayData (_patterns[channel][padIndex]);
            }
        }

      // Mark that the pattern was changed while pad is held
      if (_previewHeldPad[channel] == static_cast<int> (padIndex))
        _padPatternChanged[channel] = true;

      updatePadRowLabel (channel, padIndex);
      showTrajectoryPreview (channel, padIndex);
    }
}

void
A3MotionUIComponent::handleEncoderPress (index_t channel)
{
  auto const row = _encoderSelectedRow[channel];

  if (_encoderLevel[channel] == EncoderLevel::RowSelect)
    {
      // Enter edit mode
      _encoderLevel[channel] = EncoderLevel::OptionEdit;

      if (row == loopLengthRowIndex)
        {
          _loopLengthDisplay->setCellSelected (
              static_cast<int> (channel), true);
        }
      else if (row == elevationRowIndex)
        {
          _elevationDisplay->setCellSelected (
              static_cast<int> (channel), true);
          // Show trajectory preview for this channel's playing pattern
          // so the user can see how elevation coverage changes affect its path.
          auto playing = _engine.getPlayingPattern (channel);
          if (playing)
            setPreviewWithDisplayData (playing);
        }
      else if (static_cast<size_t> (row) < _padRowDisplays.size ())
        {
          _padRowDisplays[static_cast<size_t> (row)]
              ->setCellSelected (static_cast<int> (channel), true);
          showTrajectoryPreview (channel, static_cast<index_t> (row));
        }
    }
  else
    {
      // Exit edit mode
      _encoderLevel[channel] = EncoderLevel::RowSelect;

      if (row == loopLengthRowIndex)
        {
          _loopLengthDisplay->setCellSelected (
              static_cast<int> (channel), false);
        }
      else if (row == elevationRowIndex)
        {
          _elevationDisplay->setCellSelected (
              static_cast<int> (channel), false);
          // Remove trajectory preview for this channel shown during elevation editing
          auto playing = _engine.getPlayingPattern (channel);
          if (playing)
            _motionComponent->unsetPreviewPattern (playing);
        }
      else if (static_cast<size_t> (row) < _padRowDisplays.size ())
        {
          _padRowDisplays[static_cast<size_t> (row)]
              ->setCellSelected (static_cast<int> (channel), false);

          auto const padIndex = static_cast<index_t> (row);
          // If the pattern was changed via encoder while pad was held,
          // save the new pattern in the pad slot and stop playback.
          if (_padPatternChanged[channel]
              && _previewHeldPad[channel] == static_cast<int> (padIndex))
            {
              _engine.setPreviewMode (channel, false);
              _previewHeldPad[channel] = -1;
              _padPatternChanged[channel] = false;

              if (_patterns[channel][padIndex])
                {
                  _engine.stopPattern (
                      _patterns[channel][padIndex], _now);
                  _motionComponent->unsetPreviewPattern (
                      _patterns[channel][padIndex]);
                }
            }

          clearTrajectoryPreview (channel);
        }
    }
}

void
A3MotionUIComponent::handleElevationIncrement (index_t channel, int increment)
{
  // Discrete coverage steps: 5%
  static constexpr float step = 0.05f;
  auto coverage = _engine.getChannelCoverage (channel);
  coverage += static_cast<float> (increment) * step;
  coverage = std::clamp (coverage, 0.05f, 1.0f);

  _engine.setChannelCoverage (channel, coverage);
  _elevationDisplay->setCoverage (static_cast<int> (channel), coverage);

  // Show this channel's playing trajectory so the user can see how
  // the coverage change affects the pattern shape on the sphere.
  auto playing = _engine.getPlayingPattern (channel);
  if (playing)
    setPreviewWithDisplayData (playing);
}

void
A3MotionUIComponent::updatePadRowLabel (index_t channel, index_t pad)
{
  auto const row = pad; // pad index == row index for first page
  if (row >= numPadRows)
    return;

  if (row >= _padRowDisplays.size ())
    return;

  if (_patterns[channel][pad])
    {
      auto const &name = _patterns[channel][pad]->getName ();
      auto libIndex = _patternLibrary->indexForName (name);

      if (libIndex > 0)
        {
          // Use SVG path from the library entry for the icon
          auto const &entry = _patternLibrary->getEntry (libIndex);
          auto iconPath = svgDToPath (entry.svgPathData);
          if (!iconPath.isEmpty () || entry.hasJumpDots)
            {
              _padRowDisplays[row]->setIconPath (
                  static_cast<int> (channel),
                  iconPath,
                  entry.jumpDots);
            }
          else
            {
              _padRowDisplays[row]->setTickData (
                  static_cast<int> (channel), entry.ticks);
            }
          // Show pattern length in beats
          _padRowDisplays[row]->setLengthBeats (
              static_cast<int> (channel), entry.lengthBeats);
          // Category prefix: "S" for system, "U" for user
          _padRowDisplays[row]->setCategoryPrefix (
              static_cast<int> (channel),
              entry.category == PatternLibrary::Category::System ? "S" : "U");
        }
      else
        {
          // Pattern not in library (e.g. newly recorded, not yet saved)
          // Generate icon from the pattern's own tick data
          auto ticks = _patterns[channel][pad]->getTicks ();
          _padRowDisplays[row]->setTickData (static_cast<int> (channel),
                                             ticks.positions);
          // Compute beats from tick count
          auto numTicks = _patterns[channel][pad]->getNumTicks ();
          auto ticksPerBeat = TempoClock::getTicksPerBeat ();
          int beats = ticksPerBeat > 0
                          ? static_cast<int> (numTicks / ticksPerBeat)
                          : 0;
          _padRowDisplays[row]->setLengthBeats (
              static_cast<int> (channel), beats);
          _padRowDisplays[row]->setCategoryPrefix (
              static_cast<int> (channel), "U");
        }
    }
  else
    {
      // Empty: clear tick data and set Empty type
      _padRowDisplays[row]->setTickData (static_cast<int> (channel), {});
      _padRowDisplays[row]->setTrajectoryType (
          static_cast<int> (channel), PadRowDisplay::TrajectoryType::Empty);
      _padRowDisplays[row]->setLengthBeats (
          static_cast<int> (channel), 0);
      // Use the category from the library slot (row+1) for the prefix,
      // even when the current channel has no pattern loaded
      auto const slotIndex = static_cast<int> (row) + 1;
      if (slotIndex < _patternLibrary->getNumEntries ())
        {
          auto const &slotEntry = _patternLibrary->getEntry (slotIndex);
          _padRowDisplays[row]->setCategoryPrefix (
              static_cast<int> (channel),
              slotEntry.category == PatternLibrary::Category::System ? "S" : "U");
        }
      else
        {
          _padRowDisplays[row]->setCategoryPrefix (
              static_cast<int> (channel), "");
        }
    }
}

void
A3MotionUIComponent::showTrajectoryPreview (index_t channel, index_t pad)
{
  if (_encoderLevel[channel] == EncoderLevel::OptionEdit
      && _patterns[channel][pad])
    {
      setPreviewWithDisplayData (_patterns[channel][pad]);
    }
}

void
A3MotionUIComponent::clearTrajectoryPreview (index_t channel)
{
  auto const row = _encoderSelectedRow[channel];
  auto const pad = static_cast<index_t> (row);
  if (_patterns[channel][pad])
    _motionComponent->unsetPreviewPattern (_patterns[channel][pad]);
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
    std::shared_ptr<Pattern> const &pattern, index_t channel, index_t pad)
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
            << " pad=" << pad
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
          _patterns[channel][pad] = reloaded;
          registerPatternDisplayData (reloaded);
        }

      // Update fingerprint so the timer doesn't re-trigger for this save
      _lastLibraryFingerprint = _patternLibrary->getDirectoryFingerprint ();

      // Refresh the pad cell display to show the new recording
      updatePadRowLabel (channel, pad);
    }
}

void
A3MotionUIComponent::refreshAllPadRowLabels ()
{
  auto const numChannels = _engine.getNumChannels ();
  for (index_t ch = 0; ch < numChannels; ++ch)
    {
      for (index_t row = 0; row < numPadRows; ++row)
        {
          updatePadRowLabel (ch, row);
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
  auto address = message.getAddressPattern ().toString ();
  
  // Handle VU meter messages: /vu/<channel> ff <peak> <rms>
  if (address.startsWith ("/vu/"))
    {
      auto channelStr = address.substring (4);
      auto channel = channelStr.getIntValue ();
      
      if (message.size () >= 2)
        {
          float peak = message[0].getFloat32 ();
          float rms = message[1].getFloat32 ();

          // Channels 0-3: channel blob coronas
          if (channel >= 0 
              && static_cast<size_t>(channel) < _channelUIStates.size ())
            {
              _channelUIStates[static_cast<size_t>(channel)]->vuPeak = peak;
              _channelUIStates[static_cast<size_t>(channel)]->vuLevel = rms;
            }
          // Channel 4: subwoofer → sphere glow
          else if (channel == 4)
            {
              _motionComponent->setSphereGlow (peak, rms);
            }
          // Channels 5-8: speaker spotlights
          else if (channel >= 5 && channel <= 8)
            {
              _motionComponent->setSpeakerLight (channel - 5, peak, rms);
            }
        }
      return; // Don't log VU messages (too spammy)
    }
  
  // Handle external beat clock: /beat iii <beat> <bar> <bpm>
  if (address == "/beat" && message.size () >= 3)
    {
      // Accept both int and float arguments
      auto getIntArg = [] (const juce::OSCArgument &arg) -> int {
        if (arg.isInt32 ()) return arg.getInt32 ();
        if (arg.isFloat32 ()) return static_cast<int> (arg.getFloat32 ());
        return 0;
      };
      
      int beat = getIntArg (message[0]);
      int bar = getIntArg (message[1]);
      int bpm = getIntArg (message[2]);
      
      // Update StatusBar (filters by clock mode internally)
      _statusBar->setExternalBPM (static_cast<float> (bpm));
      _statusBar->setBeatClock (beat, bar);
      
      // In EXT mode: set internal clock BPM so patterns run at external tempo,
      // and notify LoopLengthDisplay of the external beat for interpolation.
      // The display will interpolate smoothly from this beat to the next,
      // using the measured time between beats.
      if (_clockMode != 0)
        {
          _engine.getTempoClock ().setTempoBPM (static_cast<float> (bpm));

          auto const beatsPerBar
              = _engine.getTempoClock ().getBeatsPerBar ();
          _loopLengthDisplay->setExternalBeat (beat, beatsPerBar);
        }
      return;
    }
  
  // Log other OSC messages (debug only — cout blocks the message thread)
#ifdef DEBUG
  std::cout << "OSC: " << address.toStdString ();
  for (auto &arg : message)
    {
      if (arg.isFloat32 ())
        std::cout << " " << arg.getFloat32 ();
      else if (arg.isInt32 ())
        std::cout << " " << arg.getInt32 ();
      else if (arg.isString ())
        std::cout << " " << arg.getString ();
    }
  std::cout << std::endl;
#endif
}

// ── Overlay menu helpers ──────────────────────────────────────────────────────

void
A3MotionUIComponent::openMenu ()
{
  if (_menuOpen)
    return;

  _menuOpen = true;

  // Build or rebuild items with current clock mode as active
  std::vector<OverlayMenuComponent::Item> items{
    { "INT", juce::Colours::white },
    { "EXT", juce::Colours::white },
    { "PIO", juce::Colours::white },
  };
  _overlayMenu->setItems (std::move (items));
  _overlayMenu->setActiveIndex (_clockMode);
  _overlayMenu->setSelectedIndex (_clockMode);

  // Snapshot current pot value for delta navigation
  if (runsOnHardware ())
    {
      _menuNavLastPot = static_cast<float> (
          _ioAdapter->getPot (3, 0).getValue ());
    }

  _overlayMenu->setVisible (true);
  _overlayMenu->toFront (false);
}

void
A3MotionUIComponent::closeMenu (bool applySelection)
{
  if (!_menuOpen)
    return;

  _menuOpen = false;
  _overlayMenu->setVisible (false);

  if (applySelection)
    applyClockMode (_overlayMenu->getSelectedIndex ());
}

void
A3MotionUIComponent::applyClockMode (int mode)
{
  if (mode == _clockMode)
    return;

  _clockMode = mode;

  if (_clockMode != 0)
    {
      if (_internalBPM == 0.f)
        _internalBPM = _engine.getTempoClock ().getTempoBPM ();
      _engine.getTempoClock ().reset ();
    }
  else
    {
      if (_internalBPM > 0.f)
        {
          _engine.getTempoClock ().setTempoBPM (_internalBPM);
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
}

}
