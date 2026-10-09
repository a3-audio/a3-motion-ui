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

#include "PilotLevel.hh"

namespace a3
{

juce::String
pilotLevelWord (PilotLevel level)
{
  switch (level)
    {
    case PilotLevel::Off:
      return "off";
    case PilotLevel::Hint:
      return "hint";
    case PilotLevel::Fly:
      return "fly";
    }
  return "off";
}

std::optional<PilotLevel>
pilotLevelNamed (juce::String const &word)
{
  for (auto const level : { PilotLevel::Off, PilotLevel::Hint, PilotLevel::Fly })
    if (word == pilotLevelWord (level))
      return level;
  return std::nullopt;
}

PilotLevel
nextPilotLevel (PilotLevel level)
{
  switch (level)
    {
    case PilotLevel::Off:
      return PilotLevel::Hint;
    case PilotLevel::Hint:
      return PilotLevel::Fly;
    case PilotLevel::Fly:
      return PilotLevel::Off;
    }
  return PilotLevel::Off;
}

}
