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

#include <a3-motion-engine/PilotOrder.hh>
#include <a3-motion-engine/flight/GameMoments.hh>
#include <a3-motion-engine/flight/GameTuning.hh>
#include <a3-motion-engine/preview/MusicCue.hh>

#include <JuceHeader.h>

#include <optional>

namespace a3
{

/** Who asks the music: the DJ's press, or a pilot at FLY. A press is always
 *  answered; a pilot moves only when the moment is its own. */
enum class Asker
{
  Press,
  Pilot,
};

/** The family the music asks for in bar `bar`.
 *  - Quiet (energy below quietEnergy): SOFT for a press, nothing for a pilot.
 *  - Drop: HARD for a press in any bar; for a pilot only in its first
 *    hardDropBars bars, counted from the bar it began (nothing while that is
 *    not known) -- then cruise, the contrast.
 *  - Build: RISING. Breakdown: SOFT.
 *  - Groove: a previewed energy at or above hardEnergy is HARD for a press
 *    and GROOVE for a pilot; below softEnergy SOFT for both; else GROOVE.
 *    The live mood's energy is a raw meter level: no threshold applies. */
std::optional<PilotFamily> pilotFamily (MusicCue const &cue, long long bar, Asker asker,
                                        GameTuning const &tuning);

/** The figure of `family` the floor allows. HARD: Slap. RISING: Climb when
 *  it fits, else fake-out or formation as they fit (a throw when both do),
 *  else -- for a press only -- Climb to the next phrase line. SOFT: Graze
 *  where a group leaves room for it (a press throws Graze or Encircle; a
 *  pilot never encircles: an escort is the DJ's gesture), else Hide & seek.
 *  GROOVE: Call & response, a pilot only in its window. Nothing when a
 *  pilot has nothing to play. */
std::optional<PilotGame> chooseFigure (PilotFamily family, Asker asker,
                                       FittingGames const &fitting, bool grazeFits,
                                       juce::Random &dice);

/** `game`, or Hide & seek for a soft figure with nothing to be played
 *  against: a Graze with no room for its pass, an Encircle with no group. */
PilotGame playableFigure (PilotGame game, bool groupThere, bool grazeFits);

}
