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

#include <a3-motion-ui/components/BarKnob.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>
#include <a3-motion-ui/theme/TransportLook.hh>

using namespace a3;

namespace
{

using Pieces = std::vector<std::pair<float, float> >;
constexpr float scale = 2.356f; // 135 degrees, a knob with ends
constexpr float ring = 3.1416f; // half a turn each way

}

TEST (BarKnob, AModulationAboveTheValueRunsUpFromThePointer)
{
  EXPECT_EQ (modulationArcs (0.2f, 1.f, scale, false), (Pieces{ { 0.2f, 1.f } }));
}

TEST (BarKnob, AModulationBelowTheValueRunsDownFromThePointer)
{
  // What #35 was about: sqzX swinging below its setting drew from the start
  // of the scale to the reach, and from the pointer to the end.
  EXPECT_EQ (modulationArcs (0.f, -0.8f, scale, false),
             (Pieces{ { -0.8f, 0.f } }));
}

TEST (BarKnob, ARingThatHasGoneRoundTheTopIsTwoPieces)
{
  EXPECT_EQ (modulationArcs (2.5f, -2.9f, ring, true),
             (Pieces{ { 2.5f, ring }, { -ring, -2.9f } }));
}

TEST (BarKnob, ARingStillMovingForwardIsOnePiece)
{
  EXPECT_EQ (modulationArcs (-1.f, 0.5f, ring, true),
             (Pieces{ { -1.f, 0.5f } }));
}

TEST (BarKnob, NoMovementIsNoArc)
{
  EXPECT_TRUE (modulationArcs (0.4f, 0.4f, scale, false).empty ());
  EXPECT_TRUE (modulationArcs (0.4f, 0.4f, ring, true).empty ());
}

namespace
{
/** How many pixels of a knob painted into an image are the REC key's red. */
int
recordRedPixels (bool writing, bool wraps, float reachFrac = -2.f,
                 bool laneDriven = false)
{
  juce::Image image (juce::Image::ARGB, 80, 80, true);
  {
    juce::Graphics g (image);
    paintBarKnob (g, image.getBounds (), ControlMetrics{ 60, 10.f, 10.f },
                  juce::Colours::green, "reach", 0.8f, false, true, false,
                  reachFrac, wraps, writing, laneDriven);
  }

  auto const red = transportColour (TransportKey::Record);
  int count = 0;
  for (int y = 0; y < image.getHeight (); ++y)
    for (int x = 0; x < image.getWidth (); ++x)
      {
        auto const pixel = image.getPixelAt (x, y);
        if (pixel.getAlpha () > 200 && std::abs (pixel.getRed () - red.getRed ()) < 8
            && std::abs (pixel.getGreen () - red.getGreen ()) < 8
            && std::abs (pixel.getBlue () - red.getBlue ()) < 8)
          ++count;
      }
  return count;
}
}

// A take writing a knob shows it in the recording red, as the REC key does:
// blue says a lane plays, red says the take is writing one.
TEST (BarKnob, AKnobATakeIsWritingIsRed)
{
  EXPECT_EQ (recordRedPixels (false, false), 0);
  EXPECT_GT (recordRedPixels (true, false), 20);
}

TEST (BarKnob, ARingATakeIsWritingHasARedPointer)
{
  // A ring has no value arc; its pointer says it.
  EXPECT_EQ (recordRedPixels (false, true), 0);
  EXPECT_GT (recordRedPixels (true, true), 5);
}

// A knob a take's lane is turning moves as a whole -- pointer and value arc
// on the played value -- and wears a small red dot, so it is clear the
// recording is doing it. Blue stays a sweep's arc.
TEST (BarKnob, AKnobALaneTurnsWearsARedDot)
{
  EXPECT_EQ (recordRedPixels (false, false, -0.2f, false), 0)
      << "a sweep's arc is blue";
  EXPECT_GT (recordRedPixels (false, false, -2.f, true), 5);
}

TEST (BarKnob, ARingALaneTurnsWearsARedDotToo)
{
  EXPECT_GT (recordRedPixels (false, true, -2.f, true), 5);
}

namespace
{
/** How many pixels of a knob painted into an image are the skin's accent. */
int
accentPixels (bool encoderMarked)
{
  juce::Image image (juce::Image::ARGB, 80, 80, true);
  {
    juce::Graphics g (image);
    paintBarKnob (g, image.getBounds (), ControlMetrics{ 60, 10.f, 10.f },
                  juce::Colours::green, "rot", 0.3f, false, true, false, -2.f,
                  false, false, false, encoderMarked);
  }

  auto const accent = toColour (theme ().accent);
  int count = 0;
  for (int y = 0; y < image.getHeight (); ++y)
    for (int x = 0; x < image.getWidth (); ++x)
      {
        auto const pixel = image.getPixelAt (x, y);
        if (pixel.getAlpha () > 200
            && std::abs (pixel.getRed () - accent.getRed ()) < 8
            && std::abs (pixel.getGreen () - accent.getGreen ()) < 8
            && std::abs (pixel.getBlue () - accent.getBlue ()) < 8)
          ++count;
      }
  return count;
}
}

// The knob an encoder is on, where a press switches between two, wears a
// frame in the skin's accent: a press moves it (2026-09-27).
TEST (BarKnob, TheKnobAnEncoderIsOnIsFramed)
{
  EXPECT_EQ (accentPixels (false), 0);
  EXPECT_GT (accentPixels (true), 20);
}

// The frame is only as wide as the widest knob caption needs, and every
// frame is that wide: the same frame on "rot" and on "clip-bot", whatever
// cell each stands in (2026-09-27).
TEST (BarKnob, EveryEncoderFrameIsOneSizeAndFitsItsKnob)
{
  ControlMetrics const metrics{ 30, 10.f, 10.f };

  auto const wide = encoderMarkBounds ({ 0, 0, 200, 60 }, metrics);
  auto const narrower = encoderMarkBounds ({ 0, 0, 150, 60 }, metrics);

  EXPECT_EQ (wide.getWidth (), narrower.getWidth ()) << "one size";
  EXPECT_LT (wide.getWidth (), 200.f) << "not the whole cell";
  EXPECT_GE (wide.getWidth (), static_cast<float> (metrics.knobDiam));
  EXPECT_FLOAT_EQ (wide.getCentreX (), 100.f) << "centred on the knob";
}

// A bit taller than the knob and its caption, with air above and below --
// and in a tall cell no taller than that (2026-09-27).
TEST (BarKnob, AFrameLeavesItsKnobAirAboveAndBelow)
{
  ControlMetrics const metrics{ 30, 10.f, 10.f };
  auto const frame = encoderMarkBounds ({ 0, 0, 120, 200 }, metrics);

  EXPECT_GT (frame.getHeight (), static_cast<float> (metrics.knobDiam) + 10.f)
      << "knob, caption and air";
  EXPECT_LT (frame.getHeight (), 200.f) << "not the whole tall cell";
  EXPECT_FLOAT_EQ (frame.getCentreY (), 100.f);
}

TEST (BarKnob, AFrameNeverLeavesItsCell)
{
  ControlMetrics const metrics{ 30, 10.f, 10.f };
  juce::Rectangle<int> const tight{ 0, 0, 20, 60 };

  EXPECT_TRUE (tight.toFloat ().contains (encoderMarkBounds (tight, metrics)));
}

// -- StemDeck's knob (2026-10-16) -------------------------------------------

// The rings as StemDeck draws them: the outer edge a little inside the knob's
// box, a ring 0.18 of the radius wide, the cap 2.2 rings in from the edge.
TEST (BarKnob, TheGeometryIsStemDecksInShares)
{
  auto const k = knobGeometry (60.f);
  EXPECT_FLOAT_EQ (k.radius, 0.46f * 60.f);
  EXPECT_FLOAT_EQ (k.stroke, 0.18f * k.radius);
  EXPECT_FLOAT_EQ (k.ringRadius, k.radius - 0.5f * k.stroke);
  EXPECT_FLOAT_EQ (k.bodyRadius, k.radius - 2.2f * k.stroke);

  auto const twice = knobGeometry (120.f);
  EXPECT_FLOAT_EQ (twice.bodyRadius, 2.f * k.bodyRadius) << "shares, no pixels";
}

TEST (BarKnob, TheSweepIsAHundredAndFortyFourDegreesEachWay)
{
  EXPECT_NEAR (juce::radiansToDegrees (knobSweep), 144.f, 0.01f);
}

namespace
{
constexpr int side = 80;
ControlMetrics const bigKnob{ 60, 10.f, 10.f };

/** A knob without a caption, which stands in the middle of its cell. */
juce::Image
paintedKnob (float angleFrac, bool selected, bool writing = false)
{
  juce::Image image (juce::Image::ARGB, side, side, true);
  juce::Graphics g (image);
  // Not lit: the lit wash would tint the whole cell.
  paintBarKnob (g, image.getBounds (), bigKnob, juce::Colours::green, {},
                angleFrac, false, false, selected, -2.f, false, writing);
  return image;
}

juce::Colour
pixelAt (juce::Image const &image, float angleRadians, float distance)
{
  auto const centre = juce::Point<float> (side * 0.5f, side * 0.5f);
  auto const at = centre.getPointOnCircumference (distance, angleRadians);
  return image.getPixelAt (juce::roundToInt (at.x), juce::roundToInt (at.y));
}

int
distance (juce::Colour a, juce::Colour b)
{
  return std::abs (a.getRed () - b.getRed ())
         + std::abs (a.getGreen () - b.getGreen ())
         + std::abs (a.getBlue () - b.getBlue ());
}

float
bodyRadius ()
{
  return knobGeometry (static_cast<float> (bigKnob.knobDiam)).bodyRadius;
}
}

// A knob without a caption gives the caption's row to the knob and stands
// in the middle of its cell -- the KEYS knobs have no caption.
TEST (BarKnob, AKnobWithoutACaptionStandsInTheMiddle)
{
  // The cap, walked from the middle of the cell up and down its edge. The
  // ring has a gap at the bottom, so the cap is what is symmetric.
  auto const image = paintedKnob (0.625f, true);
  auto const solid = [&image] (int y) {
    return image.getPixelAt (side / 2, y).getAlpha () > 128;
  };
  int top = side / 2, bottom = side / 2;
  while (top > 0 && solid (top - 1))
    --top;
  while (bottom < side - 1 && solid (bottom + 1))
    ++bottom;
  ASSERT_GT (bottom - top, side / 4) << "a cap in the middle";
  EXPECT_NEAR ((top + bottom) * 0.5f, side * 0.5f, 1.5f);
}

// A raised cap, lit from above: lightness is elevation.
TEST (BarKnob, TheBodyIsLighterAtTheTop)
{
  // The pointer at three o'clock, out of the way of the line probed.
  auto const image = paintedKnob (0.625f, true);
  auto const top = pixelAt (image, 0.f, 0.6f * bodyRadius ());
  auto const bottom
      = pixelAt (image, juce::MathConstants<float>::pi, 0.6f * bodyRadius ());
  EXPECT_GT (top.getAlpha (), 250) << "the cap is solid";
  EXPECT_GT (bottom.getAlpha (), 250);
  EXPECT_GT (top.getPerceivedBrightness (), bottom.getPerceivedBrightness ());
}

// The channel's colour lives in the arc; the pointer is a light tick on the
// cap, as on StemDeck.
TEST (BarKnob, ThePointerIsANeutralTickOnTheBody)
{
  auto const image = paintedKnob (0.f, true); // straight up
  auto const tick = pixelAt (image, 0.f, 0.65f * bodyRadius ());
  EXPECT_LT (distance (tick, toColour (theme ().textPrimary)), 30);
  EXPECT_GT (distance (tick, juce::Colours::green), 100);

  // It starts out on the cap, not in the middle.
  auto const middle = pixelAt (image, 0.f, 0.f);
  EXPECT_GT (distance (middle, toColour (theme ().textPrimary)), 100);
}

TEST (BarKnob, ThePointerOfAKnobNotSelectedIsMuted)
{
  auto const image = paintedKnob (0.f, false);
  auto const tick = pixelAt (image, 0.f, 0.65f * bodyRadius ());
  EXPECT_LT (distance (tick, toColour (theme ().textMuted)), 30);
}

TEST (BarKnob, ThePointerOfAKnobATakeIsWritingIsRed)
{
  auto const image = paintedKnob (0.f, true, true);
  auto const tick = pixelAt (image, 0.f, 0.65f * bodyRadius ());
  EXPECT_LT (distance (tick, transportColour (TransportKey::Record)), 30);
}

// Full scale reaches 144 degrees round, and no further.
TEST (BarKnob, AFullKnobFillsTheWholeSweep)
{
  auto const image = paintedKnob (1.f, true);
  auto const ring
      = knobGeometry (static_cast<float> (bigKnob.knobDiam)).ringRadius;
  auto const at = [&] (float degrees) {
    return pixelAt (image, juce::degreesToRadians (degrees), ring);
  };
  EXPECT_LT (distance (at (140.f), juce::Colours::green), 40);
  EXPECT_LT (distance (at (-140.f), juce::Colours::green), 40);
  EXPECT_GT (distance (at (160.f), juce::Colours::green), 100);
}
