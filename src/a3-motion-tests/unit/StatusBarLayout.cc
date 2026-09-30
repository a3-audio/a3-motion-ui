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

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/StatusBarLayout.hh>

using namespace a3;

namespace
{
// The device's own screen, and the band the bar actually lays out in: the
// height it asks for at the shipped skin, less the sixth it keeps free above
// and below. The keys at the ends are the layout's own since 2026-09-26, so
// the band is handed over whole. Written the way StatusBar::resized() arrives
// at it rather than as round numbers, so the test moves with the bar rather
// than having to be re-measured.
//
// The height is the floor rather than a size read off a screenshot.
// preferredHeight() is jmax(minimumRowHeight, header * 1.875), and the
// tallest header any of the nineteen shipped skins asks for is 18.0, which
// comes to 33 -- so every skin on the device lands on the floor and this bar
// is 35 pixels tall whichever one is chosen. It stood at 39 and said it was
// the shipped skin's height, which no skin has ever produced.
constexpr int deviceWidth = 768;
constexpr int barHeight = static_cast<int> (minimumRowHeight);
constexpr int padding = 4;

juce::Rectangle<int>
rowOf (int width, int height)
{
  auto row = juce::Rectangle<int> (0, 0, width, height);
  auto const verticalPadding = height / 6;
  row.removeFromTop (verticalPadding);
  row.removeFromBottom (verticalPadding);

  return row;
}

StatusBarLayout
deviceLayout ()
{
  return statusBarLayout (rowOf (deviceWidth, barHeight), deviceWidth,
                          padding);
}

}

// A narrower screen than the device's still keeps both readings, the beat
// display and all four keys.
TEST (StatusBarLayout, AShortBarStillKeepsItsReadings)
{
  auto const narrow = 600;
  auto const l = statusBarLayout (rowOf (narrow, barHeight), narrow, padding);

  EXPECT_FALSE (l.tick.isEmpty ());
  EXPECT_FALSE (l.bpm.isEmpty ());
  EXPECT_FALSE (l.readout.isEmpty ());
  for (auto const &key : { l.clockKey, l.deckKey, l.workspacesKey, l.cleanKey,
                           l.keyboardKey, l.menuKey })
    EXPECT_FALSE (key.isEmpty ());
}

// The one thing about this bar that has to be true whatever the screen: the
// beat display is centred on the *bar*, not on what the two readings leave
// over. It is the one thing here that is looked at rather than read, and an
// off-centre one reads as a mistake.
//
// This used to be the harder claim, because two blocks of meters were pushing
// in from either side and the row is not symmetrical -- the right end gives
// up two icon squares. With them gone it is one line again.
TEST (StatusBarLayout, TheBeatDisplayIsCentredOnTheBar)
{
  auto const l = deviceLayout ();
  EXPECT_EQ (l.tick.getCentreX (), deviceWidth / 2);
}

// Left of the beat display, in reading order: the clock's key, the tempo, and
// what was last done -- the readout stands against the display since
// 2026-09-26, so the right end is keys only.
TEST (StatusBarLayout, TheReadingsStandLeftOfTheBeatDisplay)
{
  auto const l = deviceLayout ();

  EXPECT_LE (l.clockKey.getRight (), l.bpm.getX ());
  EXPECT_LE (l.bpm.getRight (), l.readout.getX ());
  EXPECT_LE (l.readout.getRight (), l.tick.getX ());
  EXPECT_GT (l.readout.getWidth (), l.bpm.getWidth ())
      << "the readout says more than the tempo does";
}

// CLOCK leads the row, left of the tempo it decides.
TEST (StatusBarLayout, TheClockKeyLeadsTheRow)
{
  auto const row = rowOf (deviceWidth, barHeight);
  auto const l = deviceLayout ();

  EXPECT_EQ (l.clockKey.getX (), row.getX ());
  EXPECT_EQ (l.clockKey.getWidth (), l.menuKey.getWidth ());
}

// CLEAN, the on-screen keyboard and MENU close the row, one size and one look,
// MENU at the very end.
TEST (StatusBarLayout, TheThreeKeysStandBeforeTheSwitchInOneSize)
{
  auto const row = rowOf (deviceWidth, barHeight);
  auto const l = deviceLayout ();

  EXPECT_LE (l.menuKey.getRight (), l.deckKey.getX ());
  EXPECT_LE (l.keyboardKey.getRight (), l.menuKey.getX ());
  EXPECT_LE (l.cleanKey.getRight (), l.keyboardKey.getX ());

  for (auto const &key : { l.cleanKey, l.keyboardKey })
    EXPECT_EQ (key.getWidth (), l.menuKey.getWidth ());

  // Wide enough for a word, and never under the row's height.
  EXPECT_GE (l.menuKey.getWidth (), row.getHeight ());
  EXPECT_EQ (l.menuKey.getHeight (), row.getHeight ());
}

// The workspace switch closes the row, where StemDeck has it (asked for on
// 2026-09-30: "das irritiert sonst wenn man workspace switcht und der button
// springt"): STEMDECK, then the arrow, at the very right. The same numbers as
// StemDeck's top bar -- at 768 px, the arrow 30 wide and 6 from the edge,
// the app key 80 wide and 2 left of it.
TEST (StatusBarLayout, TheSwitchClosesTheRowWhereStemDeckHasIt)
{
  auto const l = deviceLayout ();

  EXPECT_EQ (l.workspacesKey.getRight (), deviceWidth - 6);
  EXPECT_EQ (l.workspacesKey.getWidth (), 30);
  EXPECT_EQ (l.deckKey.getRight (), l.workspacesKey.getX () - 2);
  EXPECT_EQ (l.deckKey.getWidth (), 80);
  EXPECT_EQ (l.workspacesKey.getHeight (), l.menuKey.getHeight ());
}

// The beat display stays centred and gives way rather than running under a
// key -- on the device and on a narrower screen, where the keys reach further
// in.
TEST (StatusBarLayout, TheBeatDisplayNeverRunsUnderAKey)
{
  for (auto const width : { deviceWidth, 600 })
    {
      auto const l = statusBarLayout (rowOf (width, barHeight), width, padding);

      EXPECT_LE (l.tick.getRight (), l.cleanKey.getX ()) << width;
      EXPECT_EQ (l.tick.getCentreX (), width / 2) << width;
      EXPECT_FALSE (l.tick.isEmpty ()) << width;
    }
}

// Nothing at all is not a layout. An empty row has to come back empty rather
// than with rectangles placed off the edge of it, since resized() is called
// once before the bar has been given a size.
TEST (StatusBarLayout, AnEmptyRowLaysOutNothing)
{
  auto const l = statusBarLayout ({}, 0, padding);

  EXPECT_TRUE (l.tick.isEmpty ());
  EXPECT_TRUE (l.bpm.isEmpty ());
  EXPECT_TRUE (l.readout.isEmpty ());
  EXPECT_TRUE (l.menuKey.isEmpty ());
}
