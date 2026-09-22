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

#include <cmath>
#include <limits>
#include <vector>

#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/MixerLayout.hh>
#include <a3-motion-ui/components/VuMeter.hh>

using namespace a3;

namespace
{
constexpr ControlMetrics metrics{ fingertipSize, 12.f, 12.f };

// The same roomy overlay MixerLayout's own suite uses, and for the same
// reason: the layout is about proportions, so the area is expressed in the
// thresholds it breaks on rather than in one screen's pixels.
juce::Rectangle<int>
aRoomyOverlay ()
{
  return juce::Rectangle<int> (0, 0,
                               minimumMixerStripWidth * 6,
                               static_cast<int> (minimumMotionHeight * 8));
}

juce::Rectangle<int>
aBarStrip ()
{
  return juce::Rectangle<int> (0, 0,
                               static_cast<int> (minimumChannelWidth * 5),
                               static_cast<int> (minimumMotionHeight * 2));
}

// A meter tall enough that a fraction of it is several pixels, so a rounding
// step cannot be mistaken for the property under test.
juce::Rectangle<int>
aMeterBar ()
{
  return { 10, 20, 12, 400 };
}
}

// The property the whole thing stands on. A meter is read at a glance and a
// meter that has never been told anything must read as silence -- a bar drawn
// full because nothing arrived would send somebody reaching for a level that
// is already down.
TEST (VuMeter, AMeterWithNoValueDrawsNothing)
{
  auto const geometry = vuMeterGeometry (aMeterBar (), VuLevel{});

  EXPECT_FALSE (geometry.track.isEmpty ()) << "the empty bar is still drawn";
  EXPECT_TRUE (geometry.rms.isEmpty ());
  EXPECT_TRUE (geometry.peak.isEmpty ());
}

// And the store answers the same way before a single message has arrived.
TEST (VuMeter, AStoreWithNothingInItReportsSilence)
{
  VuLevels levels;

  for (int channel = 0; channel < numChannelsInitial; ++channel)
    {
      EXPECT_FLOAT_EQ (levels.channel (channel, 0).rms, 0.f);
      EXPECT_FLOAT_EQ (levels.channel (channel, 0).peak, 0.f);
    }

  for (int meter = 0; meter < numOutputMeters; ++meter)
    {
      EXPECT_FLOAT_EQ (levels.output (meter, 0).rms, 0.f);
      EXPECT_FLOAT_EQ (levels.output (meter, 0).peak, 0.f);
    }
}

// Silence is the foot of the bar and full scale is its head. The trap this
// pins is the logarithm: log10 (0) is minus infinity, and a fraction computed
// without saying so first comes back as something no clamp can rescue.
TEST (VuMeter, SilenceIsTheFootAndFullScaleIsTheHead)
{
  EXPECT_FLOAT_EQ (vuMeterFraction (0.f), 0.f);
  EXPECT_FLOAT_EQ (vuMeterFraction (1.f), 1.f);
  EXPECT_GE (vuMeterFraction (2.f), 1.f);
  EXPECT_LE (vuMeterFraction (2.f), 1.f);
}

// A negative amplitude is not something a level meter can report, but a
// malformed message can carry anything and a meter is not the place to find
// that out.
TEST (VuMeter, ANonsenseAmplitudeStaysOnTheBar)
{
  EXPECT_FLOAT_EQ (vuMeterFraction (-1.f), 0.f);
  EXPECT_GE (vuMeterFraction (std::numeric_limits<float>::infinity ()), 0.f);
  EXPECT_LE (vuMeterFraction (std::numeric_limits<float>::infinity ()), 1.f);
}

// Louder is higher up, all the way. A meter whose scale doubled back
// somewhere would be worse than none.
TEST (VuMeter, LouderIsAlwaysHigher)
{
  auto previous = vuMeterFraction (0.f);

  for (auto amplitude = 0.001f; amplitude <= 1.f; amplitude *= 1.1f)
    {
      auto const fraction = vuMeterFraction (amplitude);
      EXPECT_GE (fraction, previous) << amplitude;
      previous = fraction;
    }
}

// The scale is decibels, not amplitude. Half the amplitude is six decibels
// down, which is a tenth of a sixty-decibel bar -- on a linear bar it would
// be half of it, and every passage anybody actually plays would sit in the
// bottom fifth.
TEST (VuMeter, TheScaleIsDecibelsRatherThanAmplitude)
{
  EXPECT_NEAR (vuMeterFraction (0.5f), 0.9f, 0.01f);
  EXPECT_NEAR (vuMeterFraction (0.1f), 2.f / 3.f, 0.01f);
}

// The rms is a column standing on the foot of the bar; the peak is a mark
// above it. Both from the same message, because showing only the rms throws
// the transients away and showing only the peak gives a twitching picture.
TEST (VuMeter, TheRmsStandsOnTheFootAndThePeakMarksAboveIt)
{
  auto const bar = aMeterBar ();
  auto const geometry = vuMeterGeometry (bar, VuLevel{ 0.5f, 0.1f });

  ASSERT_FALSE (geometry.rms.isEmpty ());
  ASSERT_FALSE (geometry.peak.isEmpty ());

  EXPECT_EQ (geometry.rms.getBottom (), bar.getBottom ())
      << "the fill has left the foot of the bar";
  EXPECT_LE (geometry.peak.getBottom (), geometry.rms.getY ())
      << "the peak mark is inside the fill instead of above it";
  EXPECT_LT (geometry.peak.getHeight (), geometry.rms.getHeight ())
      << "the mark is a line, not a second block of fill";
}

// Where they land is what the scale says, to the pixel the bar can express.
TEST (VuMeter, TheRmsAndThePeakLandWhereTheScaleSaysTheyDo)
{
  auto const bar = aMeterBar ();
  auto const level = VuLevel{ 0.5f, 0.1f };
  auto const geometry = vuMeterGeometry (bar, level);

  auto const heightFor = [&bar] (float amplitude) {
    return juce::roundToInt (bar.getHeight () * vuMeterFraction (amplitude));
  };

  EXPECT_EQ (geometry.rms.getHeight (), heightFor (level.rms));
  EXPECT_NEAR (geometry.peak.getBottom (),
               bar.getBottom () - heightFor (level.peak),
               geometry.peak.getHeight ());
}

// Full scale fills the bar and no more. A meter is read for exactly this
// moment, and one that spilled over its own track would be unreadable at it.
TEST (VuMeter, FullScaleFillsTheBarAndStaysInside)
{
  auto const bar = aMeterBar ();
  auto const geometry = vuMeterGeometry (bar, VuLevel{ 1.f, 1.f });

  EXPECT_EQ (geometry.rms, bar);
  EXPECT_TRUE (bar.contains (geometry.peak));
}

// Even a bar a couple of pixels tall gets a mark rather than none: an absent
// peak mark says "no transient", which is a lie a mixer must not tell.
TEST (VuMeter, AShortBarStillGetsAPeakMark)
{
  auto const bar = juce::Rectangle<int> (0, 0, 4, 6);
  auto const geometry = vuMeterGeometry (bar, VuLevel{ 1.f, 0.f });

  ASSERT_FALSE (geometry.peak.isEmpty ());
  EXPECT_TRUE (bar.contains (geometry.peak));
}

// resized() runs with an empty rectangle before the window has a size.
TEST (VuMeter, AnEmptyBarDoesNotDivideByZero)
{
  auto const geometry = vuMeterGeometry ({}, VuLevel{ 1.f, 1.f });

  EXPECT_TRUE (geometry.track.isEmpty ());
  EXPECT_TRUE (geometry.rms.isEmpty ());
  EXPECT_TRUE (geometry.peak.isEmpty ());
}

// The mark lingers. A transient at twenty-five frames a second is one frame
// long, which nobody sees -- holding it is what makes a peak meter a peak
// meter rather than a faster rms one.
TEST (VuMeter, ThePeakMarkLingersAfterTheSignalHasGone)
{
  VuLevels levels;

  levels.setChannel (0, VuLevel{ 0.8f, 0.4f }, 1000);
  levels.setChannel (0, VuLevel{ 0.1f, 0.05f }, 1040);

  EXPECT_FLOAT_EQ (levels.channel (0, 1040).peak, 0.8f)
      << "the held peak was let go on the very next frame";
  EXPECT_FLOAT_EQ (levels.channel (0, 1040).rms, 0.05f)
      << "the rms is the latest frame's, never held";
}

// And it lets go again, or a meter would carry one loud moment for the rest
// of the night.
TEST (VuMeter, ThePeakMarkLetsGoOnceItsHoldIsUp)
{
  VuLevels levels;

  levels.setChannel (0, VuLevel{ 0.8f, 0.4f }, 1000);
  levels.setChannel (0, VuLevel{ 0.1f, 0.05f }, 1000 + vuPeakHoldMs + 1);

  EXPECT_FLOAT_EQ (levels.channel (0, 1000 + vuPeakHoldMs + 1).peak, 0.1f);
}

// A louder peak takes over immediately rather than waiting its turn.
TEST (VuMeter, ALouderPeakReplacesTheHeldOneAtOnce)
{
  VuLevels levels;

  levels.setChannel (0, VuLevel{ 0.4f, 0.2f }, 1000);
  levels.setChannel (0, VuLevel{ 0.9f, 0.2f }, 1010);

  EXPECT_FLOAT_EQ (levels.channel (0, 1010).peak, 0.9f);
}

// The output store is the one thing this task had to build: the subwoofer and
// the four speakers used to go straight to the sphere and be forgotten.
TEST (VuMeter, TheOutputMetersAreHeldOneByOne)
{
  VuLevels levels;

  for (int meter = 0; meter < numOutputMeters; ++meter)
    levels.setOutput (meter, VuLevel{ 0.1f * (meter + 1), 0.f }, 1000);

  for (int meter = 0; meter < numOutputMeters; ++meter)
    EXPECT_NEAR (levels.output (meter, 1000).peak, 0.1f * (meter + 1), 1e-6f)
        << meter << " reads another meter's level";
}

// An index off the end is dropped rather than writing over the meter beside
// it: /vu carries twelve channels and only nine of them are ours.
TEST (VuMeter, AnIndexOffTheEndChangesNothing)
{
  VuLevels levels;

  levels.setChannel (-1, VuLevel{ 1.f, 1.f }, 1000);
  levels.setChannel (numChannelsInitial, VuLevel{ 1.f, 1.f }, 1000);
  levels.setOutput (-1, VuLevel{ 1.f, 1.f }, 1000);
  levels.setOutput (numOutputMeters, VuLevel{ 1.f, 1.f }, 1000);

  for (int channel = 0; channel < numChannelsInitial; ++channel)
    EXPECT_FLOAT_EQ (levels.channel (channel, 1000).peak, 0.f) << channel;
  for (int meter = 0; meter < numOutputMeters; ++meter)
    EXPECT_FLOAT_EQ (levels.output (meter, 1000).peak, 0.f) << meter;

  EXPECT_FLOAT_EQ (levels.channel (-1, 1000).peak, 0.f);
  EXPECT_FLOAT_EQ (levels.output (numOutputMeters, 1000).peak, 0.f);
}

// A meter is a tall thin thing, and that is not decoration: the scale runs
// down its length, so length is the resolution it is read with. A cell inside
// one row of a strip came out wider than it was tall at the smaller skin
// sizes, which is a lamp rather than a meter.
TEST (VuMeter, AChannelsMeterIsTallerThanItIsWide)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t channel = 0;
       channel < static_cast<std::size_t> (numChannelsInitial); ++channel)
    {
      auto const meter = layout.channelMeter[channel];

      ASSERT_FALSE (meter.isEmpty ()) << channel;
      EXPECT_GT (meter.getHeight (), meter.getWidth ()) << channel;
    }
}

// It overlaps nothing. A meter reaching into a control's cell would be drawn
// over a knob a finger is aiming at, and there is no hit area on the meter to
// say so.
TEST (VuMeter, AChannelsMeterOverlapsNothingElse)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t channel = 0;
       channel < static_cast<std::size_t> (numChannelsInitial); ++channel)
    {
      auto const meter = layout.channelMeter[channel];

      for (std::size_t i = 0; i < static_cast<std::size_t> (numMixerFaceControls);
           ++i)
        EXPECT_FALSE (meter.intersects (layout.controls[channel][i]))
            << channel << " over " << mixerControlLabel (mixerFaceOrder[i]);

      for (auto const &control : layout.master)
        EXPECT_FALSE (meter.intersects (control)) << channel;
      for (auto const &control : layout.filter)
        EXPECT_FALSE (meter.intersects (control)) << channel;
      for (auto const &bar : layout.outputMeters)
        EXPECT_FALSE (meter.intersects (bar)) << channel;
    }
}

// Two channels' meters never touch either.
TEST (VuMeter, NoTwoChannelMetersOverlap)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t i = 0; i < static_cast<std::size_t> (numChannelsInitial);
       ++i)
    for (std::size_t j = i + 1;
         j < static_cast<std::size_t> (numChannelsInitial); ++j)
      EXPECT_FALSE (layout.channelMeter[i].intersects (layout.channelMeter[j]))
          << i << " over " << j;
}

// The five output meters sit in the master column, under its own five, in
// the two rows Task 10 left for them.
TEST (VuMeter, TheFiveOutputMetersSitInTheMasterColumn)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t i = 0; i < static_cast<std::size_t> (numOutputMeters); ++i)
    {
      auto const bar = layout.outputMeters[i];
      ASSERT_FALSE (bar.isEmpty ()) << i;
      EXPECT_TRUE (layout.masterMeter.contains (bar)) << i;
      for (auto const &control : layout.master)
        EXPECT_LE (bar.getRight (), control.getX ())
            << i << ": the meters are not left of the master's pots";
    }
}

// And they overlap nothing -- not each other, not a control, not a strip.
TEST (VuMeter, TheOutputMetersOverlapNothing)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  std::vector<juce::Rectangle<int> > everythingElse;
  for (auto const &strip : layout.controls)
    for (auto const &control : strip)
      everythingElse.push_back (control);
  for (auto const &control : layout.master)
    everythingElse.push_back (control);
  for (auto const &control : layout.filter)
    everythingElse.push_back (control);
  for (auto const &meter : layout.channelMeter)
    everythingElse.push_back (meter);

  for (std::size_t i = 0; i < static_cast<std::size_t> (numOutputMeters); ++i)
    {
      for (auto const &other : everythingElse)
        EXPECT_FALSE (layout.outputMeters[i].intersects (other)) << i;

      for (std::size_t j = i + 1;
           j < static_cast<std::size_t> (numOutputMeters); ++j)
        EXPECT_FALSE (
            layout.outputMeters[i].intersects (layout.outputMeters[j]))
            << i << " over " << j;
    }
}

// One block of thin bars stacked one over the other, each the width of the
// block: the master's meters are turned a quarter and swing left to right,
// with the subwoofer at the foot -- "sub ganz unten", which is where a
// subwoofer stands in the room too.
TEST (VuMeter, TheOutputMetersAreOneBlockOfBarsStackedWithTheSubAtTheFoot)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t i = 1; i < static_cast<std::size_t> (numOutputMeters); ++i)
    {
      EXPECT_LE (layout.outputMeters[i].getBottom (),
                 layout.outputMeters[i - 1].getY ())
          << i << " is not above the one before it";
      EXPECT_EQ (layout.outputMeters[i].getX (),
                 layout.outputMeters[0].getX ())
          << i << " does not stand on the block's edge";
      EXPECT_EQ (layout.outputMeters[i].getWidth (),
                 layout.outputMeters[0].getWidth ())
          << i << " is not the width of the block";
    }
}

// One row, and exactly the one the master's five controls leave.
//
// It used to be two. rowsNoMasterControlStandsIn reads which rows are free off
// the master's own map rather than naming them, so when the channel keys began
// sharing a row the block followed from two into one without being told and
// without anything noticing. One row is what the maintainer decided to keep,
// so it is written down here: a decision nothing pins is a decision that can
// drift back.
TEST (VuMeter, TheOutputBarsSitInTheFootOfTheMastersColumn)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  auto block = layout.outputMeters[0];
  for (auto const &bar : layout.outputMeters)
    {
      ASSERT_FALSE (bar.isEmpty ());
      block = block.getUnion (bar);
    }

  // A quarter of the column at its foot: the rest of it is the master's own
  // fader track, and five bars filling the whole column were a wall.
  EXPECT_NEAR (block.getHeight (), layout.masterMeter.getHeight () / 4,
               layout.outputMeters[0].getHeight ());
  EXPECT_GT (block.getY (), layout.masterMeter.getCentreY ());
  EXPECT_LE (block.getBottom (), layout.masterMeter.getBottom ());
}

// Everything stays inside the area the overlay was given, meters included.
TEST (VuMeter, TheMetersStayInsideTheOverlaysArea)
{
  auto const area = juce::Rectangle<int> (
      5, 9, minimumMixerStripWidth * 6,
      static_cast<int> (minimumMotionHeight * 8));
  auto const layout = layOutMixerOverlay (area, metrics);
  ASSERT_TRUE (layout.fits);

  for (auto const &meter : layout.channelMeter)
    EXPECT_TRUE (area.contains (meter));
  for (auto const &bar : layout.outputMeters)
    EXPECT_TRUE (area.contains (bar));
}

// The bar's MIX tab is one channel, so it carries that channel's meter and no
// output block -- the same split the master and the filter already follow.
TEST (VuMeter, TheBarsStripCarriesOneMeterAndNoOutputBlock)
{
  auto const layout = layOutMixerStrip (aBarStrip (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const slot
      = static_cast<std::size_t> (faceSlot (MixerControl::FxSend));

  ASSERT_FALSE (layout.channelMeter[0].isEmpty ());
  EXPECT_FALSE (layout.channelMeter[0].intersects (layout.controls[0][slot]));
  // At the far right of the band, where the maintainer asked for it -- the
  // overlay's meter stays on the left of its column.
  EXPECT_GE (layout.channelMeter[0].getX (),
             layout.controls[0][slot].getRight ());

  for (std::size_t channel = 1;
       channel < static_cast<std::size_t> (numChannelsInitial); ++channel)
    EXPECT_TRUE (layout.channelMeter[channel].isEmpty ()) << channel;

  for (auto const &bar : layout.outputMeters)
    EXPECT_TRUE (bar.isEmpty ());
}

// The banding is a property of where you are on the bar, not of how loud the
// bar is. A fill reaching into the red is green at its foot, yellow through
// its middle and red only at its head -- a meter that went red as a whole
// would be a warning light, and a warning light cannot say how far over you
// are.
TEST (VuMeter, ABarFilledIntoTheRedIsStillGreenAtItsFoot)
{
  auto const bar = aMeterBar ();
  auto const geometry = vuMeterGeometry (bar, VuLevel{ 1.f, 1.f });

  for (std::size_t i = 0; i < static_cast<std::size_t> (numVuMeterBands); ++i)
    EXPECT_FALSE (geometry.bands[i].isEmpty ()) << i;

  EXPECT_EQ (geometry.bands[vuGreenBand].getBottom (), bar.getBottom ())
      << "the green band has left the foot of the bar";
  EXPECT_EQ (geometry.bands[vuRedBand].getY (), bar.getY ())
      << "the red band has left the head of the bar";
}

// A quiet passage is green and nothing else: there is no yellow to draw below
// where the yellow starts.
TEST (VuMeter, AQuietBarIsGreenAndNothingElse)
{
  auto const geometry = vuMeterGeometry (aMeterBar (), VuLevel{ 0.f, 0.01f });

  EXPECT_FALSE (geometry.bands[vuGreenBand].isEmpty ());
  EXPECT_TRUE (geometry.bands[vuYellowBand].isEmpty ());
  EXPECT_TRUE (geometry.bands[vuRedBand].isEmpty ());
}

// The two boundaries land where the scale says they do, on the same 60 dB
// mapping the fill itself is placed by. Read through vuFractionForDb rather
// than written as two pixel rows, so the claim survives a change of bar.
TEST (VuMeter, TheBandsMeetWhereTheScaleSaysTheyDo)
{
  auto const bar = aMeterBar ();
  auto const geometry = vuMeterGeometry (bar, VuLevel{ 1.f, 1.f });

  auto const yFor = [&bar] (float db) {
    return bar.getBottom ()
           - juce::roundToInt (static_cast<float> (bar.getHeight ())
                               * vuFractionForDb (db));
  };

  EXPECT_EQ (geometry.bands[vuGreenBand].getY (), yFor (vuGreenCeilingDb));
  EXPECT_EQ (geometry.bands[vuYellowBand].getBottom (),
             yFor (vuGreenCeilingDb));
  EXPECT_EQ (geometry.bands[vuYellowBand].getY (), yFor (vuYellowCeilingDb));
  EXPECT_EQ (geometry.bands[vuRedBand].getBottom (),
             yFor (vuYellowCeilingDb));
}

// The bands are the fill cut into three, so together they are exactly the
// fill: nothing outside it, no gap between them, and no band over another.
// The trap is the rounding -- three rectangles each rounded on their own
// would leave a hairline of track showing through a solid fill.
TEST (VuMeter, TheBandsCutUpTheFillAndNothingElse)
{
  auto const geometry = vuMeterGeometry (aMeterBar (), VuLevel{ 0.9f, 0.7f });
  ASSERT_FALSE (geometry.rms.isEmpty ());

  juce::Rectangle<int> covered;

  for (std::size_t i = 0; i < static_cast<std::size_t> (numVuMeterBands); ++i)
    {
      if (!geometry.bands[i].isEmpty ())
        EXPECT_TRUE (geometry.rms.contains (geometry.bands[i])) << i;

      covered = covered.getUnion (geometry.bands[i]);

      for (std::size_t j = i + 1;
           j < static_cast<std::size_t> (numVuMeterBands); ++j)
        EXPECT_FALSE (geometry.bands[i].intersects (geometry.bands[j]))
            << i << " over " << j;
    }

  EXPECT_EQ (covered, geometry.rms);
}

// An empty meter has no bands either. The bands are a cut of the fill, and
// there is no fill to cut.
TEST (VuMeter, AMeterWithNoValueHasNoBands)
{
  auto const geometry = vuMeterGeometry (aMeterBar (), VuLevel{});

  for (std::size_t i = 0; i < static_cast<std::size_t> (numVuMeterBands); ++i)
    EXPECT_TRUE (geometry.bands[i].isEmpty ()) << i;
}

// An overlay too small to lay out hands back no meters either, rather than
// rectangles a caller might paint into.
TEST (VuMeter, AnEmptyOverlayHasNoMeters)
{
  auto const layout = layOutMixerOverlay ({}, metrics);

  EXPECT_FALSE (layout.fits);
  for (auto const &meter : layout.channelMeter)
    EXPECT_TRUE (meter.isEmpty ());
  for (auto const &bar : layout.outputMeters)
    EXPECT_TRUE (bar.isEmpty ());
}

// The raster both blocks of thin bars are stepped on, extracted because both
// of them had it written out. Stepped from the left with an integer cell:
// taken from the right the remainder lands between the bars rather than
// against the edge, and a block whose bars are not evenly spaced reads as a
// fault in the picture rather than as a level.
TEST (VuMeter, ABlockOfBarsIsSteppedEvenlyFromItsLeftEdge)
{
  auto const block = juce::Rectangle<int> (10, 4, 47, 20);
  auto const cell = block.getWidth () / 5;
  auto const gap = 2;

  std::array<juce::Rectangle<int>, 5> bars{};
  stepMeterBarsAcross (block, cell, gap, bars);

  EXPECT_EQ (bars.front ().getX (), block.getX ());

  for (auto const &bar : bars)
    {
      EXPECT_EQ (bar.getY (), block.getY ());
      EXPECT_EQ (bar.getHeight (), block.getHeight ());
      EXPECT_EQ (bar.getWidth (), cell - gap);
      EXPECT_TRUE (block.contains (bar)) << bar.toString ();
    }

  for (std::size_t i = 1; i < bars.size (); ++i)
    {
      EXPECT_EQ (bars[i].getX () - bars[i - 1].getX (), cell);
      EXPECT_LT (bars[i - 1].getRight (), bars[i].getX ())
          << "two bars meeting read as one wide one";
    }
}

// A cell narrower than its own gap still has to draw something. A bar of no
// width is a meter saying nothing, where a hairline says "this output is
// here, and quiet".
TEST (VuMeter, ABarTooNarrowForItsGapIsStillAPixelWide)
{
  std::array<juce::Rectangle<int>, 5> bars{};
  stepMeterBarsAcross (juce::Rectangle<int> (0, 0, 5, 10), 1, 4, bars);

  for (auto const &bar : bars)
    EXPECT_EQ (bar.getWidth (), 1) << bar.toString ();
}


// ── The signal dot on a channel face ────────────────────────────────────────

namespace
{
/** The amplitude a level meter would be handed for a given dBFS.
 *
 *  The inverse of what vuMeterFraction does on the way in, written here
 *  rather than in the header: the tests state levels the way an engineer says
 *  them, and nothing in the picture needs to go the other way. */
float
dbToAmplitude (float db)
{
  return std::pow (10.f, db / 20.f);
}
}

TEST (VuDot, SilenceIsNoDotAtAll)
{
  // A face with no dot is a channel with no signal, which is the reading the
  // whole thing exists for. Not a dim dot: dim is "quiet", and "quiet" and
  // "nothing" are the two states a hand needs told apart at a glance.
  EXPECT_FALSE (vuDot (VuLevel{ 0.f, 0.f }).visible);
}

TEST (VuDot, AnythingAboveTheFloorIsVisible)
{
  auto const justOn = vuDot (VuLevel{ 0.f, dbToAmplitude (-59.f) });
  EXPECT_TRUE (justOn.visible);
  EXPECT_GE (justOn.alpha, vuDotMinAlpha);
}

TEST (VuDot, ItGetsBrighterWithTheLevel)
{
  auto const quiet = vuDot (VuLevel{ 0.f, dbToAmplitude (-40.f) });
  auto const loud = vuDot (VuLevel{ 0.f, dbToAmplitude (-10.f) });

  EXPECT_TRUE (quiet.visible);
  EXPECT_TRUE (loud.visible);
  EXPECT_LT (quiet.alpha, loud.alpha);
  EXPECT_LE (loud.alpha, 1.f);
}

TEST (VuDot, ItNeverFadesBelowWhereItCanBeFound)
{
  // Against a channel's own colour, a dot at a tenth of an alpha is a dot
  // nobody sees -- so the fade stops short and the last step is to absent.
  for (float db = -59.f; db < 0.f; db += 1.f)
    {
      auto const dot = vuDot (VuLevel{ 0.f, dbToAmplitude (db) });
      EXPECT_TRUE (dot.visible) << db;
      EXPECT_GE (dot.alpha, vuDotMinAlpha) << db;
    }
}

TEST (VuDot, ItTurnsColourWhereTheMeterDoes)
{
  // The same three ceilings, read through the same table. A dot that went
  // yellow at a different level from the meter beside it on the MIX page
  // would be two instruments disagreeing about one signal.
  EXPECT_EQ (vuDot (VuLevel{ 0.f, dbToAmplitude (-30.f) }).band, vuGreenBand);
  EXPECT_EQ (vuDot (VuLevel{ 0.f, dbToAmplitude (-12.f) }).band, vuYellowBand);
  EXPECT_EQ (vuDot (VuLevel{ 0.f, dbToAmplitude (-3.f) }).band, vuRedBand);
}

TEST (VuDot, TheBandBoundariesAreTheMetersOwn)
{
  EXPECT_EQ (vuDot (VuLevel{ 0.f, dbToAmplitude (vuGreenCeilingDb - 0.5f) }).band,
             vuGreenBand);
  EXPECT_EQ (vuDot (VuLevel{ 0.f, dbToAmplitude (vuGreenCeilingDb + 0.5f) }).band,
             vuYellowBand);
  EXPECT_EQ (vuDot (VuLevel{ 0.f, dbToAmplitude (vuYellowCeilingDb + 0.5f) }).band,
             vuRedBand);
}

TEST (VuDot, ItReadsTheRmsAndNotThePeak)
{
  // A peak is a transient. Read through a mark with no length it would
  // flicker at every drum hit and say nothing; the peak has somewhere to be
  // already, on the MIX page's meters.
  auto const quietWithATransient
      = vuDot (VuLevel{ dbToAmplitude (-1.f), dbToAmplitude (-40.f) });
  auto const quiet = vuDot (VuLevel{ 0.f, dbToAmplitude (-40.f) });

  EXPECT_EQ (quietWithATransient.band, quiet.band);
  EXPECT_FLOAT_EQ (quietWithATransient.alpha, quiet.alpha);
}

TEST (VuDot, ANonsenseLevelIsSilence)
{
  // The levels arrive over UDP from another program.
  EXPECT_FALSE (vuDot (VuLevel{ 0.f, -1.f }).visible);
  EXPECT_FALSE (
      vuDot (VuLevel{ 0.f, std::numeric_limits<float>::quiet_NaN () }).visible);
}

// The volume mark: where VOL stands, laid across the meter it is dragged on.
// The foot of the bar is VOL at nothing, the head VOL all the way up -- the
// knob's travel, not a level, so it is linear in the value.
TEST (VuMeter, TheFaderHandleIsAtTheFootForNothingAndAtTheHeadForFull)
{
  auto const bar = aMeterBar ();

  auto const none = vuFaderHandle (bar, 0.f);
  auto const full = vuFaderHandle (bar, 1.f);

  EXPECT_EQ (none.getBottom (), bar.getBottom ());
  EXPECT_EQ (full.getY (), bar.getY ());
}

TEST (VuMeter, TheFaderHandleClimbsWithTheValue)
{
  auto const bar = aMeterBar ();

  EXPECT_GT (vuFaderHandle (bar, 0.25f).getY (), vuFaderHandle (bar, 0.5f).getY ());
  EXPECT_GT (vuFaderHandle (bar, 0.5f).getY (), vuFaderHandle (bar, 0.75f).getY ());
  EXPECT_NEAR (vuFaderHandle (bar, 0.5f).getCentreY (), bar.getCentreY (), 3);
}

// Across the whole bar, and thicker than the peak mark, so the two lines on
// one meter cannot be read as each other.
TEST (VuMeter, TheFaderHandleSpansTheBarAndIsThickEnoughToGrasp)
{
  auto const bar = aMeterBar ();
  auto const handle = vuFaderHandle (bar, 0.6f);
  auto const peak = vuMeterGeometry (bar, { 0.5f, 0.1f }).peak;

  EXPECT_EQ (handle.getX (), bar.getX ());
  EXPECT_EQ (handle.getWidth (), bar.getWidth ());
  EXPECT_GT (handle.getHeight (), peak.getHeight ());
  // Something a finger can take hold of, not a line: a fader cap, like the
  // pots' knobs are knobs.
  EXPECT_GE (handle.getHeight (), minimumFaderHandleThickness);
}

TEST (VuMeter, AVolumeOutsideTheRangeKeepsTheHandleOnTheBar)
{
  auto const bar = aMeterBar ();

  for (auto const value : { -1.f, 2.f, std::numeric_limits<float>::quiet_NaN () })
    EXPECT_TRUE (bar.contains (vuFaderHandle (bar, value))) << value;
}

TEST (VuMeter, AnEmptyMeterHasNoFaderHandle)
{
  EXPECT_TRUE (vuFaderHandle ({}, 0.5f).isEmpty ());
}

// The overlay's meter is dragged now, as well as read: two fifths of its
// strip, where it used to take the share REAPER's own meter takes of a
// channel.
TEST (VuMeter, TheOverlaysMeterIsTwoFifthsOfItsStrip)
{
  auto const strip = juce::Rectangle<int> (0, 0, 200, 400);
  auto const split = splitStripForMeter (strip);

  EXPECT_NEAR (split.meter.getWidth () + (strip.getWidth () - split.meter.getWidth ()
                                          - split.controls.getWidth ()),
               strip.getWidth () * 2 / 5, 1);
}

// Dragging a meter moves VOL one to one with the finger: the whole height of
// the meter is the whole of VOL's travel. Relative -- it starts from where VOL
// stood when the finger came down, so landing low on a loud channel does not
// pull it down -- and without steps, so the mark stays under the finger.
TEST (VuMeter, AMeterDragMovesVolumeOneToOneWithTheFinger)
{
  EXPECT_FLOAT_EQ (vuMeterDragVolume (0.5f, 100, 400), 0.75f);
  EXPECT_FLOAT_EQ (vuMeterDragVolume (0.5f, -100, 400), 0.25f);
  EXPECT_FLOAT_EQ (vuMeterDragVolume (0.3f, 0, 400), 0.3f);
}

TEST (VuMeter, AMeterDragStopsAtBothEnds)
{
  EXPECT_FLOAT_EQ (vuMeterDragVolume (0.9f, 400, 400), 1.f);
  EXPECT_FLOAT_EQ (vuMeterDragVolume (0.1f, -400, 400), 0.f);
}

TEST (VuMeter, AMeterWithNoHeightLeavesVolumeWhereItWas)
{
  EXPECT_FLOAT_EQ (vuMeterDragVolume (0.4f, 50, 0), 0.4f);
}

// Turned a quarter: the fill grows from the left edge rather than up from the
// foot, and the bands follow it across. Everything else about the meter is
// the same picture, which is why this is a direction and not a second meter.
TEST (VuMeter, ASidewaysMeterFillsFromTheLeft)
{
  auto const bar = juce::Rectangle<int> (10, 20, 400, 12);

  auto const silent = vuMeterGeometry (bar, { 0.f, 0.f }, VuDirection::Right);
  EXPECT_TRUE (silent.rms.isEmpty ());

  auto const loud = vuMeterGeometry (bar, { 1.f, 1.f }, VuDirection::Right);
  EXPECT_EQ (loud.rms, bar);

  auto const half = vuMeterGeometry (bar, { 0.5f, 0.5f }, VuDirection::Right);
  EXPECT_EQ (half.rms.getX (), bar.getX ());
  EXPECT_EQ (half.rms.getY (), bar.getY ());
  EXPECT_EQ (half.rms.getHeight (), bar.getHeight ());
  EXPECT_GT (half.rms.getWidth (), 0);
  EXPECT_LT (half.rms.getWidth (), bar.getWidth ());
}

TEST (VuMeter, ASidewaysPeakMarkStandsUpright)
{
  auto const bar = juce::Rectangle<int> (10, 20, 400, 12);
  auto const geometry = vuMeterGeometry (bar, { 0.5f, 0.2f }, VuDirection::Right);

  ASSERT_FALSE (geometry.peak.isEmpty ());
  EXPECT_EQ (geometry.peak.getHeight (), bar.getHeight ());
  EXPECT_LT (geometry.peak.getWidth (), bar.getWidth () / 4);
  EXPECT_TRUE (bar.contains (geometry.peak));
}

TEST (VuMeter, ASidewaysBandsRunLeftToRight)
{
  auto const bar = juce::Rectangle<int> (0, 0, 400, 12);
  auto const geometry = vuMeterGeometry (bar, { 1.f, 1.f }, VuDirection::Right);

  for (std::size_t i = 1; i < static_cast<std::size_t> (numVuMeterBands); ++i)
    {
      ASSERT_FALSE (geometry.bands[i].isEmpty ()) << i;
      EXPECT_GE (geometry.bands[i].getX (), geometry.bands[i - 1].getX ())
          << i << " does not follow the band before it";
    }
}

// A fader cap is about twice as wide as it is tall. On the bar's tab the
// meter is short and wide, and a handle measured off the track's height
// alone came out flat on it -- "im clipmixer ist der faderknob zu gestaucht".
TEST (VuMeter, TheFaderHandleKeepsItsProportionsOnAShortWideMeter)
{
  auto const wide = juce::Rectangle<int> (0, 0, 90, 260);
  auto const handle = vuFaderHandle (wide, 0.5f);

  EXPECT_GE (handle.getHeight (), wide.getWidth () / 3);
  EXPECT_LE (handle.getHeight (), wide.getWidth ());
  EXPECT_TRUE (wide.contains (handle));
}

// And stays a handle on a long narrow one: the overlay's meter is nearly six
// hundred pixels tall, where a twelfth of it would be a slab.
TEST (VuMeter, TheFaderHandleStaysAHandleOnALongMeter)
{
  auto const tall = juce::Rectangle<int> (0, 0, 46, 590);
  auto const handle = vuFaderHandle (tall, 0.5f);

  EXPECT_LE (handle.getHeight (), fingertipSize);
  EXPECT_GE (handle.getHeight (), minimumFaderHandleThickness);
}

// The tab's meter is as wide as the overlay's, not twice it: a full column of
// the band made a meter 93 px across where the overlay's is 46, and the fader
// handle on it read as squashed -- "mach das vumeter ruhig genauso breit wie
// im main mixer". Still a fingertip wide, because it is dragged.
TEST (VuMeter, TheBarsMeterIsNoWiderThanTheOverlaysAndStillAFingertip)
{
  auto const strip = layOutMixerStrip (aBarStrip (), metrics);
  auto const overlay = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (strip.fits);
  ASSERT_TRUE (overlay.fits);

  auto const pot = strip.controls[0][0].getWidth ();

  EXPECT_NEAR (strip.channelMeter[0].getWidth (), pot / 2, 2);
  EXPECT_GE (strip.channelMeter[0].getWidth (), fingertipSize);
  EXPECT_GE (strip.channelMeter[0].getRight (), strip.controls[0][0].getRight ());
}
