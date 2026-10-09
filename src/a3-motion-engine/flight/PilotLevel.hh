/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <optional>

namespace a3
{

/** How much the pilots do of their own accord (FPV). OFF: a game only when
 *  an action asks for one. HINT: a pilot that sees its moment coming lights
 *  the pad of a fitting action on its channel. FLY: pilots start fitting
 *  games themselves. Global, the DJ's choice. */
enum class PilotLevel
{
  Off,
  Hint,
  Fly
};

/** "off", "hint", "fly": how the level is written in the settings file. */
juce::String pilotLevelWord (PilotLevel level);
std::optional<PilotLevel> pilotLevelNamed (juce::String const &word);
/** What one press of the level key makes of `level`: OFF -> HINT -> FLY -> OFF. */
PilotLevel nextPilotLevel (PilotLevel level);

}
