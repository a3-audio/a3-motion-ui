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

#include "ui/LayoutHints.hh"

namespace a3
{

MainComponent::MainComponent (int const numChannels)
{
  createChannels (numChannels);
  createChannelsUI ();

  addChildComponent (motionComp);
  motionComp.setVisible (true);
}

MainComponent::~MainComponent () {}

void
MainComponent::createChannels (int const numChannels)
{
  channels.resize (numChannels);
}

void
MainComponent::createChannelsUI ()
{
  auto const numChannels = channels.size ();

  juce::Logger::writeToLog (juce::String (numChannels));

  headers.reserve (numChannels);
  footers.reserve (numChannels);

  juce::Random rng;
  for (auto const &channel : channels)
    {
      auto &header = headers.emplace_back (channel);
      auto &footer = footers.emplace_back (channel);

      addChildComponent (header);
      addChildComponent (footer);

      header.setVisible (true);
      footer.setVisible (true);

      // set the channel colors
      // auto color = Colour::fromHSL (rng.nextFloat (), 0.5f, 0.8f, 1.f);
    }
}

void
MainComponent::paint (juce::Graphics &g)
{
  // g.fillAll (Colours::blueviolet);
}

float
MainComponent::getMinimumWidth () const
{
  jassert (headers.size () == footers.size ());
  return headers.size () * LayoutHints::Channels::widthMin;
}

float
MainComponent::getMinimumHeight () const
{
  return LayoutHints::Channels::heightHeader
         + LayoutHints::Channels::heightFooter
         + LayoutHints::MotionComponent::heightMin;
}

void
MainComponent::resized ()
{
  jassert (headers.size () == footers.size ());

  juce::Component::resized ();

  auto bounds = getLocalBounds ();

  // Motion Component
  auto boundsMotion
      = bounds.withTrimmedTop (LayoutHints::Channels::heightHeader)
            .withTrimmedBottom (LayoutHints::Channels::heightFooter);
  motionComp.setBounds (boundsMotion);

  // Channel headers/footers
  auto widthChannel = bounds.getWidth () / float (headers.size ());
  for (auto idxChannel = 0; idxChannel < headers.size (); ++idxChannel)
    {
      auto offsetInt = juce::roundToInt (idxChannel * widthChannel);
      auto offsetIntNext = juce::roundToInt ((idxChannel + 1) * widthChannel);
      auto widthInt = offsetIntNext - offsetInt; // account for
                                                 // rounding discrepancies

      headers[idxChannel].setBounds (offsetInt, 0, widthInt,
                                     LayoutHints::Channels::heightHeader);
      footers[idxChannel].setBounds (
          offsetInt,
          LayoutHints::Channels::heightHeader + boundsMotion.getHeight (),
          widthInt, LayoutHints::Channels::heightFooter);
    }
}

}
