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

#include "LEDColours.hh"
#include <a3-motion-engine/UserConfig.hh>

namespace a3
{

juce::Colour LEDColours::empty = juce::Colour (0, 0, 0);
juce::Colour LEDColours::scheduledForIdle = juce::Colour (60, 30, 10);
juce::Colour LEDColours::idle = juce::Colour (200, 80, 20);
juce::Colour LEDColours::scheduledForRecording = juce::Colour (55, 0, 0);
juce::Colour LEDColours::recording = juce::Colour (150, 0, 0);
juce::Colour LEDColours::scheduledForPlaying = juce::Colour (0, 45, 0);
juce::Colour LEDColours::playing = juce::Colour (0, 150, 0);

void
LEDColours::initializeFromConfig ()
{
  auto const& ledConfig = userConfig["ledColours"];

  if (!ledConfig.isVoid ())
    {
      if (!ledConfig["idle"].isVoid ())
        {
          idle = juce::Colour (
              (int)ledConfig["idle"]["r"],
              (int)ledConfig["idle"]["g"],
              (int)ledConfig["idle"]["b"]);
        }

      if (!ledConfig["scheduledForIdle"].isVoid ())
        {
          scheduledForIdle = juce::Colour (
              (int)ledConfig["scheduledForIdle"]["r"],
              (int)ledConfig["scheduledForIdle"]["g"],
              (int)ledConfig["scheduledForIdle"]["b"]);
        }

      if (!ledConfig["recording"].isVoid ())
        {
          recording = juce::Colour (
              (int)ledConfig["recording"]["r"],
              (int)ledConfig["recording"]["g"],
              (int)ledConfig["recording"]["b"]);
        }

      if (!ledConfig["scheduledForRecording"].isVoid ())
        {
          scheduledForRecording = juce::Colour (
              (int)ledConfig["scheduledForRecording"]["r"],
              (int)ledConfig["scheduledForRecording"]["g"],
              (int)ledConfig["scheduledForRecording"]["b"]);
        }

      if (!ledConfig["playing"].isVoid ())
        {
          playing = juce::Colour (
              (int)ledConfig["playing"]["r"],
              (int)ledConfig["playing"]["g"],
              (int)ledConfig["playing"]["b"]);
        }

      if (!ledConfig["scheduledForPlaying"].isVoid ())
        {
          scheduledForPlaying = juce::Colour (
              (int)ledConfig["scheduledForPlaying"]["r"],
              (int)ledConfig["scheduledForPlaying"]["g"],
              (int)ledConfig["scheduledForPlaying"]["b"]);
        }
    }
}

}
