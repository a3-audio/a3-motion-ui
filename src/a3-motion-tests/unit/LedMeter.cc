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

// The band ceilings fall on segment edges, so the alignment level lights the
// green and nothing past it -- and the first decibel over it, the first yellow.
TEST (LedMeter, TheAlignmentLevelLightsTheGreenAndNoMore)
{
  auto const atAlignment = ledMeterLights ({ amplitudeAt (vuGreenCeilingDb), 0.f });
  ASSERT_GT (atAlignment.lit, 0);
  EXPECT_EQ (ledSegmentBand (atAlignment.lit - 1), vuGreenBand);
  EXPECT_EQ (ledSegmentBand (atAlignment.lit), vuYellowBand);

  auto const over
      = ledMeterLights ({ amplitudeAt (vuGreenCeilingDb + 1.f), 0.f });
  EXPECT_EQ (over.lit, atAlignment.lit + 1);
}

TEST (LedMeter, TheBandsRunGreenYellowRedFootToHead)
{
  auto previous = vuGreenBand;
  EXPECT_EQ (ledSegmentBand (0), vuGreenBand);
  EXPECT_EQ (ledSegmentBand (ledMeterSegments - 1), vuRedBand);
  for (int i = 1; i < ledMeterSegments; ++i)
    {
      auto const band = ledSegmentBand (i);
      EXPECT_GE (band, previous) << i;
      previous = band;
    }
}

TEST (LedMeter, AHoldAboveTheBarNamesItsSegment)
{
  auto const lights
      = ledMeterLights ({ amplitudeAt (-40.f), amplitudeAt (-10.f) });
  EXPECT_GT (lights.held, lights.lit);
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

TEST (LedMeter, TheHeldPeakLightsASliverOfItsSegment)
{
  juce::Rectangle<int> const bounds{ 0, 0, 200, 20 };
  VuReading const reading{ amplitudeAt (-40.f), amplitudeAt (-10.f) };
  auto const lights = ledMeterLights (reading);
  ASSERT_GT (lights.held, lights.lit);
  auto const image = painted (reading, bounds);
  auto const segment = ledSegment (bounds, lights.held - 1);
  auto const lit = vuBandColour (theme (), ledSegmentBand (lights.held - 1));
  auto const y = juce::roundToInt (segment.getCentreY ());
  EXPECT_EQ (image.getPixelAt (juce::roundToInt (segment.getRight ()) - 1, y), lit)
      << "the head end of the held segment";
  EXPECT_NE (image.getPixelAt (juce::roundToInt (segment.getX ()) + 1, y), lit)
      << "the foot end stays a ghost";
}

TEST (LedMeter, AnEmptyBoxPaintsNothing)
{
  juce::Image image (juce::Image::ARGB, 4, 4, true);
  juce::Graphics g (image);
  paintLedMeter (g, {}, { 1.f, 1.f });
  EXPECT_EQ (image.getPixelAt (1, 1), juce::Colour ());
}
