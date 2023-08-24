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

#include "StatusBar.hh"

#include <a3-motion-ui/components/LayoutHints.hh>
#include <a3-motion-ui/components/LookAndFeel.hh>

#include <sstream>

namespace a3
{

void
StatusBar::resized ()
{
}

void
StatusBar::paint (juce::Graphics &g)
{
  g.setColour (Colours::statusBar);
  g.fillAll ();

  g.setColour (juce::Colours::white);
  g.setFont (20.f);

  auto const bpm = 120.f;
  auto boundsTextBPM
      = getLocalBounds ().withTrimmedLeft (LayoutHints::padding);
  auto stringStream = std::stringstream ();
  stringStream.precision (2);
  stringStream << "BPM " << std::fixed << bpm;
  g.drawText (stringStream.str (), boundsTextBPM,
              juce::Justification::centredLeft);
}

}
