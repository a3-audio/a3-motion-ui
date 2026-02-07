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
#include <a3-motion-engine/PatternGenerator.hh>
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
#include <a3-motion-ui/components/MotionComponent.hh>
#include <a3-motion-ui/components/StatusBar.hh>

#include <a3-motion-ui/tests/TempoEstimatorTest.hh>

#include <a3-motion-ui/io/InputOutputAdapter.hh>
#include <a3-motion-ui/io/LEDColours.hh>
#ifdef HARDWARE_INTERFACE_V2
#include <a3-motion-ui/io/InputOutputAdapterV2.hh>
#endif

namespace a3
{

A3MotionUIComponent::A3MotionUIComponent (unsigned int const numChannels)
    : _heightMap (std::make_unique<HeightMapSphere> ()),
      _engine (numChannels, *_heightMap)
{
  setLookAndFeel (&_lookAndFeel);

  initializePatterns ();

  if (runsOnHardware ())
    {
      createHardwareInterface ();
    }

  createChannelsUI ();
  createMainUI ();

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
    }
}

A3MotionUIComponent::~A3MotionUIComponent ()
{
  _oscReceiver.removeListener (this);
  _oscReceiver.disconnect ();
  _oscSender.disconnect ();

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

      auto playingPattern = _engine.getPlayingPattern (channel);
      if (playingPattern)
        {
          auto playbackLength
              = Measure{ 0, getLengthBeats (channel), 0 }.consolidate (
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
          auto recordingLength
              = Measure{ 0, getLengthBeats (channel), 0 }.consolidate (
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
          auto recordingLength
              = Measure{ 0, getLengthBeats (channel), 0 }.consolidate (
                  _engine.getTempoClock ().getBeatsPerBar ());
#ifdef DEBUG
          juce::Logger::writeToLog ("updating scheduled recording pattern length: "
                                    + toString (recordingLength));
#endif
          scheduledRecordingPattern->setPlaybackLength (recordingLength);
        }
    }
}

int
A3MotionUIComponent::getLengthBeats (index_t channel) const
{
  auto const lengthBars = std::exp2 (_lengthsBarLog2[channel]);
  auto const lengthBeats = static_cast<int> (
      lengthBars * _engine.getTempoClock ().getBeatsPerBar ());
  return lengthBeats;
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
#else
#error hardware interface enabled but no implementation selected!
#endif
  _ioAdapter->getButton (Button::ClockMode).addListener (this);
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
    }
  _ioAdapter->startThread ();

  blankLEDs ();
#endif
}

void
A3MotionUIComponent::initializePatterns ()
{
  auto const numChannels = _ioAdapter->getNumChannels ();
  _patterns.resize (numChannels);

  auto numPatternsPerChannel = numPages * _ioAdapter->getNumPadsPerChannel ();
  for (auto &channelPatterns : _patterns)
    channelPatterns.resize (numPatternsPerChannel);

  auto constexpr lengthBeatsPreMadePatterns = 16;
  for (auto channel = 0u; channel < numChannels; ++channel)
    {
      auto constexpr radius = .8f;
      auto constexpr degrees = 360.f;
      _patterns[channel][0] = PatternGenerator::createCircle (
          lengthBeatsPreMadePatterns, radius, degrees, *_heightMap);
      _patterns[channel][0]->setChannel (channel);

      _patterns[channel][1] = PatternGenerator::createFigureOfEight (
          lengthBeatsPreMadePatterns, radius, *_heightMap);
      _patterns[channel][1]->setChannel (channel);

      _patterns[channel][2] = PatternGenerator::createCornerStep (
          lengthBeatsPreMadePatterns, radius, *_heightMap);
      _patterns[channel][2]->setChannel (channel);
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

  auto constexpr statusBarOnTop = true;
  auto constexpr statusBarHeight = StatusBar::getMinimumHeight ();
  auto boundsStatus = statusBarOnTop
                          ? bounds.removeFromTop (statusBarHeight)
                          : bounds.removeFromBottom (statusBarHeight);
  _statusBar->setBounds (boundsStatus);

  // Filter display at the bottom
  auto constexpr filterDisplayHeight = FilterDisplay::getMinimumHeight ();
  auto boundsFilter = bounds.removeFromBottom (filterDisplayHeight);
  _filterDisplay->setBounds (boundsFilter);

  // Hide channel strips - no longer needed after removing width/order displays
  for (auto &strip : _channelStrips)
    strip->setVisible (false);

  _motionComponent->setBounds (bounds);
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
      // Toggle ClockMode on button press
      if (value.getValue ())
        {
          _clockMode = !_clockMode;
          
          if (_clockMode)
            {
              // Switching to EXT: save current internal tempo and reset clock
              // phase so external beats don't conflict with running internal clock
              _internalBPM = _engine.getTempoClock ().getTempoBPM ();
              _engine.getTempoClock ().reset ();
            }
          else
            {
              // Switching to INT: restore saved internal tempo
              if (_internalBPM > 0.f)
                {
                  _engine.getTempoClock ().setTempoBPM (_internalBPM);
                  _valueBPM = static_cast<double> (_internalBPM);
                }
            }
          
          // Update LED and status bar
          _ioAdapter->getButtonLED (Button::ClockMode) = _clockMode;
          _statusBar->setClockMode (_clockMode);
          
          // Send clock mode status via OSC
          auto clockModeMsg = juce::OSCMessage ("/clockmode");
          clockModeMsg.addInt32 (_clockMode ? 1 : 0);
          _oscSender.send (clockModeMsg);
        }
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
          // Debounce: ignore taps closer than 80ms to prevent hardware bounce
          auto now = juce::Time::currentTimeMillis ();
          auto elapsed = now - _tapButtonPressTime;
          _tapButtonPressTime = now;
          _tapButtonLongPress = false;
          _ioAdapter->getButtonLED (Button::Tap) = true;

          if (elapsed < 80)
            {
              std::cout << "[TAP] DEBOUNCE (ignored, " << elapsed << "ms)" << std::endl;
            }
          else
            {
              // Send /tap via OSC on every button press
              auto tapMsg = juce::OSCMessage ("/tap");
              tapMsg.addInt32 (1);
              _oscSender.send (tapMsg);
              std::cout << "[TAP] " << (_clockMode ? "EXT" : "INT")
                        << " dt=" << elapsed << "ms" << std::endl;

              // EXT mode: also reset beat counter
              if (_clockMode)
                _engine.getTempoClock ().reset ();
            }
        }
      else
        {
          _ioAdapter->getButtonLED (Button::Tap) = false;
        }
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getTapTimeMicros ()))
    {
      if (!_clockMode)
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
              handleLengthIncrement (channel, increment);
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
              _engine.setChannelPot1 (channel, pot1Normalized);
              _filterDisplay->setSweep (static_cast<int> (channel), pot1Normalized);
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
                      if (_patterns[channel][pad])
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
      
      if (!_patterns[channel][pad])
        {
          _patterns[channel][pad] = std::make_shared<Pattern> ();
          _patterns[channel][pad]->setChannel (channel);
        }

      auto recordLength = Measure{ 0, getLengthBeats (channel), 0 };
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
  else if (isButtonPressed (Button::Tap))
    {
      // Tap held + Pad = show trajectory preview
      _tapButtonLongPress = true;
      if (_patterns[channel][pad])
        {
          _motionComponent->setPreviewPattern (_patterns[channel][pad]);
        }
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
            auto playbackLength = Measure{ 0, getLengthBeats (channel), 0 };
            _patterns[channel][pad]->setPlaybackLength (playbackLength);
            _engine.playPattern (_patterns[channel][pad],
                                 TempoClock::nextDownBeat (_now));
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
        _motionComponent->setPreviewPattern (messagePatternStatus.pattern);
        _channelStrips[channel]->setTextColour (juce::Colours::red);
        break;
      }
    case Status::Stopped:
      {
        _motionComponent->unsetPreviewPattern (messagePatternStatus.pattern);
        _channelStrips[channel]->setTextColour (juce::Colours::white);
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
  if (!_clockMode && measure.tick () == 0)
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
      
      // In EXT mode: set internal clock BPM so patterns run at external tempo
      if (_clockMode)
        {
          _engine.getTempoClock ().setTempoBPM (static_cast<float> (bpm));
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

}
