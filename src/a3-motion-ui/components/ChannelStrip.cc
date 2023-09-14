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

ChannelStrip::ChannelStrip (ChannelUIState const &uiState)
    : _uiState (uiState), _directivity (uiState)
{
  addChildComponent (_directivity);
  _directivity.setVisible (true);

  auto font = juce::Font (juce::Font::getDefaultMonospacedFontName (), 30,
                          juce::Font::FontStyleFlags::bold);

  addChildComponent (_labelBars);
  _labelBars.setFont (font);
  _labelBars.setText ("1/2", juce::NotificationType::dontSendNotification);
  _labelBars.setJustificationType (juce::Justification::centred);
  _labelBars.setVisible (true);
}

void
ChannelStrip::resized ()
{
  auto bounds = getLocalBounds ();

  // bounds.removeFromTop (LayoutHints::padding);
  bounds.removeFromBottom (LayoutHints::padding);

  auto paddingDirectivity = bounds.getWidth () * 0.08f;
  bounds.removeFromTop (paddingDirectivity);
  bounds.removeFromBottom (paddingDirectivity);
  bounds.removeFromLeft (paddingDirectivity);
  bounds.removeFromRight (paddingDirectivity);

  bounds.setHeight (bounds.getWidth ());
  _directivity.setBounds (bounds);
  _labelBars.setBounds (bounds);
}

void
ChannelStrip::paint (juce::Graphics &g)
{
  juce::ignoreUnused (g);
}

DirectivityComponent &
ChannelStrip::getDirectivityComponent ()
{
  return _directivity;
}

void
ChannelStrip::setTextBarsLabel (juce::String text)
{
  _labelBars.setText (text, juce::NotificationType::dontSendNotification);
}

}
