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

#include <a3-motion-ui/components/ChannelUIState.hh>

namespace a3
{

ChannelStrip::ChannelStrip (ChannelUIState const &uiState,
                            TempoClock const &tempoClock)
    : _uiState (uiState), _patternMenu (tempoClock)
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

  g.setColour (_uiState.colour.withLightness (0.3));
  g.fillRect (bounds);

  auto boundsTop = bounds.removeFromTop (LayoutHints::padding);
  // boundsTop.setWidth (boundsTop.getWidth () * _uiState.progress);
  auto boundsBottom = bounds.removeFromBottom (LayoutHints::padding);
  boundsBottom.setWidth (boundsBottom.getWidth () * _uiState.progress);

  g.setColour (_uiState.colour);
  g.fillRect (boundsTop);
  g.fillRect (boundsBottom);

  g.setColour (_uiState.colour.withLightness (0.3));
  g.fillRect (bounds);
}

PatternMenu &
ChannelStrip::getPatternMenu ()
{
  return _patternMenu;
}

}
