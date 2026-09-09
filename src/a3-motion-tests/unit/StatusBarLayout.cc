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

std::vector<juce::Rectangle<int> >
everyMeter (StatusBarLayout const &l)
{
  std::vector<juce::Rectangle<int> > meters;
  for (auto const &bar : l.inputMeters)
    meters.push_back (bar);
  for (auto const &bar : l.outputMeters)
    meters.push_back (bar);
  return meters;
}
}

// The arrangement the maintainer asked for, and the only thing about this bar
// that has to be true whatever the screen: the inputs stand left of the beat
// display and the outputs right of it. A block on the wrong side would still
// be four bars and a level, and would still be read as the wrong four.
TEST (StatusBarLayout, TheInputsStandLeftOfTheIndicatorAndTheOutputsRight)
{
  auto const l = deviceLayout ();

  ASSERT_FALSE (l.tick.isEmpty ());

  for (auto const &bar : l.inputMeters)
    {
      ASSERT_FALSE (bar.isEmpty ());
      EXPECT_LE (bar.getRight (), l.tick.getX ())
          << "input meter " << bar.toString () << " reaches into the indicator "
          << l.tick.toString ();
    }

  for (auto const &bar : l.outputMeters)
    {
      ASSERT_FALSE (bar.isEmpty ());
      EXPECT_GE (bar.getX (), l.tick.getRight ())
          << "output meter " << bar.toString ()
          << " reaches into the indicator " << l.tick.toString ();
    }
}

// The bar is always on screen and every other thing on it is read rather than
// watched, so a meter laid over one of them would cost a reading permanently
// and for the sake of a bar a few pixels wide. The icons are outside the row
// this is handed, so staying inside the row is what keeps clear of them.
TEST (StatusBarLayout, NoMeterTouchesALabelTheIndicatorOrTheIconsBesideThem)
{
  auto const row = rowOf (deviceWidth, barHeight);
  auto const l = statusBarLayout (row, deviceWidth, padding);

  for (auto const &bar : everyMeter (l))
    {
      EXPECT_TRUE (row.contains (bar))
          << bar.toString () << " outside the bar's own row "
          << row.toString ();
      EXPECT_TRUE (bar.getIntersection (l.tick).isEmpty ())
          << bar.toString () << " overlaps the indicator";
      EXPECT_TRUE (bar.getIntersection (l.bpm).isEmpty ())
          << bar.toString () << " overlaps the tempo reading";
      EXPECT_TRUE (bar.getIntersection (l.readout).isEmpty ())
          << bar.toString () << " overlaps the control readout";
    }

  EXPECT_FALSE (l.bpm.isEmpty ());
  EXPECT_FALSE (l.readout.isEmpty ());
}

// The two rectangles a repaint is clipped to. The status bar never goes away,
// so a refresh that took the whole bar would redraw the indicator and two
// labels twenty-five times a second for the life of the device -- and a block
// that did not contain its own bars would leave a stale one on screen.
TEST (StatusBarLayout, EachBlockCoversItsOwnBarsAndNothingElse)
{
  auto const l = deviceLayout ();

  ASSERT_FALSE (l.inputBlock.isEmpty ());
  ASSERT_FALSE (l.outputBlock.isEmpty ());

  for (auto const &bar : l.inputMeters)
    EXPECT_TRUE (l.inputBlock.contains (bar))
        << bar.toString () << " outside the input block "
        << l.inputBlock.toString ();

  for (auto const &bar : l.outputMeters)
    EXPECT_TRUE (l.outputBlock.contains (bar))
        << bar.toString () << " outside the output block "
        << l.outputBlock.toString ();

  EXPECT_TRUE (l.inputBlock.getIntersection (l.tick).isEmpty ());
  EXPECT_TRUE (l.outputBlock.getIntersection (l.tick).isEmpty ());
}

// Bars side by side, evenly stepped and never touching: two meters that met
// would read as one wide one, and an uneven step reads as a fault in the
// picture rather than as a level.
TEST (StatusBarLayout, TheBarsOfABlockAreEvenlyStepped)
{
  auto const l = deviceLayout ();

  auto const step = l.inputMeters[1].getX () - l.inputMeters[0].getX ();
  ASSERT_GT (step, 0);

  for (std::size_t i = 1; i < l.inputMeters.size (); ++i)
    {
      EXPECT_EQ (l.inputMeters[i].getX () - l.inputMeters[i - 1].getX (), step);
      EXPECT_LT (l.inputMeters[i - 1].getRight (), l.inputMeters[i].getX ());
    }

  for (std::size_t i = 1; i < l.outputMeters.size (); ++i)
    {
      EXPECT_EQ (l.outputMeters[i].getX () - l.outputMeters[i - 1].getX (),
                 step);
      EXPECT_LT (l.outputMeters[i - 1].getRight (), l.outputMeters[i].getX ());
    }
}

// The indicator gives width up rather than a label being dropped -- but only
// down to a share of what it would otherwise have had. It is the one thing on
// this bar that is looked at rather than read, and a beat display crushed to
// a stripe answers no question at all.
TEST (StatusBarLayout, TheIndicatorKeepsMostOfTheWidthItWouldHaveHad)
{
  auto const row = rowOf (deviceWidth, barHeight);
  auto const l = statusBarLayout (row, deviceWidth, padding);

  // What resized() gave it before the meters existed, taken from the same two
  // constants the layout is written against rather than restated as fractions
  // here. Restated, the two arithmetics differed by a pixel -- 694 * 2/5 is
  // 277 by integer division and 278 rounded -- and the test then disagreed
  // with the code over a rule both of them meant identically.
  auto const before = juce::jmin (
      juce::roundToInt (static_cast<float> (row.getWidth ())
                        * statusTickWidthOfRow),
      juce::roundToInt (static_cast<float> (deviceWidth)
                        * statusTickWidthOfBar));

  EXPECT_GE (l.tick.getWidth (),
             juce::roundToInt (static_cast<float> (before)
                               * statusMeterMinTickShare))
      << "the indicator came out at " << l.tick.getWidth () << " of " << before;
  EXPECT_LE (l.tick.getWidth (), before);

  // Still centred on the whole bar, not on what the labels left over: an
  // off-centre beat display reads as a mistake.
  EXPECT_NEAR (l.tick.getCentreX (), deviceWidth / 2, 1);
}

// A bar too narrow to hold nine meters and still leave the indicator and the
// two readings anything drops the meters, rather than shaving every one of
// them down to something unreadable. The meters are the addition here; the
// tempo, the beat and the readout were there first.
TEST (StatusBarLayout, AShortBarKeepsItsReadingsAndDropsTheMeters)
{
  auto const narrow = 220;
  auto const l = statusBarLayout (rowOf (narrow, barHeight), narrow, padding);

  for (auto const &bar : everyMeter (l))
    EXPECT_TRUE (bar.isEmpty ()) << bar.toString () << " drawn on a short bar";

  EXPECT_TRUE (l.inputBlock.isEmpty ());
  EXPECT_TRUE (l.outputBlock.isEmpty ());

  EXPECT_FALSE (l.tick.isEmpty ());
  EXPECT_FALSE (l.bpm.isEmpty ());
  EXPECT_FALSE (l.readout.isEmpty ());
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

  for (auto const &bar : everyMeter (l))
    EXPECT_TRUE (bar.isEmpty ());
}
