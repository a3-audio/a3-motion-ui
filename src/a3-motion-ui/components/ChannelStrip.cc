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

#include "ChannelStrip.hh"

#include <a3-motion-engine/Channel.hh>

#include <a3-motion-ui/components/ChannelViewState.hh>

namespace a3
{

ChannelStrip::ChannelStrip (ChannelViewState const &viewState,
                            juce::Value &valueEncoderIncrement)
    : _viewState (viewState), _patternMenu (valueEncoderIncrement)
{
  addChildComponent (_patternMenu);
  _patternMenu.setVisible (true);
}

void
ChannelStrip::resized ()
{
  auto bounds = getLocalBounds ();

  bounds.removeFromTop (LayoutHints::padding);
  bounds.removeFromBottom (LayoutHints::padding);
  bounds.removeFromLeft (LayoutHints::padding);
  bounds.removeFromRight (LayoutHints::padding);

  _patternMenu.setBounds (bounds);
}

void
ChannelStrip::paint (juce::Graphics &g)
{
  auto bounds = getLocalBounds ();

  g.setColour (_viewState.colour);
  g.fillRect (bounds.removeFromBottom (LayoutHints::padding));
  g.fillRect (bounds.removeFromTop (LayoutHints::padding));

  g.setColour (_viewState.colour.withLightness (0.3));
  g.fillRect (bounds);
}

}
