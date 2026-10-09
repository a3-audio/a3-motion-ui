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

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/components/ControllerComponent.hh>
#include <a3-motion-ui/components/fpv/PilotHint.hh>
#include <a3-motion-ui/io/PadFunctions.hh>
#include <a3-motion-ui/theme/PadStatusColours.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

#include <cstdlib>

using namespace a3;

// At HINT a pilot that sees its moment coming lights the pad of an action on
// its channel whose script names a game that fits. Only actions carrying a
// ~game light.

namespace
{
using Games = std::array<std::optional<PilotGame>, numActionButtons>;

FittingGames
buildingToADrop ()
{
  FittingGames fitting;
  fitting.fakeOut = true;
  fitting.formation = true;
  return fitting;
}

Games
withGames (std::initializer_list<std::pair<int, PilotGame>> buttons)
{
  Games games{};
  for (auto const &[button, game] : buttons)
    games[static_cast<size_t> (button)] = game;
  return games;
}
}

TEST (PilotHint, TheLowestFittingActionLights)
{
  auto const games = withGames ({ { 1, PilotGame::HideAndSeek }, { 3, PilotGame::Formation },
                                  { 4, PilotGame::FakeOut } });
  EXPECT_EQ (hintedButton (PilotLevel::Hint, AppView::Fpv, buildingToADrop (), true, games), 3);
}

TEST (PilotHint, OnlyAtHintInFpvOnAFreeShip)
{
  auto const games = withGames ({ { 0, PilotGame::FakeOut } });
  EXPECT_EQ (hintedButton (PilotLevel::Off, AppView::Fpv, buildingToADrop (), true, games), -1);
  EXPECT_EQ (hintedButton (PilotLevel::Fly, AppView::Fpv, buildingToADrop (), true, games), -1);
  EXPECT_EQ (hintedButton (PilotLevel::Hint, AppView::Full, buildingToADrop (), true, games), -1);
  EXPECT_EQ (hintedButton (PilotLevel::Hint, AppView::Fpv, buildingToADrop (), false, games), -1);
}

TEST (PilotHint, OnlyAnActionCarryingAFittingGameLights)
{
  auto const none = withGames ({ { 0, PilotGame::None }, { 2, PilotGame::CallAndResponse } });
  EXPECT_EQ (hintedButton (PilotLevel::Hint, AppView::Fpv, buildingToADrop (), true, none), -1);
  EXPECT_EQ (hintedButton (PilotLevel::Hint, AppView::Fpv, buildingToADrop (), true, Games{}), -1);
  EXPECT_EQ (hintedButton (PilotLevel::Hint, AppView::Fpv, FittingGames{}, true,
                           withGames ({ { 0, PilotGame::FakeOut } })),
             -1);
}

TEST (PilotHint, AHintedPadPulsesOnTheBeatInThePilotsColour)
{
  auto const lit = padHintColour (0, 4);
  EXPECT_EQ (lit, toColour (theme ().notice));
  EXPECT_EQ (padHintColour (4, 4), lit);
  for (auto const step : { 1, 2, 3, 5 })
    EXPECT_NE (padHintColour (step, 4), lit) << step;
  EXPECT_EQ (padHintColour (1, 4), padHintColour (2, 4)) << "one shade between the beats";
}

TEST (PilotHint, ThePadsPagePaintsAHintedPad)
{
  ControllerComponent page;
  page.setBounds (0, 0, 768, 340);
  auto const hint = padHintColour (0, 4);
  page.setPadColour (1, padIndexForAction (2), hint);
  auto const image = page.createComponentSnapshot (page.getLocalBounds ());
  if (auto const dir = std::getenv ("A3_SNAPSHOT_DIR"))
    {
      juce::File (dir).getChildFile ("pads-hinted.png").deleteFile ();
      juce::FileOutputStream out (juce::File (dir).getChildFile ("pads-hinted.png"));
      juce::PNGImageFormat ().writeImageToStream (image, out);
    }
  auto found = false;
  for (auto y = 0; y < image.getHeight () && !found; ++y)
    for (auto x = 0; x < image.getWidth () && !found; ++x)
      {
        auto const p = image.getPixelAt (x, y);
        found = std::abs (p.getRed () - hint.getRed ()) <= 2
                && std::abs (p.getGreen () - hint.getGreen ()) <= 2
                && std::abs (p.getBlue () - hint.getBlue ()) <= 2;
      }
  EXPECT_TRUE (found);
}

TEST (PilotHint, TheBuildHintGoesDarkAsTheCurrentBarNearsTheDrop)
{
  MusicCue cue;
  cue.section = MusicSection::Build;
  cue.next = MusicSection::Drop;
  cue.changeBar = 16;
  cue.energy = 1.f;
  GameTuning const tuning;
  auto const games = withGames ({ { 0, PilotGame::FakeOut } });
  auto const at = [&] (long long bar) {
    return hintedButton (PilotLevel::Hint, AppView::Fpv, fittingGames (cue, bar, 4, tuning), true,
                         games);
  };
  EXPECT_EQ (at (10), 0);
  EXPECT_EQ (at (15), -1) << "one bar left: too late for a fake-out";
}
