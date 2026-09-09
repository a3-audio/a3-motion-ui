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

#include <limits>
#include <vector>

#include <a3-motion-ui/components/BarFader.hh>
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
                               static_cast<int> (minimumChannelWidth * 6),
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
// full because nothing arrived would send somebody reaching for a fader that
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

// The channel's meter stands beside its fader, in the volume row, and takes
// none of the room the throw needs.
TEST (VuMeter, AChannelsMeterStandsBesideItsFaderAndNotOverIt)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const slot
      = static_cast<std::size_t> (controlSlot (MixerControl::Volume));

  for (std::size_t channel = 0;
       channel < static_cast<std::size_t> (numChannelsInitial); ++channel)
    {
      auto const meter = layout.channelMeter[channel];
      auto const fader = layout.controls[channel][slot];

      ASSERT_FALSE (meter.isEmpty ()) << channel;
      EXPECT_FALSE (meter.intersects (fader)) << channel;
      EXPECT_LE (meter.getRight (), fader.getX ())
          << channel << ": the meter is on the wrong side of the fader";
      EXPECT_GE (meter.getY (), fader.getY ()) << channel;
      EXPECT_LE (meter.getBottom (), fader.getBottom ()) << channel;
    }
}

// It stays in its own cell. A meter reaching into the row above would answer
// for the EQ band drawn there.
TEST (VuMeter, AChannelsMeterStaysInsideItsCell)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t channel = 0;
       channel < static_cast<std::size_t> (numChannelsInitial); ++channel)
    {
      auto const meter = layout.channelMeter[channel];

      for (std::size_t i = 0; i < static_cast<std::size_t> (numMixerControls);
           ++i)
        EXPECT_FALSE (meter.intersects (layout.controls[channel][i]))
            << channel << " over " << mixerControlLabel (mixerControlOrder[i]);

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

// Taking the meter's width off the fader must not leave a fader that cannot
// be thrown -- that is the whole reason the layout asks rather than assumes.
TEST (VuMeter, TheFaderStillHasAThrowBesideItsMeter)
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
      EXPECT_GE (strip[slot].getWidth (), fingertipSize);
    }
}

// The five output meters sit in the master column, under its fader, in the
// two rows Task 10 left for them.
TEST (VuMeter, TheFiveOutputMetersSitInTheMasterColumn)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  auto const master = layout.master[static_cast<std::size_t> (
      controlSlot (MasterControl::Volume))];

  auto column = layout.master.front ();
  for (auto const &control : layout.master)
    column = column.getUnion (control);

  for (std::size_t i = 0; i < static_cast<std::size_t> (numOutputMeters); ++i)
    {
      auto const bar = layout.outputMeters[i];
      ASSERT_FALSE (bar.isEmpty ()) << i;

      EXPECT_GE (bar.getY (), master.getBottom ())
          << i << ": the meters are not under the master's fader";
      EXPECT_GE (bar.getX (), column.getX ()) << i;
      EXPECT_LE (bar.getRight (), column.getRight ()) << i;
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

// One block of thin bars side by side, the way a multi-channel meter is
// drawn, rather than five widgets scattered down a column.
TEST (VuMeter, TheOutputMetersAreOneBlockOfBarsSideBySide)
{
  auto const layout = layOutMixerOverlay (aRoomyOverlay (), metrics);
  ASSERT_TRUE (layout.fits);

  for (std::size_t i = 1; i < static_cast<std::size_t> (numOutputMeters); ++i)
    {
      EXPECT_GE (layout.outputMeters[i].getX (),
                 layout.outputMeters[i - 1].getRight ())
          << i << " is not beside the one before it";
      EXPECT_EQ (layout.outputMeters[i].getY (),
                 layout.outputMeters[0].getY ())
          << i << " does not stand on the block's line";
      EXPECT_EQ (layout.outputMeters[i].getHeight (),
                 layout.outputMeters[0].getHeight ())
          << i << " is not the height of the block";
    }
}

// Everything stays inside the area the overlay was given, meters included.
TEST (VuMeter, TheMetersStayInsideTheOverlaysArea)
{
  auto const area = juce::Rectangle<int> (
      5, 9, static_cast<int> (minimumChannelWidth * 6),
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
      = static_cast<std::size_t> (controlSlot (MixerControl::Volume));

  ASSERT_FALSE (layout.channelMeter[0].isEmpty ());
  EXPECT_FALSE (layout.channelMeter[0].intersects (layout.controls[0][slot]));
  EXPECT_LE (layout.channelMeter[0].getRight (),
             layout.controls[0][slot].getX ());

  for (std::size_t channel = 1;
       channel < static_cast<std::size_t> (numChannelsInitial); ++channel)
    EXPECT_TRUE (layout.channelMeter[channel].isEmpty ()) << channel;

  for (auto const &bar : layout.outputMeters)
    EXPECT_TRUE (bar.isEmpty ());
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
