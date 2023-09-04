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

#include <a3-motion-ui/components/LookAndFeel.hh>

#include <sstream>

namespace
{
auto constexpr beatsPerBar = 4; // TODO read from tempoclock
}

namespace a3
{

StatusBar::StatusBar (juce::Value &valueBPM)
    : _ticks (beatsPerBar), _valueBPM (valueBPM)
{
  addChildComponent (_ticks);
  _ticks.setVisible (true);
}

void
StatusBar::resized ()
{
  auto bounds = getLocalBounds ();

  bounds.removeFromTop (LayoutHints::padding);
  bounds.removeFromBottom (LayoutHints::padding);

  _labelBPM.setBounds (bounds.withTrimmedLeft (LayoutHints::padding)
                           .withTrimmedRight (LayoutHints::padding));

  auto boundsTicks = bounds.withSizeKeepingCentre (bounds.getWidth () * 0.4f,
                                                   bounds.getHeight () * 0.6f);
  _ticks.setBounds (boundsTicks);
}

void
StatusBar::paint (juce::Graphics &g)
{
  juce::ignoreUnused (g);
}

void
StatusBar::valueChanged (juce::Value &value)
{
  if (value.refersToSameSourceAs (_valueBPM))
    {
      jassert (value.getValue ().isDouble ());

      auto const bpm = static_cast<float> (value.getValue ());
      auto stringStream = std::stringstream ();
      stringStream.precision (1);
      stringStream << "BPM " << std::fixed << bpm;

      _labelBPM.setText (stringStream.str (), juce::dontSendNotification);
    }
}

void
StatusBar::measureChanged (TempoClock::Measure measure)
{
  _ticks.setCurrentTick (measure.beat);
}

}
