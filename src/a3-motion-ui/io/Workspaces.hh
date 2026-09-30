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

#include <vector>

namespace a3
{
/** The rig's i3 workspaces, for the bar's DECK key and its list.
 *
 *  Their names are set in a3-core's i3 config ("1:MOTION", "2:STEMDECK",
 *  ...); the list is read from i3 rather than kept here, so a workspace
 *  added there shows up without a change to this program. StemDeck asks
 *  the same way (its Source/Workspaces.h). */
namespace workspaces
{
struct Workspace
{
  /** What i3 is asked for: `workspace number 3`. */
  int number = 0;
  /** The name after the number: "3:REAPER" -> "REAPER". */
  juce::String label;
  /** The one on the screen now. */
  bool current = false;
};

/** StemDeck's workspace, where DECK goes. */
constexpr int stemDeck = 2;

/** What `i3-msg -t get_workspaces` printed, as the list offers it:
 *  workspaces without a number are left out, and anything but a list is
 *  no workspaces. */
std::vector<Workspace> parse (juce::String const &i3Reply);

/** Asks i3. Empty when there is no i3 to ask. */
std::vector<Workspace> list ();

/** Tells i3 to show that workspace. */
void goTo (int number);
}
}
