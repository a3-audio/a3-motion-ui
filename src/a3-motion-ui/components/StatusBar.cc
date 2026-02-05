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
    : _tickIndicator (beatsPerBar), _valueBPM (valueBPM)
{
  addChildComponent (_tickIndicator);
  _tickIndicator.setVisible (true);
  
  addChildComponent (_labelBPM);
  _labelBPM.setVisible (true);
  _labelBPM.setJustificationType (juce::Justification::centredLeft);
  
  addChildComponent (_labelBeatClock);
  _labelBeatClock.setVisible (true);
  _labelBeatClock.setJustificationType (juce::Justification::centredRight);
}

void
StatusBar::resized ()
{
  auto bounds = getLocalBounds ();

  // Symmetrical padding above and below the clock/timer display
  auto const verticalPadding = bounds.getHeight () / 6.f;
  bounds.removeFromTop (verticalPadding);
  bounds.removeFromBottom (verticalPadding);

  // BPM label on the left
  auto leftArea = bounds.removeFromLeft (bounds.getWidth () / 3);
  _labelBPM.setBounds (leftArea.withTrimmedLeft (LayoutHints::padding));

  // Beat clock label on the right
  auto rightArea = bounds.removeFromRight (bounds.getWidth () / 2);
  _labelBeatClock.setBounds (rightArea.withTrimmedRight (LayoutHints::padding));

  // Tick indicator in the center
  auto boundsTicks = bounds.withSizeKeepingCentre (bounds.getWidth () * 0.8f,
                                                   bounds.getHeight () * 0.6f);
  _tickIndicator.setBounds (boundsTicks);
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
StatusBar::beatCallback (Measure measure)
{
  _tickIndicator.setCurrentTick (measure.beat ());
}

void
StatusBar::setExternalBPM (float bpm)
{
  _externalBPM = bpm;
  
  auto stringStream = std::stringstream ();
  stringStream.precision (1);
  stringStream << std::fixed << bpm << " BPM";
  
  // Update on message thread
  juce::MessageManager::callAsync ([this, str = stringStream.str ()] () {
    _labelBPM.setText (str, juce::dontSendNotification);
  });
}

void
StatusBar::setBeatClock (int beat, int bar)
{
  _beatClockBeat = beat;
  _beatClockBar = bar;
  
  auto text = juce::String (bar) + "." + juce::String (beat);
  
  // Update on message thread
  juce::MessageManager::callAsync ([this, text] () {
    _labelBeatClock.setText (text, juce::dontSendNotification);
  });
}

}
