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
#include <a3-motion-engine/flight/GameTuning.hh>
#include <a3-motion-engine/preview/MusicCue.hh>

namespace a3
{

/** The games whose moment is open in a bar. */
struct FittingGames
{
  bool fakeOut = false;
  bool formation = false;
  bool hideAndSeek = false;
  bool callAndResponse = false;

  bool has (PilotGame game) const;
  bool any () const;
};

/** How long before its 1 a game must start, in beats: the run-up its figure
 *  needs. */
double minLeadBeats (PilotGame game, int beatsPerBar, GameTuning const &tuning);

/** The beat a game started at `beats` lands on -- its 1. The next change the
 *  cue names that fits the game, if at least the run-up away; else the next
 *  fallbackPhraseBars line that is. Call & response has no change to land
 *  on: its first call is on the next downbeat a run-up away. `beats` itself in
 *  a meter of no beats. */
double climaxBeatsFor (PilotGame game, MusicCue const &cue, double beats, int beatsPerBar,
                       GameTuning const &tuning);

/** How long a game goes on after its 1, in beats: a bar holding its last
 *  figure, or a call & response's calls. */
double afterClimaxBeats (PilotGame game, int beatsPerBar, GameTuning const &tuning);

/** Which games fit bar `bar` by the cue -- what a pilot sees coming. Counted
 *  from the bar's downbeat; nothing while the music is quiet. */
FittingGames fittingGames (MusicCue const &cue, long long bar, int beatsPerBar,
                           GameTuning const &tuning);

}
