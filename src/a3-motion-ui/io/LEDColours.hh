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

namespace a3
{

struct LEDColours
{
  static const juce::Colour empty;
  static const juce::Colour idle;
  static const juce::Colour scheduledForRecording;
  static const juce::Colour recording;
  static const juce::Colour scheduledForPlaying;
  static const juce::Colour playing;
};

const juce::Colour LEDColours::empty = juce::Colour (0, 0, 0);
const juce::Colour LEDColours::idle = juce::Colour (150, 150, 150);

const juce::Colour LEDColours::scheduledForRecording = juce::Colour (30, 0, 0);
const juce::Colour LEDColours::recording = juce::Colour (150, 0, 0);

const juce::Colour LEDColours::scheduledForPlaying = juce::Colour (0, 30, 0);
const juce::Colour LEDColours::playing = juce::Colour (0, 150, 0);

}
