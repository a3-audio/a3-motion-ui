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

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-ui/theme/PadStatusColours.hh>
#include <a3-motion-ui/theme/Theme.hh>

using namespace a3;

namespace
{
juce::Colour const base = juce::Colour::fromRGB (100, 160, 220);

// The blink's two halves come from consecutive steps, so a test that wants
// "the dark one" has to know which parity carries it.
int constexpr stepBright = 0;
int constexpr stepDark = 1;
}

// A pad that is doing something is its channel's colour, undimmed. That is the
// top of the scale and the reason the other three are subtractions from it:
// what the eye finds across a dark booth is the pad that is fully lit.
TEST (PadStatusColours, ARunningPadIsItsOwnColourUntouched)
{
  for (auto const status : { Pattern::Status::Recording,
                             Pattern::Status::Playing })
    EXPECT_EQ (padStatusColour (base, status, Pattern::Status::Idle,
                                stepBright, false),
               base)
        << static_cast<int> (status);
}

// Three shades, and the order is what they mean: an empty slot has least to
// say, an idle one holds a clip, and the blink's dark half sits between them
// because it is on its way to being fully lit.
TEST (PadStatusColours, TheEmptierTheSlotTheDarkerThePad)
{
  auto const empty = padStatusColour (base, Pattern::Status::Empty,
                                      Pattern::Status::Idle, stepBright, false);
  auto const idle = padStatusColour (base, Pattern::Status::Idle,
                                     Pattern::Status::Idle, stepBright, false);
  auto const blink
      = padStatusColour (base, Pattern::Status::ScheduledForPlaying,
                         Pattern::Status::Idle, stepDark, false);

  EXPECT_LT (empty.getBrightness (), blink.getBrightness ());
  EXPECT_LT (blink.getBrightness (), idle.getBrightness ());
  EXPECT_LT (idle.getBrightness (), base.getBrightness ());
}

// A pad waiting for its beat alternates between its own colour and the dark
// half, which is what makes it read as waiting rather than as a state of its
// own. Both scheduled-to-start statuses do it the same way -- they are the
// same promise about the next beat.
TEST (PadStatusColours, AScheduledPadAlternatesWithItsOwnColour)
{
  for (auto const status : { Pattern::Status::ScheduledForRecording,
                             Pattern::Status::ScheduledForPlaying })
    {
      EXPECT_EQ (padStatusColour (base, status, Pattern::Status::Idle,
                                  stepBright, false),
                 base)
          << static_cast<int> (status);
      EXPECT_LT (padStatusColour (base, status, Pattern::Status::Idle, stepDark,
                                  false)
                     .getBrightness (),
                 base.getBrightness ())
          << static_cast<int> (status);
    }
}

// A one-shot take that has run its length is not waiting for anything: it
// stops at the end of the pass whatever happens next, so blinking would
// promise a beat that is not coming.
TEST (PadStatusColours, AOneShotRecordingComingToRestDoesNotBlink)
{
  for (auto const step : { stepBright, stepDark })
    EXPECT_EQ (padStatusColour (base, Pattern::Status::ScheduledForIdle,
                                Pattern::Status::Recording, step, true),
               base)
        << step;
}

// Every status gets a colour. This is the test the arrangement stands on: the
// way this goes wrong is a status being added and nobody deciding what its pad
// looks like, and then it lights as whatever the fallback happens to be.
TEST (PadStatusColours, EveryStatusIsDecided)
{
  for (auto const status :
       { Pattern::Status::Empty, Pattern::Status::Idle,
         Pattern::Status::ScheduledForRecording, Pattern::Status::Recording,
         Pattern::Status::ScheduledForPlaying, Pattern::Status::Playing,
         Pattern::Status::ScheduledForIdle })
    {
      auto const colour = padStatusColour (base, status,
                                           Pattern::Status::Playing, stepDark,
                                           false);
      EXPECT_FALSE (colour.isTransparent ()) << static_cast<int> (status);
      // Still recognisably the channel's colour. Not exactly its hue: darker()
      // works in RGB and rounds to eight bits, so it drifts by about a
      // thousandth -- which is invisible, and which an equality would have
      // called a failure.
      EXPECT_NEAR (colour.getHue (), base.getHue (), 0.01f)
          << static_cast<int> (status);
    }
}

// The three shades are skin values like every other, so a skin can lift the
// empty pads out of the dark without touching what a waiting one looks like.
TEST (PadStatusColours, ASkinSetsOneShadeAndLeavesTheRest)
{
  auto const parsed = juce::JSON::parse (R"({"padShadeEmpty": 0.5})");
  auto const theme = loadTheme (parsed);
  auto const defaults = loadTheme (juce::var{});

  EXPECT_FLOAT_EQ (theme.padShadeEmpty, 0.5f);
  EXPECT_FLOAT_EQ (theme.padShadeIdle, defaults.padShadeIdle);
  EXPECT_FLOAT_EQ (theme.padShadeBlink, defaults.padShadeBlink);
}

// The defaults are the values the device has always lit its pads with. A skin
// may move them; a build that changes them silently is a device whose pads
// mean something new to a hand that has learned them.
TEST (PadStatusColours, TheDefaultsAreWhatTheDeviceHasAlwaysShown)
{
  auto const theme = loadTheme (juce::var{});

  EXPECT_FLOAT_EQ (theme.padShadeEmpty, 0.85f);
  EXPECT_FLOAT_EQ (theme.padShadeBlink, 0.6f);
  EXPECT_FLOAT_EQ (theme.padShadeIdle, 0.3f);
}
