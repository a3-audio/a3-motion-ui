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

#include <a3-motion-engine/Config.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/PatternGenerator.hh>

#include <chrono>
#include <fstream>

#include <a3-motion-ui/Config.hh>
#include <a3-motion-ui/Helpers.hh>
#include <a3-motion-ui/components/ChannelHeader.hh>
#include <a3-motion-ui/components/ChannelStrip.hh>
#include <a3-motion-ui/components/ChannelUIState.hh>
#include <a3-motion-ui/components/LayoutHints.hh>
#include <a3-motion-ui/components/MotionComponent.hh>
#include <a3-motion-ui/components/PatternUIState.hh>
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
    : _engine (numChannels)
{
  setLookAndFeel (&_lookAndFeel);

  createHardwareInterface ();
  createChannelsUI ();
  createMainUI ();

  initializePatterns ();

  _tickCallbackHandle = _engine.getTempoClock ().scheduleEventHandlerAddition (
      [this] (auto measure) {
        tickCallback (measure);
        if (measure.beat () == 0 && measure.tick () == 0)
          {
            stepsLED = 0;
          }

        using T =
            typename std::remove_reference<decltype (measure.tick ())>::type;
        jassert (ticksPerStepPadLEDs <= std::numeric_limits<T>::max ());
        auto const divisor = static_cast<T> (ticksPerStepPadLEDs);
        if (measure.tick () % divisor == 0)
          {
            padLEDCallback (stepsLED++);
          }
      },
      TempoClock::Event::Tick, TempoClock::Execution::JuceMessageThread);

  auto constexpr testTempoEstimation = false;
  if (testTempoEstimation)
    {
      _tempoEstimatorTest = std::make_unique<TempoEstimatorTest> ();
      _ioAdapter->getTapTimeMicros ().addListener (_tempoEstimatorTest.get ());
    }
}

A3MotionUIComponent::~A3MotionUIComponent ()
{
#if HARDWARE_INTERFACE_ENABLED
  _ioAdapter->stopThread (-1);
#endif

  setLookAndFeel (nullptr);
}

void
A3MotionUIComponent::createChannelsUI ()
{
  auto const numChannels = _engine.getNumChannels ();

  _channelUIStates.reserve (numChannels);
  if (_drawHeaders)
    _headers.reserve (numChannels);
  _channelStrips.reserve (numChannels);

  auto hueStart = 0.f;
  for (auto channel = 0u; channel < numChannels; ++channel)
    {
      auto uiState = std::make_unique<ChannelUIState> ();
      auto const hueNorm
          = hueStart + static_cast<float> (channel) / numChannels;
      auto hue = hueNorm / 360.f * 256.f; // for now rescale to
                                          // (arbitrary) range "in
                                          // degrees" that stems from
                                          // misunderstanding the
                                          // QColor documentation of
                                          // the old python
                                          // implementation.

      uiState->colour = juce::Colour::fromHSV (hue, 0.6f, 0.8f, 1.f);

      if (_drawHeaders)
        {
          auto header = std::make_unique<ChannelHeader> (*uiState);
          addChildComponent (*header);
          header->setVisible (true);
          _headers.push_back (std::move (header));
        }

      auto strip = std::make_unique<ChannelStrip> (
          *uiState, _ioAdapter->getEncoderIncrement (channel));
      addChildComponent (*strip);
      strip->setVisible (true);
      _channelStrips.push_back (std::move (strip));

      _channelUIStates.push_back (std::move (uiState));
    }
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
  _ioAdapter->getButton (Button::Shift).addListener (this);
  _ioAdapter->getButton (Button::Record).addListener (this);
  _ioAdapter->getButton (Button::Tap).addListener (this);
  _ioAdapter->getTapTimeMicros ().addListener (this);
  for (auto channel = 0u; channel < _ioAdapter->getNumChannels (); ++channel)
    {
      for (auto pad = 0u; pad < _ioAdapter->getNumPadsPerChannel (); ++pad)
        {
          _ioAdapter->getPad (channel, pad).addListener (this);
        }
    }
  _ioAdapter->startThread ();
#endif
}

void
A3MotionUIComponent::initializePatterns ()
{
  auto const numChannels = _ioAdapter->getNumChannels ();
  _patterns.resize (numChannels);
  _patternUIStates.resize (numChannels);

  auto numPatternsPerChannel = numPages * _ioAdapter->getNumPadsPerChannel ();
  for (auto &channelPatterns : _patterns)
    channelPatterns.resize (numPatternsPerChannel);
  for (auto &channelPatternUIStates : _patternUIStates)
    channelPatternUIStates.resize (numPatternsPerChannel);

  blankLEDs ();

  for (auto channel = 0u; channel < numChannels; ++channel)
    {
      _patterns[channel][0]
          = PatternGenerator::createCirclePattern (16, 0.8f, 360.f);
      _patterns[channel][0]->setChannel (channel);

      _patterns[channel][1]
          = PatternGenerator::createCirclePattern (8, 0.8f, 360.f);
      _patterns[channel][1]->setChannel (channel);
    }
}

void
A3MotionUIComponent::blankLEDs ()
{
  for (auto channel = 0u; channel < _ioAdapter->getNumChannels (); ++channel)
    for (auto pad = 0u; pad < _ioAdapter->getNumPadsPerChannel (); ++pad)
      _ioAdapter->getPadLED (channel, pad)
          = juce::VariantConverter<juce::Colour>::toVar (juce::Colours::black);

  _ioAdapter->getButtonLED (Button::Shift) = false;
  _ioAdapter->getButtonLED (Button::Record) = false;
  _ioAdapter->getButtonLED (Button::Tap) = false;
}

void
A3MotionUIComponent::paint (juce::Graphics &g)
{
  juce::ignoreUnused (g);
}

void
A3MotionUIComponent::resized ()
{
  if (_drawHeaders)
    jassert (_headers.size () == _channelStrips.size ());

  juce::Component::resized ();

  auto bounds = getLocalBounds ();

  auto constexpr statusBarOnTop = true;
  auto constexpr statusBarHeight = StatusBar::getMinimumHeight ();
  auto boundsStatus = statusBarOnTop
                          ? bounds.removeFromTop (statusBarHeight)
                          : bounds.removeFromBottom (statusBarHeight);
  _statusBar->setBounds (boundsStatus);

  auto boundsHeaders = juce::Rectangle<int> ();
  if (_drawHeaders)
    {
      boundsHeaders
          = bounds.removeFromTop (ChannelHeader::getMinimumHeight ());
    }
  auto boundsStrips = bounds.removeFromTop (ChannelStrip::getMinimumHeight ());

  _motionComponent->setBounds (bounds);

  // Channel headers/strips
  auto widthChannel = bounds.getWidth () / float (_channelStrips.size ());
  for (auto channelIndex = 0u; channelIndex < _channelStrips.size ();
       ++channelIndex)
    {
      auto offsetInt = juce::roundToInt (channelIndex * widthChannel);
      auto offsetIntNext
          = juce::roundToInt ((channelIndex + 1) * widthChannel);
      auto widthInt = offsetIntNext - offsetInt; // account for
                                                 // rounding discrepancies

      if (_drawHeaders)
        {
          _headers[channelIndex]->setBounds (
              boundsHeaders.removeFromLeft (widthInt));
        }
      _channelStrips[channelIndex]->setBounds (
          boundsStrips.removeFromLeft (widthInt));
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
  auto minimumHeight = ChannelStrip::getMinimumHeight ()
                       + LayoutHints::MotionComponent::heightMin;
  if (_drawHeaders)
    {
      minimumHeight += ChannelHeader::getMinimumHeight ();
    }
  return minimumHeight;
}

void
A3MotionUIComponent::valueChanged (juce::Value &value)
{
  if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::Shift)))
    {
      _ioAdapter->getButtonLED (Button::Shift) = value.getValue ();
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::Record)))
    {
      _ioAdapter->getButtonLED (Button::Record) = value.getValue ();
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getButton (Button::Tap)))
    {
      _ioAdapter->getButtonLED (Button::Tap) = value.getValue ();
      if (_ioAdapter->getButton (Button::Shift).getValue ()
          && _ioAdapter->getButton (Button::Tap).getValue ())
        {
          _engine.getTempoClock ().reset ();
        }
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getTapTimeMicros ()))
    {
      if (!_ioAdapter->getButton (Button::Shift).getValue ())
        {
          auto const tapTime = juce::int64 (value.getValue ());
          auto const result = _engine.getTempoClock ().tap (tapTime);
          if (result == TempoClock::TapResult::TempoAvailable)
            {
              auto const bpm = _engine.getTempoClock ().getTempoBPM ();
              _valueBPM = bpm;
            }
        }
    }
  else
    {
      for (auto channel = 0u; channel < _ioAdapter->getNumChannels ();
           ++channel)
        {
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
                      _motionComponent->setPreviewPattern (nullptr);
                    }
                }
            }
        }
    }
}

void
A3MotionUIComponent::handlePadPress (index_t channel, index_t pad)
{
  // juce::Logger::writeToLog ("pad (" + juce::String (channel) + ", "
  //                           + juce::String (pad) + ")");

  // TODO read from UI
  auto const recordLength = Measure (1, 0, 0);

  if (isButtonPressed (Button::Record))
    {
      if (!_patterns[channel][pad])
        {
          _patterns[channel][pad] = std::make_shared<Pattern> ();
          _patterns[channel][pad]->setChannel (channel);
        }
      _engine.recordPattern (_patterns[channel][pad],
                             TempoClock::nextDownBeat (_now), recordLength);
    }
  else if (isButtonPressed (Button::Shift))
    {
      _motionComponent->setPreviewPattern (_patterns[channel][pad]);
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

bool
A3MotionUIComponent::isButtonPressed (Button button)
{
  return _ioAdapter->getButton (button).getValue ();
}

void
A3MotionUIComponent::tickCallback (Measure measure)
{
  _now = measure;
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
                    if (step % 2 == 0)
                      {
                        colour = LEDColours::scheduledForIdle;
                      }
                    else
                      {
                        if (statusLast == Pattern::Status::Playing || //
                            statusLast == Pattern::Status::ScheduledForPlaying)
                          {
                            colour = LEDColours::scheduledForPlaying;
                          }
                        else
                          {
                            colour = LEDColours::scheduledForRecording;
                          }
                      }
                  }
                }
              _ioAdapter->getPadLED (channel, pad)
                  = juce::VariantConverter<juce::Colour>::toVar (colour);
            }
        }
    }
}

}
