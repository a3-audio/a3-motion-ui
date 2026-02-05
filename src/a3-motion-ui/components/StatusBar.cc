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
  _labelBPM.setText ("BPM 60.0", juce::dontSendNotification);  // Default tempo
  _labelBPM.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
  
  // Register for BPM value changes
  _valueBPM.addListener (this);
  
  addChildComponent (_labelBeatClock);
  _labelBeatClock.setVisible (true);
  _labelBeatClock.setJustificationType (juce::Justification::centredRight);
  _labelBeatClock.setText ("1.1", juce::dontSendNotification);  // Initial beat/bar
  _labelBeatClock.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
  
  addChildComponent (_labelClockMode);
  _labelClockMode.setVisible (true);
  _labelClockMode.setJustificationType (juce::Justification::centredRight);
  _labelClockMode.setText ("INT", juce::dontSendNotification);
  _labelClockMode.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
}

StatusBar::~StatusBar ()
{
  _valueBPM.removeListener (this);
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
  auto leftArea = bounds.removeFromLeft (bounds.getWidth () / 4);
  _labelBPM.setBounds (leftArea.withTrimmedLeft (LayoutHints::padding));

  // Clock mode label on the far right
  auto clockModeArea = bounds.removeFromRight (50);
  _labelClockMode.setBounds (clockModeArea.withTrimmedRight (LayoutHints::padding));

  // Beat clock label next to clock mode
  auto rightArea = bounds.removeFromRight (bounds.getWidth () / 3);
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
      // Only update BPM display when in internal clock mode
      if (_clockModeExternal)
        return;
        
      jassert (value.getValue ().isDouble ());

      auto const bpm = static_cast<float> (value.getValue ());
      auto stringStream = std::stringstream ();
      stringStream.precision (1);
      stringStream << "BPM " << std::fixed << bpm;

      _labelBPM.setText (stringStream.str (), juce::dontSendNotification);
      _labelBPM.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
    }
}

void
StatusBar::beatCallback (Measure measure)
{
  _tickIndicator.setCurrentTick (measure.beat ());
  
  // Only update beat/bar display when in internal clock mode
  if (_clockModeExternal)
    return;
    
  // Show as beat/4 (beatsPerBar is fixed to 4)
  auto text = juce::String (measure.beat () + 1) + "/4";
  _labelBeatClock.setText (text, juce::dontSendNotification);
  _labelBeatClock.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
}

void
StatusBar::setExternalBPM (float bpm)
{
  _externalBPM = bpm;
  
  // Only update BPM display when in external clock mode
  if (!_clockModeExternal)
    return;
  
  auto stringStream = std::stringstream ();
  stringStream.precision (1);
  stringStream << std::fixed << bpm << " BPM";
  
  // Update on message thread
  juce::MessageManager::callAsync ([this, str = stringStream.str ()] () {
    _labelBPM.setText (str, juce::dontSendNotification);
    _labelBPM.setColour (juce::Label::textColourId, juce::Colours::orange);
  });
}

void
StatusBar::setBeatClock (int beat, int bar)
{
  _beatClockBeat = beat;
  _beatClockBar = bar;
  
  // Only update beat/bar display when in external clock mode
  if (!_clockModeExternal)
    return;
  
  // Show as beat/4 (beatsPerBar is fixed to 4)
  auto text = juce::String (beat) + "/4";
  
  // Update on message thread
  juce::MessageManager::callAsync ([this, text] () {
    _labelBeatClock.setText (text, juce::dontSendNotification);
    _labelBeatClock.setColour (juce::Label::textColourId, juce::Colours::orange);
  });
}

void
StatusBar::setClockMode (bool external)
{
  _clockModeExternal = external;
  
  juce::MessageManager::callAsync ([this, external] () {
    if (external)
      {
        _labelClockMode.setText ("EXT", juce::dontSendNotification);
        _labelClockMode.setColour (juce::Label::textColourId, juce::Colours::orange);
        _labelBPM.setColour (juce::Label::textColourId, juce::Colours::orange);
        _labelBeatClock.setColour (juce::Label::textColourId, juce::Colours::orange);
        
        // Show external BPM if available
        float extBpm = _externalBPM.load ();
        if (extBpm > 0.f)
          {
            auto stringStream = std::stringstream ();
            stringStream.precision (1);
            stringStream << std::fixed << extBpm << " BPM";
            _labelBPM.setText (stringStream.str (), juce::dontSendNotification);
          }
        
        // Show external beat clock if available
        int beat = _beatClockBeat.load ();
        int bar = _beatClockBar.load ();
        if (beat > 0 || bar > 0)
          {
            auto text = juce::String (bar) + "." + juce::String (beat);
            _labelBeatClock.setText (text, juce::dontSendNotification);
          }
      }
    else
      {
        _labelClockMode.setText ("INT", juce::dontSendNotification);
        _labelClockMode.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
        _labelBPM.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
        _labelBeatClock.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
        
        // Show internal BPM
        if (_valueBPM.getValue ().isDouble ())
          {
            auto const bpm = static_cast<float> (_valueBPM.getValue ());
            auto stringStream = std::stringstream ();
            stringStream.precision (1);
            stringStream << "BPM " << std::fixed << bpm;
            _labelBPM.setText (stringStream.str (), juce::dontSendNotification);
          }
      }
  });
}

}
