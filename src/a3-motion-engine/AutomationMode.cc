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

#include "AutomationMode.hh"

namespace a3
{

bool
shouldWriteTick (AutomationMode mode, bool fingerDown, bool hasTouched)
{
  if (fingerDown)
    return true;

  switch (mode)
    {
    case AutomationMode::Touch:
      return false;
    case AutomationMode::Latch:
      return hasTouched;
    case AutomationMode::Write:
      return true;
    }

  return false;
}

juce::String
automationModeName (AutomationMode mode)
{
  switch (mode)
    {
    case AutomationMode::Touch:
      return "Touch";
    case AutomationMode::Latch:
      return "Latch";
    case AutomationMode::Write:
      return "Write";
    }

  return "Touch";
}

AutomationMode
automationModeFromName (juce::String const &name)
{
  if (name == "Latch")
    return AutomationMode::Latch;
  if (name == "Write")
    return AutomationMode::Write;
  return AutomationMode::Touch;
}

}
