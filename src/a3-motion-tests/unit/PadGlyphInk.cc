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

#include <a3-motion-ui/theme/TransportLook.hh>

#include <vector>

using namespace a3;

// A pad's mark sat in its function's colour on whatever colour the pad had,
// and nothing asked whether the two told apart. A running clip turns its Play
// pad `accent` -- the very colour of the triangle -- and the mark vanished.
// Measured on 2026-09-22 with the sunset skin: eleven of the fifteen
// function/ground pairs on the pads page were under 3:1.

namespace
{
// What a pad can be: a running Play pad, the shipped sunset skin's four
// channel colours, and the extremes a skin can choose.
std::vector<juce::Colour>
padGrounds ()
{
  return { padFunctionColour (PadFunction::PlayPause),
           padFunctionColour (PadFunction::Stop),
           juce::Colour (167, 79, 12),   juce::Colour (252, 202, 65),
           juce::Colour (242, 19, 142),  juce::Colour (48, 172, 237),
           juce::Colours::black,         juce::Colours::white,
           juce::Colour (119, 119, 119) };
}

}

TEST (PadGlyphInk, PlayOnARunningPlayPadCanBeRead)
{
  auto const running = padFunctionColour (PadFunction::PlayPause);
  EXPECT_GE (contrastRatio (padGlyphInk (running),
                            running),
             minimumInkContrast);
}

TEST (PadGlyphInk, EveryMarkCanBeReadOnEveryGround)
{
  for (auto const ground : padGrounds ())
    EXPECT_GE (contrastRatio (padGlyphInk (ground), ground),
               minimumInkContrast)
        << ground.toDisplayString (false);
}

// Black or white on every pad, whichever stands out more -- decided on
// 2026-09-22. The function's own colour stayed wherever it could be read, and
// the page became a patchwork: one column all black, the others a mix of
// green, yellow and white. Which key it is, the shape says.
TEST (PadGlyphInk, EveryMarkIsBlackOrWhite)
{
  for (auto const ground : padGrounds ())
    {
      auto const ink = padGlyphInk (ground);
      EXPECT_TRUE (ink == juce::Colours::black || ink == juce::Colours::white)
          << ground.toDisplayString (false);
    }
}

TEST (PadGlyphInk, ItIsTheOneOfTheTwoThatStandsOutMore)
{
  EXPECT_EQ (padGlyphInk (juce::Colours::white), juce::Colours::black);
  EXPECT_EQ (padGlyphInk (juce::Colours::black), juce::Colours::white);
}
