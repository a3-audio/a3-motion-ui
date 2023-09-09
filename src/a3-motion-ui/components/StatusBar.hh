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

#pragma once

#include <JuceHeader.h>

#include <a3-motion-engine/tempo/TempoClock.hh>

#include <a3-motion-ui/components/LayoutHints.hh>
#include <a3-motion-ui/components/TickIndicator.hh>

namespace a3
{

class StatusBar : public juce::Component, public juce::Value::Listener
{
public:
  StatusBar (juce::Value &valueBPM);

  void resized () override;
  void paint (juce::Graphics &g) override;

  void valueChanged (juce::Value &value) override;
  void beatCallback (Measure measure);

  static constexpr int
  getMinimumHeight ()
  {
    return LayoutHints::lineHeight + 2 * LayoutHints::padding;
  }

private:
  TickIndicator _tickIndicator;

  juce::Label _labelBPM;
  juce::Value &_valueBPM;
};

}
