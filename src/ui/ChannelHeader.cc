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

#include "ChannelHeader.hh"

#include "LayoutHints.hh"

namespace a3
{

ChannelHeader::ChannelHeader (Channel const &channel)
    : channel (channel), slidersFX{ { { juce::Slider::RotaryVerticalDrag,
                                        juce::Slider::NoTextBox },
                                      { juce::Slider::RotaryVerticalDrag,
                                        juce::Slider::NoTextBox } } },
      labelsFX{ { { juce::String (), juce::String ("FX1") },
                  { juce::String (), juce::String ("FX2") } } }
{
  for (auto &slider : slidersFX)
    {
      // slider.setNumDecimalPlacesToDisplay (2);
      addChildComponent (slider);
      slider.setVisible (true);
    }
  for (auto &label : labelsFX)
    {
      addChildComponent (label);
      label.setVisible (true);
    }
}

void
ChannelHeader::paint (juce::Graphics &g)
{
  auto const color = findColour (backgroundColourId);
  g.fillAll (color);

  // g.setColour (juce::Colours::red);

  // auto bounds = getLocalBounds ();
  // bounds = bounds.withTrimmedTop (LayoutHints::padding)
  //              .withTrimmedLeft (LayoutHints::padding)
  //              .withTrimmedRight (LayoutHints::padding)
  //              .withHeight (LayoutHints::lineHeight);
  // g.fillRect (bounds);
}

void
ChannelHeader::resized ()
{
  jassert (slidersFX.size () == labelsFX.size ());

  auto bounds = getLocalBounds ();
  bounds = bounds.withTrimmedTop (LayoutHints::padding)
               .withTrimmedLeft (LayoutHints::padding)
               .withTrimmedRight (LayoutHints::padding)
               .withHeight (LayoutHints::lineHeight);

  for (auto idx = 0; idx < slidersFX.size (); ++idx)
    {
      auto boundsLabel = bounds.withTrimmedRight (LayoutHints::lineHeight);

      labelsFX[idx].setBounds (boundsLabel);
      slidersFX[idx].setBounds (boundsLabel.withWidth (LayoutHints::lineHeight)
                                    .withRightX (bounds.getRight ()));
      bounds = bounds.translated (0, LayoutHints::lineHeight);
    }
}
}
