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
// and below, less the two icon squares at its right end. Written the way
// StatusBar::resized() arrives at it rather than as three round numbers, so
// the test moves with the bar rather than having to be re-measured.
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

  auto const iconWidth = static_cast<int> (row.getHeight () * 1.5f);
  row.removeFromRight (iconWidth);
  row.removeFromRight (iconWidth);

  return row;
}

StatusBarLayout
deviceLayout ()
{
  return statusBarLayout (rowOf (deviceWidth, barHeight), deviceWidth,
                          padding);
}

}

// The bar carried nine VU meters until 2026-09-12 and most of this suite was
// about the width they took from the two readings. They are gone -- the four
// inputs to the channel faces below as a dot each, the five outputs to the
// MIX page, where the same five have always been -- and what is left is what
// the bar was before them: two readings and the beat display between them.
TEST (StatusBarLayout, AShortBarStillKeepsItsReadings)
{
  auto const narrow = 220;
  auto const l = statusBarLayout (rowOf (narrow, barHeight), narrow, padding);

  EXPECT_FALSE (l.tick.isEmpty ());
  EXPECT_FALSE (l.bpm.isEmpty ());
  EXPECT_FALSE (l.readout.isEmpty ());
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

// The readings keep clear of the bar's ends, and of each other.
TEST (StatusBarLayout, TheTwoReadingsShareTheRow)
{
  auto const l = deviceLayout ();

  EXPECT_LT (l.bpm.getX (), l.readout.getX ());
  EXPECT_LE (l.bpm.getRight (), l.readout.getX ());
  EXPECT_GT (l.bpm.getX (), 0);
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
}
