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

/** What an action's Pilot section asks of the channel's pilot (FPV). Read with
 *  the script, kept with the press, ignored in FULL. It only records a game;
 *  the game that starts it plays it.
 */

/** The games of the first version. `None` written out calls off a request
 *  still waiting -- it is a word with a meaning, unlike a line left out. */
enum class PilotGame
{
  None,
  FakeOut,
  Formation,
  HideAndSeek,
  CallAndResponse,
};

enum class PilotTargetKind
{
  Nearest,
  Group,
  Crowd,
  Hotspot,
};

/** \group1 .. \group8: the floor's G1..G8. */
constexpr int pilotGroups = 8;

/** Whom a game is played against. For a Group, `groupId` is the FlightBody::id
 *  the floor labels G(id + 1); -1 for every other kind. */
struct PilotTarget
{
  PilotTargetKind kind = PilotTargetKind::Nearest;
  int groupId = -1;
};

/** Which ships a game takes: this channel's alone, it and the nearest free
 *  one, or every free one. */
enum class PilotRecruit
{
  Self,
  Nearest,
  All,
};

/** The section as a whole. `game` is empty when the script names no game:
 *  then nothing is asked of the pilot, whatever `target` and `with` say. */
struct PilotOrder
{
  std::optional<PilotGame> game;
  PilotTarget target{};
  PilotRecruit with = PilotRecruit::Self;
};

/** The word a script writes after the backslash, and back. Unknown words
 *  are empty, never guessed. */
std::optional<PilotGame> pilotGameNamed (juce::String const &word);
std::optional<PilotTarget> pilotTargetNamed (juce::String const &word);
std::optional<PilotRecruit> pilotRecruitNamed (juce::String const &word);

juce::String pilotWord (PilotGame game);
juce::String pilotWord (PilotTarget target);
juce::String pilotWord (PilotRecruit with);

}
