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

#include <JuceHeader.h>

#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/components/SegmentMeter.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

using namespace a3;

namespace
{
float
amplitudeAt (float db)
{
  return std::pow (10.f, db / 20.f);
}

int
pixelsThatDiffer (juce::Image const &a, juce::Image const &b)
{
  auto differ = 0;
  for (int y = 0; y < a.getHeight (); ++y)
    for (int x = 0; x < a.getWidth (); ++x)
      differ += a.getPixelAt (x, y) != b.getPixelAt (x, y) ? 1 : 0;
  return differ;
}
}

// StemDeck's scale: the desk's LED thresholds stand at k/8 of the bar and the
// bar is linear in dB between them, so a level in the top half is the last
// 12 dB.
TEST (SegmentMeter, TheScaleStandsWhereTheDesksLedsDo)
{
  EXPECT_FLOAT_EQ (segmentMeterFraction (0.f), 0.f);
  EXPECT_FLOAT_EQ (segmentMeterFraction (amplitudeAt (-12.f)), 0.5f);
  EXPECT_FLOAT_EQ (segmentMeterFraction (amplitudeAt (-6.f)), 0.75f);
  EXPECT_FLOAT_EQ (segmentMeterFraction (1.f), 1.f);
  EXPECT_FLOAT_EQ (segmentMeterFraction (4.f), 1.f);
}

TEST (SegmentMeter, BelowTheFirstLedTheBarContinuesItsSlope)
{
  // -48 dBFS is where an empty bar ends: the first step's slope run down.
  EXPECT_FLOAT_EQ (segmentMeterFraction (amplitudeAt (-48.f)), 0.f);
  EXPECT_FLOAT_EQ (segmentMeterFraction (amplitudeAt (-42.f)), 0.5f / 8.f);
}

TEST (SegmentMeter, ALevelOnASegmentsTopLightsIt)
{
  EXPECT_EQ (segmentsLit (0.f, 24), 0);
  EXPECT_EQ (segmentsLit (amplitudeAt (-12.f), 24), 12);
  EXPECT_EQ (segmentsLit (amplitudeAt (-9.f), 24), 15);
  EXPECT_EQ (segmentsLit (amplitudeAt (-3.f), 24), 21);
  EXPECT_EQ (segmentsLit (1.f, 24), 24);
  EXPECT_EQ (segmentsLit (1.f, 5), 5);
}

TEST (SegmentMeter, NothingThatIsNotANumberLightsAnything)
{
  EXPECT_EQ (segmentsLit (std::nanf (""), 24), 0);
  EXPECT_EQ (segmentsLit (-1.f, 24), 0);
}

// Green to the -12 LED, yellow at -9 and -6, red at -3 and 0.
TEST (SegmentMeter, ZonesFollowTheLedsEighths)
{
  for (int i = 0; i < 12; ++i)
    EXPECT_EQ (segmentBand (i, 24), vuGreenBand) << i;
  for (int i = 12; i < 18; ++i)
    EXPECT_EQ (segmentBand (i, 24), vuYellowBand) << i;
  for (int i = 18; i < 24; ++i)
    EXPECT_EQ (segmentBand (i, 24), vuRedBand) << i;
}

TEST (SegmentMeter, ZonesHoldAtFewerSegments)
{
  EXPECT_EQ (segmentBand (3, 8), vuGreenBand);
  EXPECT_EQ (segmentBand (4, 8), vuYellowBand);
  EXPECT_EQ (segmentBand (5, 8), vuYellowBand);
  EXPECT_EQ (segmentBand (6, 8), vuRedBand);
}

TEST (SegmentMeter, ALongMeterDrawsTwentyFourAndAShortOneFewer)
{
  EXPECT_EQ (segmentMeterCount (2000), 24);
  EXPECT_LT (segmentMeterCount (30), 24);
  EXPECT_EQ (segmentMeterCount (0), 1);
}

namespace
{
struct Fixture
{
  Fixture ()
  {
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    meter.setBounds (0, 0, 300, 24);
  }
  ~Fixture () { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

  juce::Image paint ()
  {
    return meter.createComponentSnapshot (meter.getLocalBounds ());
  }

  LookAndFeel_A3 lookAndFeel;
  SegmentMeter meter;
};
}

TEST (SegmentMeter, ALevelChangeAsksForARepaintAndTheSameLevelDoesNot)
{
  Fixture f;
  EXPECT_FALSE (f.meter.setReading ({ 0.f, 0.f })) << "silence is what it shows";
  EXPECT_TRUE (f.meter.setReading ({ 0.8f, 0.8f }));
  EXPECT_FALSE (f.meter.setReading ({ 0.8f, 0.8f }));
}

// A twentieth of a segment's worth of level changes nothing that is drawn.
TEST (SegmentMeter, ALevelThatLightsTheSameSegmentsAsksForNothing)
{
  Fixture f;
  f.meter.setReading ({ 0.80f, 0.80f });
  EXPECT_FALSE (f.meter.setReading ({ 0.801f, 0.801f }));
}

TEST (SegmentMeter, TheHeldPeakMovingAsksForARepaint)
{
  Fixture f;
  f.meter.setReading ({ 0.2f, 0.2f });
  EXPECT_TRUE (f.meter.setReading ({ 0.2f, 0.9f }));
}

TEST (SegmentMeter, ItIsOpaqueSoOnlyItsOwnRectangleIsRedrawn)
{
  Fixture f;
  EXPECT_TRUE (f.meter.isOpaque ());
}

TEST (SegmentMeter, LouderPaintsMoreOfTheBar)
{
  Fixture f;
  f.meter.setReading ({ 0.f, 0.f });
  auto const silent = f.paint ();
  f.meter.setReading ({ amplitudeAt (-18.f), amplitudeAt (-18.f) });
  auto const quiet = f.paint ();
  f.meter.setReading ({ 1.f, 1.f });
  auto const full = f.paint ();
  EXPECT_GT (pixelsThatDiffer (silent, quiet), 0);
  EXPECT_GT (pixelsThatDiffer (quiet, full), 0);
}

// StemDeck's LEDs, whatever the skin: the two screens stand side by side and
// read one scale in one set of colours.
TEST (SegmentMeter, TheZonesWearStemDecksLedColours)
{
  EXPECT_EQ (segmentZoneColour (vuGreenBand), juce::Colour (0xff3ec46d));
  EXPECT_EQ (segmentZoneColour (vuYellowBand), juce::Colour (0xffe8c33d));
  EXPECT_EQ (segmentZoneColour (vuRedBand), juce::Colour (0xffe04848));
}

TEST (SegmentMeter, ALitSegmentWearsItsZonesColourAndAnUnlitOneDoesNot)
{
  Fixture f;
  f.meter.setReading ({ amplitudeAt (-12.f), amplitudeAt (-12.f) });
  auto const image = f.paint ();
  auto const y = f.meter.getHeight () / 2;
  auto const segment = [&] (int i) {
    return image.getPixelAt (
        juce::roundToInt ((static_cast<float> (i) + 0.5f)
                          * static_cast<float> (f.meter.getWidth ()) / 24.f),
        y);
  };
  EXPECT_EQ (segment (2), segmentZoneColour (vuGreenBand));
  EXPECT_NE (segment (14), segmentZoneColour (vuYellowBand))
      << "past the bar's head";
  EXPECT_NE (segment (14), segment (2));
}

TEST (SegmentMeter, AHeldPeakIsALineAboveTheBarInItsZonesColour)
{
  Fixture f;
  f.meter.setReading ({ amplitudeAt (-18.f), amplitudeAt (-3.f) });
  auto const image = f.paint ();
  auto const held = 20; // -3 dBFS lights segments 0..20 of 24
  auto const segmentWidth = static_cast<float> (f.meter.getWidth ()) / 24.f;
  auto const y = f.meter.getHeight () / 2;
  auto found = false;
  for (int x = juce::roundToInt (segmentWidth * static_cast<float> (held));
       x < juce::roundToInt (segmentWidth * static_cast<float> (held + 1)); ++x)
    found = found || image.getPixelAt (x, y) == segmentZoneColour (vuRedBand);
  EXPECT_TRUE (found);
}

TEST (SegmentMeter, ItPaintsAtEveryLevelWithoutCrashing)
{
  Fixture f;
  for (auto const db : { -90.f, -48.f, -36.f, -20.f, -9.f, -3.f, 0.f, 6.f })
    {
      f.meter.setReading ({ amplitudeAt (db), amplitudeAt (db) });
      EXPECT_TRUE (f.paint ().isValid ()) << db;
    }
  f.meter.setBounds (0, 0, 0, 0);
  f.meter.setReading ({ 1.f, 1.f });
}

TEST (SegmentMeter, ItPaintsAColumnToo)
{
  Fixture f;
  f.meter.setDirection (VuDirection::Up);
  f.meter.setBounds (0, 0, 20, 200);
  f.meter.setReading ({ 0.5f, 0.9f });
  EXPECT_TRUE (f.paint ().isValid ());
}
