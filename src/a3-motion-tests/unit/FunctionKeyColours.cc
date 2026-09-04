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

#include <a3-motion-ui/theme/FunctionKeyColours.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

using namespace a3;

// Whose tempo is running has one answer and one colour, wherever it is shown
// — the status bar reads it by the same rule.
TEST (FunctionKeyColours, TheClockKeyCarriesItsModesColour)
{
  for (int mode : { 0, 1, 2 })
    {
      FunctionKeyLook look;
      look.clockMode = mode;

      EXPECT_EQ (functionKeyColour (FunctionKey::ClockMode, look),
                 Colours::clockMode (mode));
    }
}

// The one key coloured while nothing is happening, and red while it is:
// recording writes over what you cannot get back, so the key that does it
// says so before it is pressed as well as after. Running is the same red,
// brighter — a difference in loudness rather than a second thing to learn.
TEST (FunctionKeyColours, RecordIsRedAndBrighterWhileRunning)
{
  FunctionKeyLook look;
  auto const resting = functionKeyColour (FunctionKey::Record, look);
  EXPECT_EQ (resting, toColour (theme ().danger));

  look.recording = true;
  auto const running = functionKeyColour (FunctionKey::Record, look);

  EXPECT_NE (running, resting);

  // Perceived brightness, not getBrightness(): danger is a fully saturated
  // red, whose brightness is already 1, so brighter() can only take it
  // towards white. What the eye reads is the perceived value, and that is
  // what has to go up.
  EXPECT_GT (running.getPerceivedBrightness (),
             resting.getPerceivedBrightness ());

  // Still red: the hue is what the key means, and it does not change.
  EXPECT_NEAR (running.getHue (), resting.getHue (), 0.02f);
}

// A modifier you cannot see at a glance is one you will get wrong — but only
// while it is down; held is the whole of what it has to say.
TEST (FunctionKeyColours, ShiftIsColouredOnlyWhileHeld)
{
  FunctionKeyLook look;
  EXPECT_TRUE (functionKeyColour (FunctionKey::Shift, look).isTransparent ());

  look.shiftHeld = true;
  EXPECT_EQ (functionKeyColour (FunctionKey::Shift, look),
             toColour (theme ().accent));
}

TEST (FunctionKeyColours, TapIsColouredOnABeatAndUnderAFinger)
{
  FunctionKeyLook look;
  EXPECT_TRUE (functionKeyColour (FunctionKey::Tap, look).isTransparent ());

  look.tapBeat = true;
  EXPECT_EQ (functionKeyColour (FunctionKey::Tap, look),
             toColour (theme ().accent));

  look = {};
  look.tapPressed = true;
  EXPECT_EQ (functionKeyColour (FunctionKey::Tap, look),
             toColour (theme ().accent));
}

// The two that say what they do with a word. A colour on them would be a
// colour that means nothing, and every colour that means nothing makes the
// ones that mean something harder to read.
TEST (FunctionKeyColours, MenuCarriesNoColour)
{
  FunctionKeyLook look;
  look.recording = true;
  look.shiftHeld = true;
  look.tapBeat = true;

  EXPECT_TRUE (functionKeyColour (FunctionKey::Menu, look).isTransparent ());
}


// The rec mode says how much of an old take a pass will destroy, so it says it
// on the scale the rest of the device uses: the accent for the one that mends,
// the warning for the one that holds on, danger for the one that clears.
TEST (FunctionKeyColours, TheRecModeIsColouredByHowMuchItDestroys)
{
  EXPECT_EQ (recModeColour (0), toColour (theme ().accent));
  EXPECT_EQ (recModeColour (1), toColour (theme ().warning));
  EXPECT_EQ (recModeColour (2), toColour (theme ().danger));

  // Out of range is the safe one, not an absent colour: this arrives from a
  // settings file that can be older than the list.
  EXPECT_EQ (recModeColour (7), toColour (theme ().accent));

  FunctionKeyLook look;
  look.recMode = 2;
  EXPECT_EQ (functionKeyColour (FunctionKey::RecMode, look),
             recModeColour (2));
}
