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

// The one key coloured while nothing is happening. Recording writes over what
// you cannot get back, so it must never be a thing you have to check.
TEST (FunctionKeyColours, RecordIsArmedOrangeAndRunningRed)
{
  FunctionKeyLook look;

  EXPECT_EQ (functionKeyColour (FunctionKey::Record, look),
             toColour (theme ().warning));

  look.recording = true;
  EXPECT_EQ (functionKeyColour (FunctionKey::Record, look),
             toColour (theme ().danger));
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
TEST (FunctionKeyColours, MenuAndRecModeCarryNoColour)
{
  FunctionKeyLook look;
  look.recording = true;
  look.shiftHeld = true;
  look.tapBeat = true;

  EXPECT_TRUE (functionKeyColour (FunctionKey::Menu, look).isTransparent ());
  EXPECT_TRUE (functionKeyColour (FunctionKey::RecMode, look).isTransparent ());
}
