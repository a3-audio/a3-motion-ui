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

#include "MotionController.hh"

#include <a3-motion-ui/components/LayoutHints.hh>
#include <a3-motion-ui/components/MotionComponent.hh>

namespace a3
{

MotionController::MotionController (unsigned int const numChannels)
    : _engine (numChannels), _motionComponent ()
{
  setLookAndFeel (&_lookAndFeel);

  createChannelsUI ();

  _motionComponent = std::make_unique<MotionComponent> (_engine.getChannels (),
                                                        _viewStates);
  addChildComponent (*_motionComponent);
  _motionComponent->setVisible (true);
}

MotionController::~MotionController ()
{
  setLookAndFeel (nullptr);
}

void
MotionController::createChannelsUI ()
{
  auto const numChannels = _engine.getChannels ().size ();

  _viewStates.reserve (numChannels);
  _footers.reserve (numChannels);
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

      auto header = std::make_unique<ChannelHeader> (*channel, *viewState);
      auto footer = std::make_unique<ChannelFooter> (*channel, *viewState);

      addChildComponent (*header);
      addChildComponent (*footer);

      header->setVisible (true);
      footer->setVisible (true);

      _viewStates.push_back (std::move (viewState));
      _headers.push_back (std::move (header));
      _footers.push_back (std::move (footer));
    }
}

void
MotionController::paint (juce::Graphics &g)
{
  juce::ignoreUnused (g);
}

float
MotionController::getMinimumWidth () const
{
  jassert (_headers.size () == _footers.size ());
  return _headers.size () * LayoutHints::Channels::widthMin;
}

float
MotionController::getMinimumHeight () const
{
  return LayoutHints::Channels::heightHeader ()
         + LayoutHints::Channels::heightFooter
         + LayoutHints::MotionComponent::heightMin;
}

void
MotionController::resized ()
{
  jassert (_headers.size () == _footers.size ());

  juce::Component::resized ();

  auto bounds = getLocalBounds ();

  // Motion Component
  auto boundsMotion
      = bounds.withTrimmedTop (LayoutHints::Channels::heightHeader ())
            .withTrimmedBottom (LayoutHints::Channels::heightFooter);
  _motionComponent->setBounds (boundsMotion);

  // Channel headers/footers
  auto widthChannel = bounds.getWidth () / float (_headers.size ());
  for (auto channelIndex = 0u; channelIndex < _headers.size (); ++channelIndex)
    {
      auto offsetInt = juce::roundToInt (channelIndex * widthChannel);
      auto offsetIntNext
          = juce::roundToInt ((channelIndex + 1) * widthChannel);
      auto widthInt = offsetIntNext - offsetInt; // account for
                                                 // rounding discrepancies

      _headers[channelIndex]->setBounds (
          offsetInt, 0, widthInt, LayoutHints::Channels::heightHeader ());
      _footers[channelIndex]->setBounds (
          offsetInt,
          LayoutHints::Channels::heightHeader () + boundsMotion.getHeight (),
          widthInt, LayoutHints::Channels::heightFooter);
    }
}

}
