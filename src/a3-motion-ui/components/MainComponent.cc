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

#include "MainComponent.hh"

#include <a3-motion-ui/LayoutHints.hh>
#include <a3-motion-ui/components/MotionComponent.hh>

namespace a3
{

MainComponent::MainComponent (unsigned int const numChannels)
    : _engine (numChannels), _motionComp ()
{
  setLookAndFeel (&_lookAndFeel);

  createChannelsUI ();

  _motionComp = std::make_unique<MotionComponent> (_engine.getChannels (),
                                                   _viewStates);
  addChildComponent (*_motionComp);
  _motionComp->setVisible (true);
}

MainComponent::~MainComponent ()
{
  setLookAndFeel (nullptr);
}

void
MainComponent::createChannelsUI ()
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
MainComponent::paint (juce::Graphics &g)
{
  juce::ignoreUnused (g);
}

float
MainComponent::getMinimumWidth () const
{
  jassert (_headers.size () == _footers.size ());
  return _headers.size () * LayoutHints::Channels::widthMin;
}

float
MainComponent::getMinimumHeight () const
{
  return LayoutHints::Channels::heightHeader ()
         + LayoutHints::Channels::heightFooter
         + LayoutHints::MotionComponent::heightMin;
}

void
MainComponent::resized ()
{
  jassert (_headers.size () == _footers.size ());

  juce::Component::resized ();

  auto bounds = getLocalBounds ();

  // Motion Component
  auto boundsMotion
      = bounds.withTrimmedTop (LayoutHints::Channels::heightHeader ())
            .withTrimmedBottom (LayoutHints::Channels::heightFooter);
  _motionComp->setBounds (boundsMotion);

  // Channel headers/footers
  auto widthChannel = bounds.getWidth () / float (_headers.size ());
  for (auto idxChannel = 0u; idxChannel < _headers.size (); ++idxChannel)
    {
      auto offsetInt = juce::roundToInt (idxChannel * widthChannel);
      auto offsetIntNext = juce::roundToInt ((idxChannel + 1) * widthChannel);
      auto widthInt = offsetIntNext - offsetInt; // account for
                                                 // rounding discrepancies

      _headers[idxChannel]->setBounds (offsetInt, 0, widthInt,
                                       LayoutHints::Channels::heightHeader ());
      _footers[idxChannel]->setBounds (
          offsetInt,
          LayoutHints::Channels::heightHeader () + boundsMotion.getHeight (),
          widthInt, LayoutHints::Channels::heightFooter);
    }
}

}
