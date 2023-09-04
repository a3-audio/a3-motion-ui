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

#include <chrono>
#include <fstream>

#include <a3-motion-ui/Config.hh>
#include <a3-motion-ui/components/ChannelHeader.hh>
#include <a3-motion-ui/components/ChannelStrip.hh>
#include <a3-motion-ui/components/ChannelViewState.hh>
#include <a3-motion-ui/components/LayoutHints.hh>
#include <a3-motion-ui/components/MotionComponent.hh>
#include <a3-motion-ui/components/StatusBar.hh>

#include <a3-motion-ui/tests/TempoEstimatorTest.hh>

#include <a3-motion-ui/io/InputOutputAdapter.hh>
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

  _viewStates.reserve (numChannels);
  if (_drawHeaders)
    _headers.reserve (numChannels);
  _channelStrips.reserve (numChannels);

  auto hueStart = 0.f;
  for (auto channel = 0u; channel < numChannels; ++channel)
    {
      auto viewState = std::make_unique<ChannelViewState> ();
      auto const hueNorm
          = hueStart + static_cast<float> (channel) / numChannels;
      auto hue = hueNorm / 360.f * 256.f; // for now rescale to
                                          // (arbitrary) range "in
                                          // degrees" that stems from
                                          // misunderstanding the
                                          // QColor documentation of
                                          // the old python
                                          // implementation.

      viewState->colour = juce::Colour::fromHSV (hue, 0.6f, 0.8f, 1.f);

      if (_drawHeaders)
        {
          auto header = std::make_unique<ChannelHeader> (*viewState);
          addChildComponent (*header);
          header->setVisible (true);
          _headers.push_back (std::move (header));
        }

      auto strip = std::make_unique<ChannelStrip> (
          *viewState, _ioAdapter->getEncoderIncrement (channel));
      addChildComponent (*strip);
      strip->setVisible (true);
      _channelStrips.push_back (std::move (strip));

      _viewStates.push_back (std::move (viewState));
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
          [this] (auto measure) { _statusBar->measureChanged (measure); },
          TempoClock::Event::Beat, TempoClock::Execution::JuceMessageThread);

  _motionComponent = std::make_unique<MotionComponent> (_engine, _viewStates);
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
  using Button = InputOutputAdapter::Button;
  _ioAdapter->startThread ();
  _ioAdapter->getButton (Button::Shift).addListener (this);
  _ioAdapter->getButton (Button::Record).addListener (this);
  _ioAdapter->getButton (Button::Tap).addListener (this);
  _ioAdapter->getTapTimeMicros ().addListener (this);
#endif
}

void
A3MotionUIComponent::initializePatterns ()
{
  _patterns.resize (_ioAdapter->getNumChannels ());
  for (auto &pattern : _patterns)
    {
      auto numPatterns = numPages * _ioAdapter->getNumPadsPerChannel ();
      pattern.resize (numPatterns);
    }

  updatePadLEDs ();
}

void
A3MotionUIComponent::updatePadLEDs ()
{
  for (auto channel = 0u; channel < _ioAdapter->getNumChannels (); ++channel)
    for (auto pad = 0u; pad < _ioAdapter->getNumPadsPerChannel (); ++pad)
      _ioAdapter->getPadLED (channel, pad)
          .setValue (juce::VariantConverter<juce::Colour>::toVar (
              juce::Colours::black));
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
  using Button = InputOutputAdapter::Button;

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
          juce::Logger::writeToLog ("tapping: " + juce::String (tapTime));
          if (result == TempoClock::TapResult::TempoAvailable)
            {
              auto const bpm = _engine.getTempoClock ().getTempoBPM ();
              _valueBPM = bpm;
              juce::Logger::writeToLog ("tempo bpm: " + juce::String (bpm));
            }
        }
    }
}

}
