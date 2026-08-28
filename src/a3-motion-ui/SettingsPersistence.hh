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

#include <a3-motion-engine/AutomationMode.hh>

namespace a3
{

/** Persisted UI preference — a small JSON
 *  file alongside config.json, read once at startup and rewritten whenever
 *  one of these actually changes, so they survive app restarts. */
struct AppSettings
{
  int clockMode = 0;

  /** What a recording pass writes where the finger is not. Touch is what the
   *  device did before this was a choice, so a settings file that predates it
   *  keeps behaving exactly as it did. */
  AutomationMode automationMode = AutomationMode::Touch;
};

/** Returns defaults if the file doesn't exist or fails to parse as JSON. */
AppSettings loadSettings (juce::File const &file);

void saveSettings (juce::File const &file, AppSettings const &settings);

}
