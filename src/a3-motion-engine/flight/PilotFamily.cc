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

#include "PilotFamily.hh"

namespace a3
{

std::optional<PilotFamily>
pilotFamily (MusicCue const &cue, long long bar, Asker asker, GameTuning const &tuning)
{
  auto const press = asker == Asker::Press;
  if (!(cue.energy >= tuning.quietEnergy))
    return press ? std::optional<PilotFamily> (PilotFamily::Soft) : std::nullopt;

  switch (cue.section)
    {
    case MusicSection::Drop:
      {
        if (press)
          return PilotFamily::Hard;
        auto const known = cue.sectionStartBar >= 0 && bar >= cue.sectionStartBar;
        if (known && bar - cue.sectionStartBar < tuning.hardDropBars)
          return PilotFamily::Hard;
        return std::nullopt;
      }
    case MusicSection::Build:
      return PilotFamily::Rising;
    case MusicSection::Breakdown:
      return PilotFamily::Soft;
    case MusicSection::Groove:
      if (cue.previewed && cue.energy >= tuning.hardEnergy)
        return press ? PilotFamily::Hard : PilotFamily::Groove;
      if (cue.previewed && cue.energy < tuning.softEnergy)
        return PilotFamily::Soft;
      return PilotFamily::Groove;
    }
  return std::nullopt;
}

std::optional<PilotGame>
chooseFigure (PilotFamily family, Asker asker, FittingGames const &fitting, bool grazeFits,
              juce::Random &dice)
{
  auto const press = asker == Asker::Press;
  switch (family)
    {
    case PilotFamily::Hard:
      return PilotGame::Slap;
    case PilotFamily::Rising:
      if (fitting.climb)
        return PilotGame::Climb;
      if (fitting.fakeOut && fitting.formation)
        return dice.nextBool () ? PilotGame::Formation : PilotGame::FakeOut;
      if (fitting.fakeOut)
        return PilotGame::FakeOut;
      if (fitting.formation)
        return PilotGame::Formation;
      return press ? std::optional<PilotGame> (PilotGame::Climb) : std::nullopt;
    case PilotFamily::Soft:
      if (!grazeFits)
        return PilotGame::HideAndSeek;
      if (press && dice.nextBool ())
        return PilotGame::Encircle;
      return PilotGame::Graze;
    case PilotFamily::Groove:
      if (press || fitting.callAndResponse)
        return PilotGame::CallAndResponse;
      return std::nullopt;
    }
  return std::nullopt;
}

PilotGame
playableFigure (PilotGame game, bool groupThere, bool grazeFits)
{
  if (game == PilotGame::Graze && !grazeFits)
    return PilotGame::HideAndSeek;
  if (game == PilotGame::Encircle && !groupThere)
    return PilotGame::HideAndSeek;
  return game;
}

}
