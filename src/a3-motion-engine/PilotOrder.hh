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

/** The games and figures a pilot plays. The first four are the games of the
 *  first version; the last four are the contrast figures, which open on the
 *  next beat and carry their own timing. `None` written out calls off what a
 *  ship plays -- a word with a meaning, unlike a line left out. Appended
 *  only: the engine publishes a game in four bits. */
enum class PilotGame
{
  None,
  FakeOut,
  Formation,
  HideAndSeek,
  CallAndResponse,
  Slap,
  Climb,
  Graze,
  Encircle,
};

/** A character of movement the music asks for: a hit, a rise, a soft
 *  touch, a conversation. A family is chosen by the music; the figure in it
 *  by the floor. */
enum class PilotFamily
{
  Hard,
  Rising,
  Soft,
  Groove,
};

/** What `~style` asks: the music's own family (Auto, also what a script
 *  without the line asks in FPV), one family whatever the music says, or no
 *  figure at all (None: only the action's floor keys fly). */
enum class PilotStyle
{
  Auto,
  Hard,
  Rising,
  Soft,
  Groove,
  None,
};

/** The family a style names; nothing for Auto and None. */
std::optional<PilotFamily> familyOf (PilotStyle style);

/** Slap, Climb, Graze and Encircle: they open on the next beat. */
bool isContrastFigure (PilotGame game);

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

/** The section as a whole. `game` names a figure outright and beats
 *  `style`; with neither, nothing is asked here -- in FPV the press then asks
 *  for Auto (pilotOrderAtPress). */
struct PilotOrder
{
  std::optional<PilotGame> game;
  std::optional<PilotStyle> style;
  PilotTarget target{};
  PilotRecruit with = PilotRecruit::Self;
};

/** The word a script writes after the backslash, and back. Unknown words
 *  are empty, never guessed. */
std::optional<PilotGame> pilotGameNamed (juce::String const &word);
std::optional<PilotTarget> pilotTargetNamed (juce::String const &word);
std::optional<PilotRecruit> pilotRecruitNamed (juce::String const &word);
std::optional<PilotStyle> pilotStyleNamed (juce::String const &word);

juce::String pilotWord (PilotGame game);
juce::String pilotWord (PilotTarget target);
juce::String pilotWord (PilotRecruit with);
juce::String pilotWord (PilotStyle style);

}
