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

#include <set>

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

// Wide enough that the four strips still stand side by side and each row of
// them clears its floor, and no wider: a column at the break's own minimum
// leaves the two keys half of what is left after the meter, which is where
// they fall under a fingertip. The one size at which the row fits and the
// fields in it do not.
juce::Rectangle<int>
aTightOverlay ()
{
  return juce::Rectangle<int> (0, 0,
                               static_cast<int> (minimumChannelWidth * 5),
                               static_cast<int> (minimumMotionHeight * 8));
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
    {
      auto const previous = layout.controls[0][i - 1];
      auto const control = layout.controls[0][i];

      // Down the strip, except across the one row two controls share -- the
      // same list read the way a row is read rather than a second order.
      if (control.getY () == previous.getY ())
        EXPECT_GE (control.getX (), previous.getRight ())
            << mixerControlLabel (mixerControlOrder[i]) << " is out of order";
      else
        EXPECT_GE (control.getY (), previous.getBottom ())
            << mixerControlLabel (mixerControlOrder[i]) << " is out of order";
    }
}

// PFL and FX are the only two of the seven that are pressed rather than
// turned, and a key asks for a fingertip rather than for a row of its own:
// they share the last row at half its width each. The row that frees goes to
// the six above, which is why this is a change of arrangement rather than a
// gap at the foot of the strip.
TEST (MixerLayout, TheTwoKeysShareTheLastRowOfAStrip)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const pfl = static_cast<std::size_t> (controlSlot (MixerControl::Pfl));
  auto const fx = static_cast<std::size_t> (controlSlot (MixerControl::Fx));

  for (auto const &strip : layout.controls)
    {
      EXPECT_EQ (strip[pfl].getY (), strip[fx].getY ());
      EXPECT_EQ (strip[pfl].getHeight (), strip[fx].getHeight ());
      EXPECT_LE (strip[pfl].getRight (), strip[fx].getX ())
          << "PFL is not left of FX";
      EXPECT_FALSE (strip[pfl].intersects (strip[fx]));

      EXPECT_GE (strip[pfl].getWidth (), fingertipSize);
      EXPECT_GE (strip[fx].getWidth (), fingertipSize);
    }
}

// Six rows for seven controls, and the six fill the strip. The row count
// stopped being the control count the moment two shared one: a layout still
// dividing the height by the control count would step six rows down a grid
// made for seven and leave the seventh standing empty.
TEST (MixerLayout, AStripHasOneRowFewerThanItHasControls)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  std::set<int> lines;
  for (auto const &control : layout.controls[0])
    lines.insert (control.getY ());

  EXPECT_EQ (lines.size (), static_cast<std::size_t> (numMixerControls - 1));

  // And the rows they stand on cover the strip's whole length, to within the
  // remainder an integer row height leaves against the bottom edge.
  auto const rows = static_cast<int> (lines.size ());
  auto const covered = layout.controls[0][0].getHeight () * rows;
  EXPECT_GE (covered, layout.channelMeter[0].getHeight () - rows);
}

// The trap the half-width keys bring: a column wide enough to carry a row can
// still be too narrow to carry two fields across it, and a key under a
// fingertip has to make the page say so rather than shrink quietly.
TEST (MixerLayout, AColumnTooNarrowForTwoKeysSideBySideSaysSo)
{
  auto const layout = layOutMixerOverlay (aTightOverlay (), metrics);

  // The rows themselves clear their floor -- it is only the fields across the
  // last of them that do not, which is exactly the case a check on the row
  // alone would let through.
  EXPECT_GE (layout.controls[0][0].getHeight (), fingertipSize);
  EXPECT_LT (layout.controls[0][static_cast<std::size_t> (
                                    controlSlot (MixerControl::Pfl))]
                 .getWidth (),
             fingertipSize);
  EXPECT_FALSE (layout.fits);
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

// Every row of a strip is the same size, and that is what makes the master's
// column line up with the channels' for free rather than by arrangement. The
// volume row used to be taller than the rows around it, because a fader
// needed somewhere to travel; nothing in the strip is thrown any more, so
// nothing has a claim on more of the column than its neighbours.
TEST (MixerLayout, EveryRowOfAStripIsTheSameHeight)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (auto const &strip : layout.controls)
    for (std::size_t i = 1; i < static_cast<std::size_t> (numMixerControls);
         ++i)
      EXPECT_EQ (strip[i].getHeight (), strip[0].getHeight ()) << i;
}

// The meter is a column of its own down the whole strip, not a cell inside one
// of its rows. Length is what a level meter is read by: a bar a seventh of the
// strip tall can say "loud" and nothing else, where one running the strip's
// full height says how much of the headroom is left. REAPER stands its meter
// beside the column of controls for exactly that reason.
TEST (MixerLayout, TheMeterRunsTheWholeHeightOfItsStrip)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t channel = 0;
       channel < static_cast<std::size_t> (numChannelsInitial); ++channel)
    {
      auto const meter = layout.channelMeter[channel];
      auto const &strip = layout.controls[channel];

      ASSERT_FALSE (meter.isEmpty ()) << channel;
      EXPECT_LE (meter.getY (), strip.front ().getY ())
          << channel << ": the meter starts below the strip's first control";
      EXPECT_GE (meter.getBottom (), strip.back ().getBottom ())
          << channel << ": the meter stops above the strip's last control";
    }
}

// And it stands to the left of every one of them. A meter in its own column
// takes no room out of any control's cell, which is what lets the seven rows
// stay the one height they have been since the fader went.
TEST (MixerLayout, TheMeterStandsLeftOfEveryControlOfItsStrip)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t channel = 0;
       channel < static_cast<std::size_t> (numChannelsInitial); ++channel)
    for (std::size_t i = 0; i < static_cast<std::size_t> (numMixerControls);
         ++i)
      {
        auto const meter = layout.channelMeter[channel];
        auto const control = layout.controls[channel][i];

        EXPECT_FALSE (meter.intersects (control))
            << channel << " over " << mixerControlLabel (mixerControlOrder[i]);
        EXPECT_LE (meter.getRight (), control.getX ())
            << channel << ": the meter is on the wrong side of "
            << mixerControlLabel (mixerControlOrder[i]);
      }
}

// The bar's MIX tab keeps the meter a full-height column beside the controls,
// but at the far right of the band rather than before them -- asked for by the
// maintainer after using the tab on the device. The overlay's strips are
// columns and the level reads before them; the tab is one band read left to
// right, and the level reads at the end of it.
TEST (MixerLayout, TheBarsStripStandsItsMeterAtTheFarRight)
{
  auto const layout = layOutMixerStrip (aBarStrip (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const meter = layout.channelMeter[0];
  ASSERT_FALSE (meter.isEmpty ());

  for (auto const &control : layout.controls[0])
    {
      EXPECT_FALSE (meter.intersects (control));
      EXPECT_GE (meter.getX (), control.getRight ());
      EXPECT_LE (meter.getY (), control.getY ());
      EXPECT_GE (meter.getBottom (), control.getBottom ());
    }
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

// The bar's tab has one strip and three times the width, so it lays the pots
// across rather than down. The table's order still holds -- left to right is
// the reading order there.
TEST (MixerLayout, TheBarsStripReadsAcrossInTheTablesOrder)
{
  auto const layout = layOutMixerStrip (aBarStrip (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const keys
      = static_cast<std::size_t> (controlSlot (MixerControl::Pfl));

  for (std::size_t i = 1; i < keys; ++i)
    EXPECT_GE (layout.controls[0][i].getX (),
               layout.controls[0][i - 1].getRight ())
        << mixerControlLabel (mixerControlOrder[i]) << " is out of order";
}

// And the two keys are a second row under them, not a sixth and seventh cell
// beside them. The maintainer asked for it after using the tab: five pots and
// two keys across one band leaves seven cells of a width nobody wants to aim
// a knob at, where two rows leave the pots the width they need and the keys
// the width a key needs anyway.
TEST (MixerLayout, TheBarsStripPutsTheTwoKeysInARowUnderThePots)
{
  auto const layout = layOutMixerStrip (aBarStrip (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const pfl = static_cast<std::size_t> (controlSlot (MixerControl::Pfl));
  auto const fx = static_cast<std::size_t> (controlSlot (MixerControl::Fx));

  EXPECT_EQ (layout.controls[0][pfl].getY (),
             layout.controls[0][fx].getY ());
  EXPECT_LE (layout.controls[0][pfl].getRight (),
             layout.controls[0][fx].getX ());

  for (std::size_t i = 0; i < pfl; ++i)
    {
      EXPECT_LE (layout.controls[0][i].getBottom (),
                 layout.controls[0][pfl].getY ())
          << mixerControlLabel (mixerControlOrder[i])
          << " is not above the keys";
      EXPECT_FALSE (
          layout.controls[0][i].intersects (layout.controls[0][pfl]));
      EXPECT_FALSE (layout.controls[0][i].intersects (layout.controls[0][fx]));
    }
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
// levels on one line; a master volume half a row off the channels' is four
// levels and a stray one, which is the arrangement this replaced.
TEST (MixerLayout, TheMastersVolumeSitsOnTheChannelsVolumeLine)
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

// Five controls against a channel's seven, and the two rows that leaves are
// left empty on purpose: the output level meters go there. A layout that
// filled them now would have to be undone.
TEST (MixerLayout, TheMasterLeavesTheRowsBelowItsVolumeEmpty)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const volumeRow = layout.controls[0][static_cast<std::size_t> (
      controlSlot (MixerControl::Volume))];

  for (auto const &control : layout.master)
    EXPECT_LE (control.getBottom (), volumeRow.getBottom ())
        << "the master reaches into the rows the meters are waiting for";
}

// The filter is three half-width fields side by side under the columns, not a
// band across the foot of the page. Each field is half of the third of the row
// it used to have, which is how the channel keys are read too -- so the row
// reads as one strip's worth of controls standing for all four decks rather
// than as a fourth arrangement on a page that already has three.
TEST (MixerLayout, TheFilterIsThreeHalfWidthFieldsUnderTheColumns)
{
  auto const area = aRoomyOverlay ();
  auto const layout = layOutMixerOverlay (area, metrics);
  ASSERT_TRUE (layout.fits);

  auto span = layout.filter.front ();
  for (auto const &control : layout.filter)
    {
      ASSERT_FALSE (control.isEmpty ());
      EXPECT_GE (control.getWidth (), fingertipSize);
      EXPECT_GE (control.getHeight (), fingertipSize);
      span = span.getUnion (control);
    }

  for (std::size_t i = 1; i < static_cast<std::size_t> (numFilterControls);
       ++i)
    EXPECT_GE (layout.filter[i].getX (), layout.filter[i - 1].getRight ())
        << filterControlLabel (filterControlOrder[i]) << " is out of order";

  // Half the width, and centred in what it no longer fills: a compact block
  // hanging on one edge under five columns reads as a row that ran out rather
  // than as one that was placed. The tolerance is the remainder an integer
  // cell width leaves.
  EXPECT_LE (span.getWidth () * 2, area.getWidth ());
  EXPECT_GT (span.getWidth () * 3, area.getWidth ());
  EXPECT_NEAR (span.getCentreX (), area.getCentreX (), numFilterControls);

  std::vector<juce::Rectangle<int> > columns;
  for (auto const &strip : layout.controls)
    columns.push_back (strip.back ());
  columns.push_back (layout.master.back ());

  for (auto const &column : columns)
    EXPECT_GE (span.getY (), column.getBottom ())
        << "the filter is not under the columns";
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
