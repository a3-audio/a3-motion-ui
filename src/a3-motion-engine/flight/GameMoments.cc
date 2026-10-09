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

#include "GameMoments.hh"

#include <cmath>

namespace a3
{

namespace
{
/** The first multiple of `step` beats at or after `beats`. */
double
nextLine (double beats, double step)
{
  return std::ceil (beats / step) * step;
}

bool
headsFor (MusicCue const &cue, MusicSection now, MusicSection next)
{
  return cue.section == now && cue.next && *cue.next == next && cue.changeBar >= 0;
}

/** The section and the change a game lands on, if it has one. */
bool
landsOnTheChange (PilotGame game, MusicCue const &cue)
{
  switch (game)
    {
    case PilotGame::FakeOut:
    case PilotGame::Formation:
      return headsFor (cue, MusicSection::Build, MusicSection::Drop);
    case PilotGame::HideAndSeek:
      return cue.section == MusicSection::Breakdown && cue.next && cue.changeBar >= 0;
    case PilotGame::None:
    case PilotGame::CallAndResponse:
      return false;
    }
  return false;
}
}

bool
FittingGames::has (PilotGame game) const
{
  switch (game)
    {
    case PilotGame::FakeOut:
      return fakeOut;
    case PilotGame::Formation:
      return formation;
    case PilotGame::HideAndSeek:
      return hideAndSeek;
    case PilotGame::CallAndResponse:
      return callAndResponse;
    case PilotGame::None:
      return false;
    }
  return false;
}

bool
FittingGames::any () const
{
  return fakeOut || formation || hideAndSeek || callAndResponse;
}

double
minLeadBeats (PilotGame game, int beatsPerBar, GameTuning const &tuning)
{
  auto const bar = static_cast<double> (beatsPerBar);
  switch (game)
    {
    case PilotGame::FakeOut:
      return tuning.fakeOutLeadBars * bar;
    case PilotGame::Formation:
      return tuning.formationLeadBars * bar;
    case PilotGame::HideAndSeek:
      return tuning.hideLeadBars * bar;
    case PilotGame::CallAndResponse:
      return tuning.callGatherBars * bar;
    case PilotGame::None:
      return 0.;
    }
  return 0.;
}

double
climaxBeatsFor (PilotGame game, MusicCue const &cue, double beats, int beatsPerBar,
                GameTuning const &tuning)
{
  if (beatsPerBar <= 0)
    return beats;

  auto const bar = static_cast<double> (beatsPerBar);
  auto const lead = minLeadBeats (game, beatsPerBar, tuning);

  if (game == PilotGame::CallAndResponse)
    return nextLine (beats + lead, bar);

  if (landsOnTheChange (game, cue))
    {
      auto const change = static_cast<double> (cue.changeBar) * bar;
      if (change - beats >= lead)
        return change;
    }

  auto const phrase = static_cast<double> (tuning.fallbackPhraseBars) * bar;
  return phrase > 0. ? nextLine (beats + lead, phrase) : beats + lead;
}

double
afterClimaxBeats (PilotGame game, int beatsPerBar, GameTuning const &tuning)
{
  auto const bar = static_cast<double> (beatsPerBar);
  switch (game)
    {
    case PilotGame::CallAndResponse:
      return 2. * tuning.exchanges * bar;
    case PilotGame::FakeOut:
    case PilotGame::Formation:
    case PilotGame::HideAndSeek:
      return tuning.afterClimaxBars * bar;
    case PilotGame::None:
      return 0.;
    }
  return 0.;
}

FittingGames
fittingGames (MusicCue const &cue, long long bar, int beatsPerBar, GameTuning const &tuning)
{
  FittingGames out;
  if (beatsPerBar <= 0 || !(cue.energy >= tuning.quietEnergy))
    return out;

  auto const known = cue.changeBar >= 0 && cue.next.has_value ();
  auto const beatsLeft
      = static_cast<double> (cue.changeBar - bar) * static_cast<double> (beatsPerBar);
  auto const leaves = [&] (PilotGame game) {
    return beatsLeft >= minLeadBeats (game, beatsPerBar, tuning);
  };

  switch (cue.section)
    {
    case MusicSection::Build:
      if (headsFor (cue, MusicSection::Build, MusicSection::Drop)
          && cue.changeBar - bar <= tuning.hintHorizonBars)
        {
          out.fakeOut = leaves (PilotGame::FakeOut);
          out.formation = leaves (PilotGame::Formation);
        }
      break;
    case MusicSection::Breakdown:
      out.hideAndSeek = !known || leaves (PilotGame::HideAndSeek);
      break;
    case MusicSection::Groove:
      {
        auto const every = static_cast<long long> (tuning.grooveEveryBars);
        auto const inWindow
            = every > 0 && ((bar % every) + every) % every < tuning.grooveWindowBars;
        auto const length = minLeadBeats (PilotGame::CallAndResponse, beatsPerBar, tuning)
                            + afterClimaxBeats (PilotGame::CallAndResponse, beatsPerBar, tuning);
        out.callAndResponse = inWindow && (!known || beatsLeft >= length);
        break;
      }
    case MusicSection::Drop:
      break;
    }
  return out;
}

}
