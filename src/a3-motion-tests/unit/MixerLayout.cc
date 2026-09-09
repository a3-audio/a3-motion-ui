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

#include <a3-motion-ui/components/BarFader.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/MixerLayout.hh>

using namespace a3;

namespace
{
constexpr ControlMetrics metrics{ fingertipSize, 12.f, 12.f };

// A portrait area with room for four strips side by side. Expressed in
// thresholds rather than in the device's pixels: the layout is about
// proportions, and a test that quoted 768x1024 would have to be rewritten for
// the next screen.
juce::Rectangle<int>
aRoomyOverlay ()
{
  // Cast rather than braced: minimumChannelWidth and minimumMotionHeight are
  // floats, and a float in a braced initialiser narrows whatever its value --
  // the constant-expression exception runs the other way, from integer to
  // floating point.
  return juce::Rectangle<int> (
      0, 0, static_cast<int> (minimumChannelWidth * 6),
      static_cast<int> (minimumMotionHeight * 8));
}

// The bar's tab: one strip in a landscape band. Wider than the overlay's
// column and a quarter of its height, which is what turns the strip on its
// side -- and expressed in the same thresholds, for the same reason.
juce::Rectangle<int>
aBarStrip ()
{
  return juce::Rectangle<int> (0, 0,
                               static_cast<int> (minimumChannelWidth * 5),
                               static_cast<int> (minimumMotionHeight * 2));
}

// Too narrow for four strips side by side, and tall enough that breaking them
// is the only thing being asked. Built here rather than three times over, and
// parenthesised for the narrowing reason above.
juce::Rectangle<int>
aNarrowOverlay ()
{
  return juce::Rectangle<int> (0, 0,
                               static_cast<int> (minimumChannelWidth * 4) - 1,
                               static_cast<int> (minimumMotionHeight * 12));
}
}

// Every control of every channel has a rectangle, and none of them is empty.
TEST (MixerLayout, EveryControlOfEveryChannelGetsARectangle)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t channel = 0;
       channel < static_cast<std::size_t> (numChannelsInitial); ++channel)
    for (std::size_t i = 0; i < static_cast<std::size_t> (numMixerControls);
         ++i)
      EXPECT_FALSE (layout.controls[channel][i].isEmpty ())
          << channel << " " << mixerControlLabel (mixerControlOrder[i]);
}

// The order on screen is the order in the table. This is what makes the table
// the authority rather than a list that happens to agree.
TEST (MixerLayout, TheControlsAreInTheTablesOrderDownTheStrip)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t i = 1; i < static_cast<std::size_t> (numMixerControls);
       ++i)
    EXPECT_GE (layout.controls[0][i].getY (),
               layout.controls[0][i - 1].getBottom ())
        << mixerControlLabel (mixerControlOrder[i]) << " is out of order";
}

// Nothing overlaps anything, within a strip or between strips. A control drawn
// over another is a control that answers for the wrong channel.
TEST (MixerLayout, NoTwoControlsOverlap)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  std::vector<juce::Rectangle<int> > all;
  for (auto const &strip : layout.controls)
    for (auto const &control : strip)
      all.push_back (control);
  for (auto const &control : layout.master)
    all.push_back (control);
  for (auto const &control : layout.filter)
    all.push_back (control);

  for (std::size_t i = 0; i < all.size (); ++i)
    for (std::size_t j = i + 1; j < all.size (); ++j)
      EXPECT_FALSE (all[i].intersects (all[j])) << i << " over " << j;
}

// Hit in the dark, mid-set, with one hand.
TEST (MixerLayout, EveryControlIsAtLeastAFingertip)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (auto const &strip : layout.controls)
    for (auto const &control : strip)
      {
        EXPECT_GE (control.getWidth (), fingertipSize);
        EXPECT_GE (control.getHeight (), fingertipSize);
      }
}

// The channel volume is thrown, and a throw that cannot move is not one.
// Asserted on the fader's own geometry rather than on the cell's proportions:
// what matters is not how the cell is shaped but whether the control moves.
TEST (MixerLayout, TheVolumeCellGivesTheFaderARealThrow)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const slot
      = static_cast<std::size_t> (controlSlot (MixerControl::Volume));

  for (auto const &strip : layout.controls)
    {
      auto const bottom = faderGeometry (strip[slot], metrics, 0.f).cap;
      auto const top = faderGeometry (strip[slot], metrics, 1.f).cap;
      EXPECT_GT (bottom.getY (), top.getY ());
    }
}

// And the room for it is in the layout, where a reader can see the intent,
// rather than only in the fader that happens to fit.
TEST (MixerLayout, TheVolumeRowIsTallerThanTheRowsAroundIt)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const slot
      = static_cast<std::size_t> (controlSlot (MixerControl::Volume));
  ASSERT_GT (slot, std::size_t{ 0 });
  ASSERT_LT (slot + 1, static_cast<std::size_t> (numMixerControls));

  auto const &strip = layout.controls[0];
  EXPECT_GT (strip[slot].getHeight (), strip[slot - 1].getHeight ());
  EXPECT_GT (strip[slot].getHeight (), strip[slot + 1].getHeight ());
}

// Narrow enough and the strips break into two by two rather than four thin
// ones -- the whole point of ColumnBreak, exercised through the layout that
// uses it.
TEST (MixerLayout, ANarrowOverlayBreaksTheStripsIntoTwoByTwo)
{
  auto const layout = layOutMixerOverlay (aNarrowOverlay (), metrics);

  EXPECT_EQ (layout.strips.columns, 2);
  EXPECT_EQ (layout.strips.rows, 2);
}

// Everything inside the area it was given.
TEST (MixerLayout, EverythingStaysInsideTheArea)
{
  auto const area = juce::Rectangle<int> (5, 9, minimumChannelWidth * 6,
                                          minimumMotionHeight * 8);
  auto const layout = layOutMixerOverlay (area, metrics);
  ASSERT_TRUE (layout.fits);

  for (auto const &strip : layout.controls)
    for (auto const &control : strip)
      EXPECT_TRUE (area.contains (control));
  for (auto const &control : layout.master)
    EXPECT_TRUE (area.contains (control));
}

// An area too small to lay out says so, rather than handing back targets
// nobody can hit. The caller draws a short line of text instead.
TEST (MixerLayout, AnAreaTooSmallSaysSo)
{
  auto const layout = layOutMixerOverlay (
      { 0, 0, fingertipSize, fingertipSize }, metrics);
  EXPECT_FALSE (layout.fits);
}

// resized() is called with an empty rectangle before the window has a size.
TEST (MixerLayout, AnEmptyAreaDoesNotDivideByZero)
{
  auto const layout = layOutMixerOverlay ({}, metrics);
  EXPECT_FALSE (layout.fits);
}

// The bar's tab has one strip and three times the width, so it lays the
// controls across rather than down. The table's order still holds -- left to
// right is the reading order there.
TEST (MixerLayout, TheBarsStripReadsAcrossInTheTablesOrder)
{
  auto const layout = layOutMixerStrip (aBarStrip (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t i = 1; i < static_cast<std::size_t> (numMixerControls);
       ++i)
    EXPECT_GE (layout.controls[0][i].getX (),
               layout.controls[0][i - 1].getRight ())
        << mixerControlLabel (mixerControlOrder[i]) << " is out of order";
}

// One channel, so the other three strips are empty rather than laid out
// somewhere off screen.
TEST (MixerLayout, TheBarsStripLaysOutOneChannelOnly)
{
  auto const layout = layOutMixerStrip (aBarStrip (), metrics);

  EXPECT_EQ (layout.strips.columns, 1);
  for (std::size_t channel = 1;
       channel < static_cast<std::size_t> (numChannelsInitial); ++channel)
    for (auto const &control : layout.controls[channel])
      EXPECT_TRUE (control.isEmpty ()) << channel;
}

// The master and the filter are not here: the tab is about one channel, and
// the whole mixer is one tap away on the MIX key in the status bar.
TEST (MixerLayout, TheBarsStripCarriesNoMasterAndNoFilter)
{
  auto const layout = layOutMixerStrip (aBarStrip (), metrics);

  for (auto const &control : layout.master)
    EXPECT_TRUE (control.isEmpty ());
  for (auto const &control : layout.filter)
    EXPECT_TRUE (control.isEmpty ());
}

// Still a fingertip, at the bar's height rather than the overlay's.
TEST (MixerLayout, TheBarsStripKeepsEveryControlAtAFingertip)
{
  auto const layout = layOutMixerStrip (aBarStrip (), metrics);
  ASSERT_TRUE (layout.fits);

  for (auto const &control : layout.controls[0])
    {
      EXPECT_GE (control.getWidth (), fingertipSize);
      EXPECT_GE (control.getHeight (), fingertipSize);
    }
}

// The master is the fifth strip, not a row under the four: its column stands
// to the right of every channel's control, so the eye runs across five levels
// instead of jumping between two arrangements.
TEST (MixerLayout, TheMasterStandsAsAColumnRightOfTheChannels)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (auto const &control : layout.master)
    {
      ASSERT_FALSE (control.isEmpty ());
      for (auto const &strip : layout.controls)
        for (auto const &cell : strip)
          EXPECT_GE (control.getX (), cell.getRight ())
              << "the master's column runs into a channel's";
    }
}

// The assurance the whole change stands on. Five vertical strips are five
// levels on one line; a master volume half a row off the channels' faders is
// four faders and a stray one, which is the arrangement this replaced.
TEST (MixerLayout, TheMastersVolumeSitsOnTheChannelsFaderLine)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const channelSlot
      = static_cast<std::size_t> (controlSlot (MixerControl::Volume));
  auto const masterSlot
      = static_cast<std::size_t> (controlSlot (MasterControl::Volume));
  auto const master = layout.master[masterSlot];

  for (auto const &strip : layout.controls)
    {
      EXPECT_EQ (master.getY (), strip[channelSlot].getY ());
      EXPECT_EQ (master.getHeight (), strip[channelSlot].getHeight ());
    }
}

// The master's level is thrown like the four beside it, so its row has to
// hold a throw as theirs does -- asked of the fader's own geometry rather
// than of the cell's proportions, the way the channels' is.
TEST (MixerLayout, TheMastersVolumeCellGivesTheFaderARealThrow)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const cell = layout.master[static_cast<std::size_t> (
      controlSlot (MasterControl::Volume))];
  auto const bottom = faderGeometry (cell, metrics, 0.f).cap;
  auto const top = faderGeometry (cell, metrics, 1.f).cap;
  EXPECT_GT (bottom.getY (), top.getY ());
}

// Five controls against a channel's seven, and the two rows that leaves are
// left empty on purpose: the output level meters go there. A layout that
// filled them now would have to be undone.
TEST (MixerLayout, TheMasterLeavesTheRowsBelowItsFaderEmpty)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const faderRow = layout.controls[0][static_cast<std::size_t> (
      controlSlot (MixerControl::Volume))];

  for (auto const &control : layout.master)
    EXPECT_LE (control.getBottom (), faderRow.getBottom ())
        << "the master reaches into the rows the meters are waiting for";
}

// It is neither channel nor master, and it is the one global control touched
// constantly mid-set: a row of its own under all five columns, so three
// controls get the full width and the biggest targets on the page.
TEST (MixerLayout, TheFilterRunsUnderAllFiveColumns)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  auto span = layout.filter.front ();
  for (auto const &control : layout.filter)
    {
      ASSERT_FALSE (control.isEmpty ());
      span = span.getUnion (control);
    }

  std::vector<juce::Rectangle<int> > columns;
  for (auto const &strip : layout.controls)
    columns.push_back (strip.front ());
  columns.push_back (layout.master.front ());

  for (auto const &column : columns)
    {
      EXPECT_LT (span.getX (), column.getCentreX ());
      EXPECT_GT (span.getRight (), column.getCentreX ());
      EXPECT_GE (span.getY (), column.getBottom ())
          << "the filter is not under the columns";
    }
}

// The break is asked for the four channels, never for five. Five would halve
// to two columns, and two columns of five is three rows -- six cells for five
// strips, with cellIn admitting an index that stands for nothing.
TEST (MixerLayout, TheBreakHoldsExactlyTheFourChannels)
{
  auto const roomy = layOutMixerOverlay (aRoomyOverlay (), metrics);
  EXPECT_EQ (roomy.strips.columns * roomy.strips.rows, numChannelsInitial);

  auto const narrow = layOutMixerOverlay (aNarrowOverlay (), metrics);
  EXPECT_EQ (narrow.strips.columns * narrow.strips.rows, numChannelsInitial);
}

// Narrow, the four fall into a 2x2 block and the master keeps standing beside
// them -- which is what taking its width off first buys, rather than a
// five-way split coming apart.
TEST (MixerLayout, ANarrowOverlayLeavesTheMasterBesideTheTwoByTwo)
{
  auto const layout = layOutMixerOverlay (aNarrowOverlay (), metrics);

  EXPECT_EQ (layout.strips.columns, 2);
  EXPECT_EQ (layout.strips.rows, 2);

  for (auto const &control : layout.master)
    {
      ASSERT_FALSE (control.isEmpty ());
      for (auto const &strip : layout.controls)
        for (auto const &cell : strip)
          EXPECT_GE (control.getX (), cell.getRight ());
    }
}

// Hit in the dark like everything else on the page.
TEST (MixerLayout, EveryMasterControlIsAtLeastAFingertip)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (auto const &control : layout.master)
    {
      EXPECT_GE (control.getWidth (), fingertipSize);
      EXPECT_GE (control.getHeight (), fingertipSize);
    }
}
