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

#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/components/fpv/GameLabel.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

using namespace a3;

TEST (GameLabel, EveryGameHasItsOwnShortWordAndNoneHasNone)
{
  EXPECT_EQ (gameWord (PilotGame::FakeOut), "FAKE-OUT");
  EXPECT_EQ (gameWord (PilotGame::Formation), "FORMATION");
  EXPECT_EQ (gameWord (PilotGame::HideAndSeek), "HIDE");
  EXPECT_EQ (gameWord (PilotGame::CallAndResponse), "CALL");
  EXPECT_TRUE (gameWord (PilotGame::None).isEmpty ());
}

TEST (GameLabel, APilotsGameIsDimmerAndCarriesTheTag)
{
  auto const dj = gameInk (false);
  auto const pilot = gameInk (true);
  EXPECT_FLOAT_EQ (dj.word, 1.f);
  EXPECT_FALSE (dj.showsTag);
  EXPECT_TRUE (pilot.showsTag);
  EXPECT_LT (pilot.word, dj.word);
  EXPECT_LT (pilot.tag, pilot.word) << "the tag is quieter than the word it follows";
}

namespace
{
FlightBodies
bodiesWithIds (std::initializer_list<int> ids)
{
  FlightBodies bodies;
  for (auto id : ids)
    bodies.body[static_cast<size_t> (bodies.count++)].id = id;
  return bodies;
}

std::array<std::optional<ShipGame>, fpvShips>
noGames ()
{
  return {};
}
}

TEST (GameLabel, OnlyTheLeaderDrawsALineToTheTarget)
{
  auto games = noGames ();
  // A formation of three on group 2: ship 1 leads, 0 and 3 are recruited.
  games[1] = ShipGame{ PilotGame::Formation, false, 1, 2 };
  games[0] = ShipGame{ PilotGame::Formation, false, 1, 2 };
  games[3] = ShipGame{ PilotGame::Formation, false, 1, 2 };

  auto const lines = gameLines (games, bodiesWithIds ({ 0, 2 }));

  EXPECT_FALSE (lines[0].has_value ());
  ASSERT_TRUE (lines[1].has_value ());
  EXPECT_EQ (*lines[1], 2);
  EXPECT_FALSE (lines[2].has_value ());
  EXPECT_FALSE (lines[3].has_value ());
}

TEST (GameLabel, NoLineForAGameWithoutATargetOrWithAGroupThatIsGone)
{
  auto games = noGames ();
  games[0] = ShipGame{ PilotGame::FakeOut, false, 0, noBodyId };
  games[2] = ShipGame{ PilotGame::HideAndSeek, true, 2, 5 };

  auto const lines = gameLines (games, bodiesWithIds ({ 0, 1 }));

  EXPECT_FALSE (lines[0].has_value ());
  EXPECT_FALSE (lines[2].has_value ());
}

TEST (GameLabel, TheBoxSitsCentredUnderTheShip)
{
  juce::Rectangle<float> const floor{ 0.f, 0.f, 400.f, 400.f };
  auto const box = gameLabelBox ({ 200.f, 200.f }, 40.f, { 60.f, 10.f }, floor);
  EXPECT_FLOAT_EQ (box.getCentreX (), 200.f);
  EXPECT_GE (box.getY (), 200.f + 20.f) << "below the ship's tail";
  EXPECT_FLOAT_EQ (box.getWidth (), 60.f);
}

TEST (GameLabel, TheBoxIsKeptInsideTheFloor)
{
  juce::Rectangle<float> const floor{ 0.f, 0.f, 400.f, 400.f };
  auto const left = gameLabelBox ({ 5.f, 200.f }, 40.f, { 60.f, 10.f }, floor);
  EXPECT_GE (left.getX (), floor.getX ());
  auto const right = gameLabelBox ({ 398.f, 200.f }, 40.f, { 60.f, 10.f }, floor);
  EXPECT_LE (right.getRight (), floor.getRight ());
  auto const bottom = gameLabelBox ({ 200.f, 395.f }, 40.f, { 60.f, 10.f }, floor);
  EXPECT_LE (bottom.getBottom (), floor.getBottom ());
}

TEST (GameLabel, ABoxLargerThanTheFloorStillAnswers)
{
  juce::Rectangle<float> const floor{ 0.f, 0.f, 20.f, 20.f };
  auto const box = gameLabelBox ({ 10.f, 10.f }, 40.f, { 60.f, 10.f }, floor);
  EXPECT_TRUE (std::isfinite (box.getX ()));
  EXPECT_TRUE (std::isfinite (box.getY ()));
}

namespace
{
constexpr int side = 300;

int
pixelsAwayFromBackground (juce::Image const &image)
{
  auto const bg = toColour (theme ().background);
  auto count = 0;
  for (int y = 0; y < image.getHeight (); ++y)
    for (int x = 0; x < image.getWidth (); ++x)
      count += image.getPixelAt (x, y) != bg ? 1 : 0;
  return count;
}

/** Summed distance from the background: grows with ink, which a pixel count
 *  cannot tell once the same pixels are merely paler. */
int
inkAwayFromBackground (juce::Image const &image)
{
  auto const bg = toColour (theme ().background);
  auto sum = 0;
  for (int y = 0; y < image.getHeight (); ++y)
    for (int x = 0; x < image.getWidth (); ++x)
      {
        auto const px = image.getPixelAt (x, y);
        sum += std::abs (px.getRed () - bg.getRed ()) + std::abs (px.getGreen () - bg.getGreen ())
               + std::abs (px.getBlue () - bg.getBlue ());
      }
  return sum;
}

struct Painter
{
  Painter () { juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel); }
  ~Painter () { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

  juce::Image
  paint (PilotGame game, bool byPilot, float alpha = 1.f) const
  {
    juce::Image image (juce::Image::ARGB, side, side, true);
    juce::Graphics g (image);
    g.fillAll (toColour (theme ().background));
    GameLabelPaint label;
    label.shipCentre = { 150.f, 120.f };
    label.shipLength = 40.f;
    label.colour = toColour (theme ().channel[1]);
    label.game = game;
    label.byPilot = byPilot;
    label.alpha = alpha;
    label.fontHeight = 14.f;
    label.floor = { 0.f, 0.f, float (side), float (side) };
    paintGameLabel (g, label);
    return image;
  }

  LookAndFeel_A3 lookAndFeel;
};
}

TEST (GameLabelPaint, AShipInAGameGetsItsWordPainted)
{
  Painter p;
  EXPECT_GT (pixelsAwayFromBackground (p.paint (PilotGame::Formation, false)), 0);
}

TEST (GameLabelPaint, AShipInNoGamePaintsNothing)
{
  Painter p;
  EXPECT_EQ (pixelsAwayFromBackground (p.paint (PilotGame::None, false)), 0);
}

TEST (GameLabelPaint, APilotsGameAddsTheTagAndIsDimmer)
{
  Painter p;
  auto const dj = p.paint (PilotGame::HideAndSeek, false);
  auto const pilot = p.paint (PilotGame::HideAndSeek, true);
  EXPECT_GT (pixelsAwayFromBackground (pilot), pixelsAwayFromBackground (dj))
      << "the AUTO tag adds ink";
}

TEST (GameLabelPaint, EveryGamePaintsWithoutCrashingOnATinyFloor)
{
  Painter p;
  juce::Image image (juce::Image::ARGB, 30, 30, true);
  juce::Graphics g (image);
  for (auto game : { PilotGame::FakeOut, PilotGame::Formation, PilotGame::HideAndSeek,
                     PilotGame::CallAndResponse, PilotGame::None })
    {
      GameLabelPaint label;
      label.shipCentre = { 15.f, 15.f };
      label.shipLength = 20.f;
      label.game = game;
      label.byPilot = true;
      label.fontHeight = 10.f;
      label.floor = { 0.f, 0.f, 30.f, 30.f };
      paintGameLabel (g, label);
    }
  SUCCEED ();
}

TEST (GameLabelPaint, AFadingLabelHasLessInkAndAGoneOneHasNone)
{
  Painter p;
  auto const full = inkAwayFromBackground (p.paint (PilotGame::Formation, false, 1.f));
  auto const half = inkAwayFromBackground (p.paint (PilotGame::Formation, false, 0.5f));
  EXPECT_GT (full, half);
  EXPECT_GT (half, 0);
  EXPECT_EQ (pixelsAwayFromBackground (p.paint (PilotGame::Formation, true, 0.f)), 0);
}

namespace
{
ShipGame
gameOfLeader (PilotGame game, int leader, bool byPilot = true)
{
  ShipGame g;
  g.game = game;
  g.leader = leader;
  g.byPilot = byPilot;
  return g;
}
constexpr int bar = 4;
}

TEST (HeldGame, ARunningGameIsShownInFullInk)
{
  auto const now = gameOfLeader (PilotGame::Formation, 2);
  auto const held = heldGame (now, std::nullopt, 0.0, 10.0, bar);
  ASSERT_TRUE (held.game);
  EXPECT_EQ (held.game->game, PilotGame::Formation);
  EXPECT_FLOAT_EQ (held.alpha, 1.f);
}

TEST (HeldGame, AnEndedGameStaysAndFadesLinearlyOverOneBar)
{
  auto const last = gameOfLeader (PilotGame::CallAndResponse, 1);
  auto const start = heldGame (std::nullopt, last, 8.0, 8.0, bar);
  ASSERT_TRUE (start.game);
  EXPECT_EQ (start.game->game, PilotGame::CallAndResponse);
  EXPECT_FLOAT_EQ (start.alpha, 1.f);
  EXPECT_NEAR (heldGame (std::nullopt, last, 8.0, 10.0, bar).alpha, 0.5f, 1e-5f);
  EXPECT_NEAR (heldGame (std::nullopt, last, 8.0, 11.0, bar).alpha, 0.25f, 1e-5f);
}

TEST (HeldGame, AfterTheBarNothingIsLeft)
{
  auto const last = gameOfLeader (PilotGame::HideAndSeek, 0);
  EXPECT_FALSE (heldGame (std::nullopt, last, 8.0, 12.0, bar).game);
  EXPECT_FALSE (heldGame (std::nullopt, last, 8.0, 40.0, bar).game);
}

TEST (HeldGame, NothingBeforeAnyGameAndNothingForNone)
{
  EXPECT_FALSE (heldGame (std::nullopt, std::nullopt, 0.0, 1.0, bar).game);
  auto const none = gameOfLeader (PilotGame::None, 0);
  EXPECT_FALSE (heldGame (none, std::nullopt, 0.0, 1.0, bar).game);
  EXPECT_FALSE (heldGame (std::nullopt, none, 0.0, 1.0, bar).game);
}

TEST (HeldGame, ABeatClockThatRanBackwardsOrABadBarKeepsTheLabelOut)
{
  auto const last = gameOfLeader (PilotGame::FakeOut, 3);
  EXPECT_FALSE (heldGame (std::nullopt, last, 8.0, 4.0, bar).game);
  EXPECT_FALSE (heldGame (std::nullopt, last, 8.0, 9.0, 0).game);
}

TEST (SameGame, ComparesWhichWhoLeadsAndWhoStartedIt)
{
  auto const a = gameOfLeader (PilotGame::Formation, 1);
  EXPECT_TRUE (sameGame (a, a));
  EXPECT_TRUE (sameGame (std::nullopt, std::nullopt));
  EXPECT_FALSE (sameGame (a, std::nullopt));
  EXPECT_FALSE (sameGame (a, gameOfLeader (PilotGame::HideAndSeek, 1)));
  EXPECT_FALSE (sameGame (a, gameOfLeader (PilotGame::Formation, 2)));
  EXPECT_FALSE (sameGame (a, gameOfLeader (PilotGame::Formation, 1, false)));
}

TEST (GameJournal, StartNamesWordStarterLeaderAndTarget)
{
  auto g = gameOfLeader (PilotGame::CallAndResponse, 1);
  g.target = 5;
  EXPECT_EQ (gameStartLine (2, g), "A3 Motion: game start ch3 CALL by pilot leader ch2 target G6");
  auto dj = gameOfLeader (PilotGame::Formation, 0, false);
  dj.target = noBodyId;
  EXPECT_EQ (gameStartLine (0, dj), "A3 Motion: game start ch1 FORMATION by dj leader ch1 target none");
}

TEST (GameJournal, EndNamesChannelAndWord)
{
  EXPECT_EQ (gameEndLine (2, PilotGame::HideAndSeek), "A3 Motion: game end ch3 HIDE");
}
