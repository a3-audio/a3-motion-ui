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
recordRedPixels (bool writing, bool wraps)
{
  juce::Image image (juce::Image::ARGB, 80, 80, true);
  {
    juce::Graphics g (image);
    paintBarKnob (g, image.getBounds (), ControlMetrics{ 60, 10.f, 10.f },
                  juce::Colours::green, "reach", 0.8f, false, true, false,
                  -2.f, wraps, writing);
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
