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

#include <a3-motion-ui/Config.hh>
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

  createChannelsUI ();
  createHardwareInterface ();

  _statusBar = std::make_unique<StatusBar> (_engine.getTempoClock ());
  addChildComponent (*_statusBar);
  _statusBar->setVisible (true);

  _motionComponent = std::make_unique<MotionComponent> (_engine.getChannels (),
                                                        _viewStates);
  addChildComponent (*_motionComponent);
  _motionComponent->setVisible (true);

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
  auto const numChannels = _engine.getChannels ().size ();

  _viewStates.reserve (numChannels);
  if (_drawHeaders)
    _headers.reserve (numChannels);
  _footers.reserve (numChannels);

  auto hueStart = 0.f;
  auto hueNorm = hueStart;
  for (auto const &channel : _engine.getChannels ())
    {
      auto viewState = std::make_unique<ChannelViewState> ();
      auto hue = hueNorm / 360.f * 256.f; // for now rescale to
                                          // (arbitrary) range "in
                                          // degrees" that stems from
                                          // misunderstanding the
                                          // QColor documentation of
                                          // the old python
                                          // implementation.

      viewState->colour = juce::Colour::fromHSV (hue, 0.6f, 0.8f, 1.f);
      hueNorm += 1.f / numChannels;

      if (_drawHeaders)
        {
          auto header = std::make_unique<ChannelHeader> (*channel, *viewState);
          addChildComponent (*header);
          header->setVisible (true);
          _headers.push_back (std::move (header));
        }

      auto footer = std::make_unique<ChannelFooter> (*channel, *viewState);
      addChildComponent (*footer);
      footer->setVisible (true);
      _footers.push_back (std::move (footer));

      _viewStates.push_back (std::move (viewState));
    }
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
  _ioAdapter->startThread ();
  _ioAdapter->getButton (InputOutputAdapter::Button::Shift).addListener (this);
  _ioAdapter->getButton (InputOutputAdapter::Button::Record)
      .addListener (this);
  _ioAdapter->getButton (InputOutputAdapter::Button::Tap).addListener (this);
  _ioAdapter->getPad (0, 0).addListener (this);
  _ioAdapter->getEncoderIncrement (0).addListener (this);
  _ioAdapter->getEncoderPress (0).addListener (this);
  _ioAdapter->getPot (0, 0).addListener (this);
  _ioAdapter->getPot (0, 1).addListener (this);
  _ioAdapter->getTapTimeMicros ().addListener (this);
#endif
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
    jassert (_headers.size () == _footers.size ());

  juce::Component::resized ();

  auto bounds = getLocalBounds ();

  auto boundsHeaders = juce::Rectangle<int> ();
  if (_drawHeaders)
    {
      boundsHeaders
          = bounds.removeFromTop (LayoutHints::Channels::heightHeader ());
    }
  auto boundsFooters
      = bounds.removeFromBottom (LayoutHints::Channels::heightFooter);

  auto constexpr statusBarOnTop = true;
  auto constexpr statusBarHeight = 25;
  auto boundsStatus = statusBarOnTop
                          ? bounds.removeFromTop (statusBarHeight)
                          : bounds.removeFromBottom (statusBarHeight);
  _statusBar->setBounds (boundsStatus);

  _motionComponent->setBounds (bounds);

  // Channel headers/footers
  auto widthChannel = bounds.getWidth () / float (_footers.size ());
  for (auto channelIndex = 0u; channelIndex < _footers.size (); ++channelIndex)
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
      _footers[channelIndex]->setBounds (
          boundsFooters.removeFromLeft (widthInt));
    }
}

float
A3MotionUIComponent::getMinimumWidth () const
{
  return _footers.size () * LayoutHints::Channels::widthMin;
}

float
A3MotionUIComponent::getMinimumHeight () const
{
  auto minimumHeight = LayoutHints::Channels::heightFooter
                       + LayoutHints::MotionComponent::heightMin;
  if (_drawHeaders)
    {
      minimumHeight += LayoutHints::Channels::heightHeader ();
    }
  return minimumHeight;
}

void
A3MotionUIComponent::valueChanged (juce::Value &value)
{
  if (value.refersToSameSourceAs (
          _ioAdapter->getButton (InputOutputAdapter::Button::Shift)))
    {
      juce::Logger::writeToLog ("MotionController: SHIFT: "
                                + value.toString ());
      _ioAdapter->getButtonLED (InputOutputAdapter::Button::Shift)
          = value.getValue ();
    }
  else if (value.refersToSameSourceAs (
               _ioAdapter->getButton (InputOutputAdapter::Button::Record)))
    {
      juce::Logger::writeToLog ("MotionController: RECORD: "
                                + value.toString ());
      _ioAdapter->getButtonLED (InputOutputAdapter::Button::Record)
          = value.getValue ();
    }
  else if (value.refersToSameSourceAs (
               _ioAdapter->getButton (InputOutputAdapter::Button::Tap)))
    {
      _ioAdapter->getButtonLED (InputOutputAdapter::Button::Tap)
          = value.getValue ();
      if (bool (_ioAdapter->getButton (InputOutputAdapter::Button::Shift)
                    .getValue ()))
        {
          _engine.getTempoClock ().reset ();
        }
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getPad (0, 0)))
    {
      juce::Logger::writeToLog ("MotionController: PAD 0 0: "
                                + value.toString ());
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getEncoderPress (0)))
    {
      juce::Logger::writeToLog ("MotionController: Encoder Press 0: "
                                + value.toString ());
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getEncoderIncrement (0)))
    {
      juce::Logger::writeToLog ("MotionController: Encoder Increment 0: "
                                + value.toString ());
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getPot (0, 0)))
    {
      juce::Logger::writeToLog ("MotionController: Pot 0 0: "
                                + value.toString ());
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getPot (0, 1)))
    {
      juce::Logger::writeToLog ("MotionController: Pot 0 1: "
                                + value.toString ());
    }
  else if (value.refersToSameSourceAs (_ioAdapter->getTapTimeMicros ()))
    {
      if (_engine.getTempoClock ().tap (value.getValue ())
          == TempoClock::TapResult::TempoAvailable)
        {
          _statusBar->repaint ();
        }
    }
}

}
