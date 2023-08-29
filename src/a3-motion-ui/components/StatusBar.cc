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

StatusBar::StatusBar (TempoClock &tempoClock)
    : _ticks (beatsPerBar), _tempoClock (tempoClock)
{
  addChildComponent (_ticks);
  _ticks.setVisible (true);

  _callbackHandle = _tempoClock.scheduleEventHandlerAddition (
      [this] (auto measure) {
        // _measure = measure;
        _ticks.setCurrentTick (measure.beat);
        repaint ();
      },
      TempoClock::Event::Beat, TempoClock::Execution::JuceMessageThread);
}

void
StatusBar::resized ()
{
  auto bounds = getLocalBounds ();

  bounds.removeFromTop (LayoutHints::padding);
  bounds.removeFromLeft (LayoutHints::padding);
  bounds.removeFromBottom (LayoutHints::padding);

  _boundsTextBPM = bounds.withTrimmedLeft (LayoutHints::padding)
                       .withTrimmedRight (LayoutHints::padding);
  // _boundsTextMeasure = _boundsTextBPM;

  auto boundsTicks = bounds.withSizeKeepingCentre (bounds.getWidth () * 0.3f,
                                                   bounds.getHeight ());
  _ticks.setBounds (boundsTicks);
}

void
StatusBar::paint (juce::Graphics &g)
{
  g.setColour (Colours::statusBar);
  g.fillAll ();

  g.setColour (juce::Colours::white);
  g.setFont (18.f);

  auto const bpm = _tempoClock.getTempoBPM ();

  auto stringStream = std::stringstream ();
  stringStream.precision (1);
  stringStream << "BPM " << std::fixed << bpm;
  g.drawText (stringStream.str (), _boundsTextBPM,
              juce::Justification::centredLeft);

  // stringStream.str ("");
  // stringStream.clear ();
  // stringStream << _measure.bar + 1 << "." << _measure.beat + 1;
  // g.drawText (stringStream.str (), _boundsTextMeasure,
  //             juce::Justification::centredRight);
}

}
