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

#include "A3MotionAudioProcessor.hh"

#include <a3-motion-ui/components/A3MotionUIComponent.hh>

namespace a3
{

class A3MotionEditor : public juce::AudioProcessorEditor
{
public:
  A3MotionEditor (A3MotionAudioProcessor &);
  ~A3MotionEditor ();

  void paint (juce::Graphics &g) override;
  void resized () override;

private:
  A3MotionUIComponent _motionController;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (A3MotionEditor)
};

}
