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

namespace
{
auto constexpr beatsPerBar = 4; // TODO read from tempoclock
}

namespace a3
{

StatusBar::StatusBar (juce::Value &tempoBPM)
    : _ticks (beatsPerBar), _tempoBPM (tempoBPM)
{
  addChildComponent (_ticks);
  _ticks.setVisible (true);
}

void
StatusBar::resized ()
{
  auto bounds = getLocalBounds ();

  bounds.removeFromTop (LayoutHints::padding);
  bounds.removeFromLeft (LayoutHints::padding);
  bounds.removeFromBottom (LayoutHints::padding);

  auto boundsTicks = bounds.removeFromLeft (beatsPerBar * bounds.getHeight ());
  _ticks.setBounds (boundsTicks);

  _boundsTextArea = bounds.withTrimmedLeft (LayoutHints::padding);
}

void
StatusBar::paint (juce::Graphics &g)
{
  g.setColour (Colours::statusBar);
  g.fillAll ();

  g.setColour (juce::Colours::white);
  g.setFont (18.f);

  jassert (_tempoBPM.getValue ().isDouble ());
  auto const bpm = static_cast<float> (_tempoBPM.getValue ());

  auto stringStream = std::stringstream ();
  stringStream.precision (2);
  stringStream << std::fixed << bpm << " BPM";
  g.drawText (stringStream.str (), _boundsTextArea,
              juce::Justification::centredLeft);
}

}
