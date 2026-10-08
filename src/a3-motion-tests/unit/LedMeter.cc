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

#include <a3-motion-ui/components/LedMeter.hh>
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

juce::Image
painted (VuReading reading, juce::Rectangle<int> bounds)
{
  juce::Image image (juce::Image::ARGB, bounds.getRight (), bounds.getBottom (),
                     true);
  juce::Graphics g (image);
  paintLedMeter (g, bounds, reading);
  return image;
}

/** Blending rounds differently in the renderer and in Colour: a step or two
 *  per channel is the same colour. */
bool
nearly (juce::Colour a, juce::Colour b)
{
  return std::abs (a.getRed () - b.getRed ()) <= 2
         && std::abs (a.getGreen () - b.getGreen ()) <= 2
         && std::abs (a.getBlue () - b.getBlue ()) <= 2;
}

juce::Point<int>
middleOf (juce::Rectangle<float> segment)
{
  return segment.getCentre ().roundToInt ();
}
}

TEST (LedMeter, SilenceLightsNothing)
{
  auto const lights = ledMeterLights ({});
  EXPECT_EQ (lights.lit, 0);
  EXPECT_EQ (lights.held, 0);
}

TEST (LedMeter, FullScaleLightsEverySegment)
{
  EXPECT_EQ (ledMeterLights ({ 1.f, 1.f }).lit, ledMeterSegments);
}

// What the desk does with a channel's peak, written out from a3-mixer's
// a3_mixer_meters.py (CHANNEL_LED_THRESHOLDS_DB, CHANNEL_LED_COLOURS, decided
// 2026-10-07) and the firmware's channelLedColour: LED n lights once the peak
// reaches its threshold; four green, two yellow, two red.
namespace
{
constexpr float deskThresholdsDb[8] = { -36.f, -24.f, -18.f, -12.f,
                                        -9.f,  -6.f,  -3.f,  0.f };
constexpr std::size_t deskColours[8]
    = { vuGreenBand,  vuGreenBand,  vuGreenBand, vuGreenBand,
        vuYellowBand, vuYellowBand, vuRedBand,   vuRedBand };

int
deskLedsLit (float peakDb)
{
  auto lit = 0;
  for (auto threshold : deskThresholdsDb)
    lit += peakDb >= threshold ? 1 : 0;
  return lit;
}
}

// The fault the maintainer saw on the rig (2026-10-08): a peak of -5 dBFS
// is yellow on the desk and was red here, because this meter put its red at
// -6 dBFS on a -60..0 scale of its own.
TEST (LedMeter, APeakTheDeskShowsYellowIsYellowHere)
{
  auto const lights = ledMeterLights ({ amplitudeAt (-5.f), 0.f });
  ASSERT_GT (lights.lit, 0);
  EXPECT_EQ (ledSegmentBand (lights.lit - 1), vuYellowBand);
}

// The same level lights the same number of LEDs in the same colour as the
// desk's channel meter, all the way up -- not only at the one level reported.
TEST (LedMeter, EveryLevelLightsWhatTheDeskLights)
{
  ASSERT_EQ (ledMeterSegments, 8);
  for (auto db = -60.f; db <= 3.f; db += 0.25f)
    {
      auto const lights = ledMeterLights ({ amplitudeAt (db), 0.f });
      ASSERT_EQ (lights.lit, deskLedsLit (db)) << db << " dBFS";
      if (lights.lit > 0)
        EXPECT_EQ (ledSegmentBand (lights.lit - 1), deskColours[lights.lit - 1])
            << db << " dBFS";
    }
}

TEST (LedMeter, EachLedWearsTheDesksColour)
{
  ASSERT_EQ (ledMeterSegments, 8);
  for (int i = 0; i < ledMeterSegments; ++i)
    EXPECT_EQ (ledSegmentBand (i), deskColours[i]) << i;
}

TEST (LedMeter, TheThresholdsAreTheDesks)
{
  ASSERT_EQ (ledMeterSegments, 8);
  for (int i = 0; i < ledMeterSegments; ++i)
    EXPECT_FLOAT_EQ (ledThresholdsDb[static_cast<std::size_t> (i)],
                     deskThresholdsDb[i])
        << i;
}

// The desk's own file, where it can be reached (the a3-system checkout): the
// numbers above are a copy, and a copy is held to its source here. Skipped
// in a checkout without a3-mixer beside it.
TEST (LedMeter, TheCopyAgreesWithTheDesksSource)
{
  auto const ui = juce::File (A3_UI_SOURCE_DIR).getParentDirectory ().getParentDirectory ();
  juce::File meters;
  for (auto const *up : { "../../a3-mixer", "../../../../a3-mixer", "../a3-mixer" })
    {
      auto const candidate = ui.getChildFile (up).getChildFile (
          "software/scripts/a3_mixer_meters.py");
      if (candidate.existsAsFile ())
        {
          meters = candidate;
          break;
        }
    }
  if (meters == juce::File ())
    GTEST_SKIP () << "no a3-mixer checkout beside " << ui.getFullPathName ();

  auto const text = meters.loadFileAsString ();
  auto const thresholds = text.fromFirstOccurrenceOf ("CHANNEL_LED_THRESHOLDS_DB = (", false, false)
                              .upToFirstOccurrenceOf (")", false, false);
  juce::StringArray numbers;
  numbers.addTokens (thresholds, ",", "");
  numbers.trim ();
  numbers.removeEmptyStrings ();
  ASSERT_EQ (numbers.size (), 8) << thresholds;
  for (int i = 0; i < 8; ++i)
    EXPECT_FLOAT_EQ (numbers[i].getFloatValue (), deskThresholdsDb[i]) << i;

  auto const colours = text.fromFirstOccurrenceOf ("CHANNEL_LED_COLOURS = ", false, false)
                           .upToFirstOccurrenceOf ("\n", false, false)
                           .removeCharacters (" ");
  EXPECT_EQ (colours, "(\"green\",)*4+(\"yellow\",)*2+(\"red\",)*2");
}

TEST (LedMeter, AHoldAboveTheBarNamesItsSegment)
{
  auto const lights
      = ledMeterLights ({ amplitudeAt (-40.f), amplitudeAt (-7.f) });
  EXPECT_GT (lights.held, lights.lit);
  EXPECT_EQ (lights.held, deskLedsLit (-7.f));
  EXPECT_EQ (ledSegmentBand (lights.held - 1), vuYellowBand);
}

TEST (LedMeter, AHoldAtTheBarIsNotShownTwice)
{
  auto const level = amplitudeAt (-20.f);
  EXPECT_EQ (ledMeterLights ({ level, level }).held, 0);
}

TEST (LedMeter, SegmentsSitInOrderInsideTheWell)
{
  juce::Rectangle<int> const bounds{ 0, 0, 200, 20 };
  auto previousRight = static_cast<float> (bounds.getX ());
  for (int i = 0; i < ledMeterSegments; ++i)
    {
      auto const segment = ledSegment (bounds, i);
      EXPECT_TRUE (bounds.toFloat ().contains (segment)) << i;
      EXPECT_GE (segment.getX (), previousRight) << i;
      EXPECT_GT (segment.getWidth (), 0.f) << i;
      previousRight = segment.getRight ();
    }
}

// Every LED is drawn: an unlit one is a ghost of its band's colour on the
// well, which is how a desk's meter reads before anything plays.
TEST (LedMeter, AnUnlitSegmentIsAGhostOnTheWell)
{
  juce::Rectangle<int> const bounds{ 0, 0, 200, 20 };
  auto const image = painted ({}, bounds);
  auto const at = middleOf (ledSegment (bounds, 0));
  auto const pixel = image.getPixelAt (at.x, at.y);
  auto const ground = toColour (theme ().background);
  auto const green = vuBandColour (theme (), vuGreenBand);
  EXPECT_NE (pixel, ground);
  EXPECT_TRUE (nearly (
      pixel, ground.overlaidWith (green.withAlpha (theme ().alphaOutline))))
      << pixel.toDisplayString (false);
}

TEST (LedMeter, ALitSegmentIsItsBandsColour)
{
  juce::Rectangle<int> const bounds{ 0, 0, 200, 20 };
  auto const image = painted ({ 1.f, 1.f }, bounds);
  for (auto i : { 0, ledMeterSegments - 1 })
    {
      auto const at = middleOf (ledSegment (bounds, i));
      EXPECT_EQ (image.getPixelAt (at.x, at.y),
                 vuBandColour (theme (), ledSegmentBand (i)))
          << i;
    }
}

TEST (LedMeter, TheWellIsTheSkinsGround)
{
  juce::Rectangle<int> const bounds{ 0, 0, 200, 20 };
  auto const image = painted ({ 1.f, 1.f }, bounds);
  auto const gap = (ledSegment (bounds, 0).getRight ()
                    + ledSegment (bounds, 1).getX ())
                   / 2.f;
  EXPECT_EQ (image.getPixelAt (juce::roundToInt (gap), bounds.getCentreY ()),
             toColour (theme ().background));
}

// The desk lights the held peak's LED whole, in its own colour, above a
// dark gap -- so does this one.
TEST (LedMeter, TheHeldPeakLightsItsWholeLed)
{
  juce::Rectangle<int> const bounds{ 0, 0, 200, 20 };
  VuReading const reading{ amplitudeAt (-40.f), amplitudeAt (-7.f) };
  auto const lights = ledMeterLights (reading);
  ASSERT_GT (lights.held, lights.lit);
  auto const image = painted (reading, bounds);
  auto const segment = ledSegment (bounds, lights.held - 1);
  auto const lit = vuBandColour (theme (), ledSegmentBand (lights.held - 1));
  auto const y = juce::roundToInt (segment.getCentreY ());
  EXPECT_EQ (image.getPixelAt (juce::roundToInt (segment.getRight ()) - 1, y), lit);
  EXPECT_EQ (image.getPixelAt (juce::roundToInt (segment.getX ()) + 1, y), lit);
  auto const below = ledSegment (bounds, lights.held - 2);
  EXPECT_NE (image.getPixelAt (juce::roundToInt (below.getCentreX ()), y),
             vuBandColour (theme (), ledSegmentBand (lights.held - 2)))
      << "the LED under it stays dark";
}

TEST (LedMeter, AnEmptyBoxPaintsNothing)
{
  juce::Image image (juce::Image::ARGB, 4, 4, true);
  juce::Graphics g (image);
  paintLedMeter (g, {}, { 1.f, 1.f });
  EXPECT_EQ (image.getPixelAt (1, 1), juce::Colour ());
}
