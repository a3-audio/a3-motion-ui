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

std::vector<PadFunction> const everyPad{ PadFunction::PlayPause,
                                         PadFunction::Stop,
                                         PadFunction::Action,
                                         PadFunction::Settings };
}

TEST (PadGlyphInk, PlayOnARunningPlayPadCanBeRead)
{
  auto const running = padFunctionColour (PadFunction::PlayPause);
  EXPECT_GE (contrastRatio (padGlyphInk (PadFunction::PlayPause, running),
                            running),
             minimumInkContrast);
}

TEST (PadGlyphInk, EveryMarkCanBeReadOnEveryGround)
{
  for (auto const ground : padGrounds ())
    for (auto const function : everyPad)
      EXPECT_GE (contrastRatio (padGlyphInk (function, ground), ground),
                 minimumInkContrast)
          << "pad function " << static_cast<int> (function) << " on "
          << ground.toDisplayString (false);
}

// The colour says which key it is, so it stays wherever it can be read. Only
// where it cannot does the mark give it up.
TEST (PadGlyphInk, AReadableFunctionColourIsKept)
{
  auto const ground = juce::Colours::black;
  auto const play = padFunctionColour (PadFunction::PlayPause);
  ASSERT_GE (contrastRatio (play, ground), minimumInkContrast);
  EXPECT_EQ (padGlyphInk (PadFunction::PlayPause, ground), play);
}

// Settings stands for no state and so has no colour of its own: it takes
// whichever of black and white stands out more, the same fallback every
// other mark falls back to.
TEST (PadGlyphInk, SettingsIsBlackOrWhite)
{
  EXPECT_EQ (padGlyphInk (PadFunction::Settings, juce::Colours::white),
             juce::Colours::black);
  EXPECT_EQ (padGlyphInk (PadFunction::Settings, juce::Colours::black),
             juce::Colours::white);
}
