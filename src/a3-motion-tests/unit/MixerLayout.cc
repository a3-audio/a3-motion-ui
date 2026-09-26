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
      0, 0, minimumMixerStripWidth * 6,
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

// Wide enough that the four strips still stand side by side, and no wider:
// each channel exactly at the break's own minimum, with the master's fifth
// beside them.
juce::Rectangle<int>
aTightOverlay ()
{
  return juce::Rectangle<int> (0, 0, minimumMixerStripWidth * 5,
                               static_cast<int> (minimumMotionHeight * 8));
}

// Narrower than one strip may be even with the four stacked one by four --
// the one arrangement left that cannot be broken any further -- and tall
// enough that each of those four still clears its rows.
juce::Rectangle<int>
aSliverOverlay ()
{
  return juce::Rectangle<int> (0, 0, minimumMixerStripWidth * 9 / 10,
                               static_cast<int> (minimumMotionHeight * 14));
}
}

// Every control of every channel has a rectangle, and none of them is empty.
TEST (MixerLayout, EveryControlOfEveryChannelGetsARectangle)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t channel = 0;
       channel < static_cast<std::size_t> (numChannelsInitial); ++channel)
    for (std::size_t i = 0; i < static_cast<std::size_t> (numMixerFaceControls);
         ++i)
      EXPECT_FALSE (layout.controls[channel][i].isEmpty ())
          << channel << " " << mixerControlLabel (mixerFaceOrder[i]);
}

// The order on screen is the order in the table. This is what makes the table
// the authority rather than a list that happens to agree.
TEST (MixerLayout, TheControlsAreInTheTablesOrderDownTheStrip)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t i = 1; i < static_cast<std::size_t> (numMixerFaceControls);
       ++i)
    {
      auto const previous = layout.controls[0][i - 1];
      auto const control = layout.controls[0][i];

      // Down the strip, except across the one row two controls share -- the
      // same list read the way a row is read rather than a second order.
      if (control.getY () == previous.getY ())
        EXPECT_GE (control.getX (), previous.getRight ())
            << mixerControlLabel (mixerFaceOrder[i]) << " is out of order";
      else
        EXPECT_GE (control.getY (), previous.getBottom ())
            << mixerControlLabel (mixerFaceOrder[i]) << " is out of order";
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

  auto const pfl = static_cast<std::size_t> (faceSlot (MixerControl::Pfl));
  auto const fx = static_cast<std::size_t> (faceSlot (MixerControl::Fx));

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

// The break's minimum is derived from the two keys, so a strip exactly at it
// still holds PFL and FX a fingertip each -- the widened meter used to leave
// them under one at the minimum the break was made on.
TEST (MixerLayout, AtTheBreaksMinimumTheKeysAreStillAFingertip)
{
  auto const layout = layOutMixerOverlay (aTightOverlay (), metrics);

  EXPECT_EQ (layout.strips.columns, numChannelsInitial);
  EXPECT_GE (layout.controls[0][static_cast<std::size_t> (
                                    faceSlot (MixerControl::Pfl))]
                 .getWidth (),
             fingertipSize);
  EXPECT_TRUE (layout.fits);
}

// The trap the half-width keys bring: a column wide enough to carry a row can
// still be too narrow to carry two fields across it, and a key under a
// fingertip has to make the page say so rather than shrink quietly.
TEST (MixerLayout, AColumnTooNarrowForTwoKeysSideBySideSaysSo)
{
  auto const layout = layOutMixerOverlay (aSliverOverlay (), metrics);

  // The rows themselves clear their floor -- it is only the fields across the
  // last of them that do not, which is exactly the case a check on the row
  // alone would let through.
  EXPECT_GE (layout.controls[0][0].getHeight (), fingertipSize);
  EXPECT_LT (layout.controls[0][static_cast<std::size_t> (
                                    faceSlot (MixerControl::Pfl))]
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
    for (std::size_t i = 1; i < static_cast<std::size_t> (numMixerFaceControls);
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
// takes no room out of any control's cell, which is what lets the strip's six
// rows -- five knobs, then PFL and FX sharing the sixth -- stay the one height
// they have been since the fader went.
TEST (MixerLayout, TheMeterStandsLeftOfEveryControlOfItsStrip)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t channel = 0;
       channel < static_cast<std::size_t> (numChannelsInitial); ++channel)
    for (std::size_t i = 0; i < static_cast<std::size_t> (numMixerFaceControls);
         ++i)
      {
        auto const meter = layout.channelMeter[channel];
        auto const control = layout.controls[channel][i];

        EXPECT_FALSE (meter.intersects (control))
            << channel << " over " << mixerControlLabel (mixerFaceOrder[i]);
        EXPECT_LE (meter.getRight (), control.getX ())
            << channel << ": the meter is on the wrong side of "
            << mixerControlLabel (mixerFaceOrder[i]);
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
      = static_cast<std::size_t> (faceSlot (MixerControl::Pfl));

  for (std::size_t i = 1; i < keys; ++i)
    EXPECT_GE (layout.controls[0][i].getX (),
               layout.controls[0][i - 1].getRight ())
        << mixerControlLabel (mixerFaceOrder[i]) << " is out of order";
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
// the whole mixer is one tap away on MAINMIX.
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

// The master is laid out like a channel: its meters in a column on the left,
// running the strip's whole height, and its pots beside them. The meters are
// the room's -- sub and speakers -- and there will be more of them, so they
// get the column rather than a row.
TEST (MixerLayout, TheMastersMetersStandLeftOfItsPotsTheWholeHeight)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);
  ASSERT_FALSE (layout.masterMeter.isEmpty ());

  for (auto const &control : layout.master)
    EXPECT_LE (layout.masterMeter.getRight (), control.getX ());

  // The master's fader track: the column a channel's meter has, top to foot.
  // The output bars stand in its foot; the rest is where the handle travels.
  EXPECT_EQ (layout.masterMeter.getY (), layout.channelMeter[0].getY ());
  EXPECT_EQ (layout.masterMeter.getBottom (),
             layout.channelMeter[0].getBottom ());

  for (auto const &bar : layout.outputMeters)
    EXPECT_TRUE (layout.masterMeter.contains (bar));
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

// ── 2026-09-26: 3D, FREQ and Q in the strips, the filter in OUT ──────
//
// The bar's 4x3 grid is gone; each channel's 3D, FREQ and Q stand in its own
// strip under SEND, and the filter's row across the foot moved into the OUT
// column under RET -- FX FREQ, FX RES, and FX MODE on the keys' line. The
// row it took is height the strips now use.

// Nine rows: the five pots, the three channel pots, the keys.
TEST (MixerLayout, AStripHasARowForEveryPotAndOneForTheKeys)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  std::set<int> lines;
  for (auto const &control : layout.controls[0])
    lines.insert (control.getY ());
  for (auto const &pot : layout.channelPots[0])
    lines.insert (pot.getY ());

  EXPECT_EQ (lines.size (),
             static_cast<std::size_t> (numMixerFaceControls - 1
                                       + numChannelPots));
}

TEST (MixerLayout, TheChannelPotsStandUnderSendInTheirOrder)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const send = static_cast<std::size_t> (faceSlot (MixerControl::FxSend));
  auto const keys = static_cast<std::size_t> (faceSlot (MixerControl::Pfl));

  for (std::size_t channel = 0;
       channel < static_cast<std::size_t> (numChannelsInitial); ++channel)
    {
      auto const &strip = layout.controls[channel];
      auto const &pots = layout.channelPots[channel];

      EXPECT_EQ (pots[0].getY (), strip[send].getBottom ())
          << channelPotLabel (channelPotOrder[0]) << " is not under SEND";
      for (std::size_t i = 1; i < pots.size (); ++i)
        EXPECT_EQ (pots[i].getY (), pots[i - 1].getBottom ())
            << channelPotLabel (channelPotOrder[i]) << " is out of order";
      EXPECT_EQ (strip[keys].getY (), pots.back ().getBottom ())
          << "the keys are not the last row";

      for (auto const &pot : pots)
        {
          EXPECT_GE (pot.getWidth (), fingertipSize);
          EXPECT_GE (pot.getHeight (), fingertipSize);
          for (auto const &control : strip)
            EXPECT_FALSE (pot.intersects (control));
        }
    }
}

TEST (MixerLayout, TheFilterStandsInTheOutColumnUnderRet)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const index = [] (FilterControl control) {
    for (std::size_t i = 0; i < static_cast<std::size_t> (numFilterControls);
         ++i)
      if (filterControlOrder[i] == control)
        return i;
    return std::size_t{ 0 };
  };
  auto const &ret = layout.master[static_cast<std::size_t> (
      masterFaceSlot (MasterControl::Return))];
  auto const &freq = layout.filter[index (FilterControl::Frequency)];
  auto const &res = layout.filter[index (FilterControl::Resonance)];
  auto const &mode = layout.filter[index (FilterControl::Mode)];
  auto const &keys = layout.controls[0][static_cast<std::size_t> (
      faceSlot (MixerControl::Pfl))];

  EXPECT_EQ (freq.getY (), ret.getBottom ()) << "FX FREQ is not under RET";
  EXPECT_EQ (res.getY (), freq.getBottom ()) << "FX RES is not under FX FREQ";
  EXPECT_EQ (mode.getY (), keys.getY ()) << "FX MODE is not on the keys' line";

  for (auto const &control : { freq, res, mode })
    {
      EXPECT_EQ (control.getX (), ret.getX ()) << "not in the OUT column";
      EXPECT_GE (control.getWidth (), fingertipSize);
      EXPECT_GE (control.getHeight (), fingertipSize);
    }

  // And nothing is left below the columns: the row across the foot is gone.
  for (auto const &control : layout.filter)
    EXPECT_LE (control.getBottom (), keys.getBottom ());
}

TEST (MixerLayout, TheMastersPotsStandRowOnRowOnTheChannelsLines)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  std::set<int> channelLines;
  for (auto const &control : layout.controls[0])
    channelLines.insert (control.getY ());
  for (auto const &pot : layout.channelPots[0])
    channelLines.insert (pot.getY ());

  for (std::size_t i = 0; i < static_cast<std::size_t> (numMasterFaceControls); ++i)
    {
      EXPECT_EQ (channelLines.count (layout.master[i].getY ()), 1u)
          << masterControlLabel (masterFaceOrder[i]) << " is off the lines";
      if (i > 0)
        {
          EXPECT_EQ (layout.master[i].getY (),
                     layout.master[i - 1].getBottom ());
        }
    }
}

// The bar's tab: the channel pots in a second row, each under the pot in the
// same column, and the keys under them -- a third of the height rather than
// half, which is the room the second row takes.
TEST (MixerLayout, TheBarsStripHasTheChannelPotsInASecondRow)
{
  auto const layout = layOutMixerStrip (aBarStrip (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const &pots = layout.controls[0];
  auto const &channelPots = layout.channelPots[0];
  auto const keys = static_cast<std::size_t> (faceSlot (MixerControl::Pfl));

  // The bar strip insets every cell by a gap, so the rows never touch: the
  // second row is told by standing between the pots and the keys, one row
  // pitch below the first, and the keys one more below it.
  auto const pitch = channelPots[0].getCentreY () - pots[0].getCentreY ();
  EXPECT_GE (pitch, fingertipSize);
  EXPECT_NEAR (pots[keys].getCentreY () - channelPots[0].getCentreY (), pitch,
               1)
      << "the three rows are not evenly pitched";

  for (std::size_t i = 0; i < channelPots.size (); ++i)
    {
      EXPECT_GE (channelPots[i].getY (), pots[0].getBottom ())
          << channelPotLabel (channelPotOrder[i]) << " is not in the second row";
      EXPECT_EQ (channelPots[i].getCentreY (), channelPots[0].getCentreY ())
          << channelPotLabel (channelPotOrder[i]) << " is off the row";
      EXPECT_EQ (channelPots[i].getX (), pots[i].getX ())
          << channelPotLabel (channelPotOrder[i]) << " is not under "
          << mixerControlLabel (mixerFaceOrder[i]);
      EXPECT_GE (channelPots[i].getWidth (), fingertipSize);
      EXPECT_GE (channelPots[i].getHeight (), fingertipSize);
    }

  EXPECT_GE (pots[keys].getY (), channelPots[0].getBottom ())
      << "the keys are not under the channel pots";
  EXPECT_LE (pots[keys].getHeight (), pots[0].getHeight () + 1)
      << "the keys are no bigger than a row of pots";
}

TEST (MixerLayout, TheChannelPotsHaveTheirOwnNames)
{
  EXPECT_STREQ (channelPotLabel (ChannelPot::ThreeD), "3D");
  EXPECT_STREQ (channelPotLabel (ChannelPot::Freq), "FREQ");
  EXPECT_STREQ (channelPotLabel (ChannelPot::Q), "Q");
  EXPECT_STREQ (filterControlLabel (FilterControl::Frequency), "FX FREQ");
  EXPECT_STREQ (filterControlLabel (FilterControl::Resonance), "FX RES");
  EXPECT_STREQ (filterControlLabel (FilterControl::Mode), "FX MODE");
}
