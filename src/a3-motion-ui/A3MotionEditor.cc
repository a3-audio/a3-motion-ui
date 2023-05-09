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

#include "A3MotionEditor.hh"

#include "Config.hh"

namespace a3
{

A3MotionEditor::A3MotionEditor (A3MotionAudioProcessor &p)
    : AudioProcessorEditor (&p), _mainComponent (numChannelsInitial)
{
  // auto scaleFactor = SystemStats::getEnvironmentVariable
  //     ("OSCCONTROL_SCALE_FACTOR", "1").getFloatValue();
  // setScaleFactor (scaleFactor);
}

A3MotionEditor::~A3MotionEditor () {}

void
A3MotionEditor::paint (juce::Graphics &g)
{
  g.fillAll (getLookAndFeel ().findColour (
      juce::ResizableWindow::backgroundColourId));
}

void
A3MotionEditor::resized ()
{
}

}
