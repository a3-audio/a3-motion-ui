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

#include <a3-motion-ui/components/FittedFont.hh>

using namespace a3;

namespace
{

// The share wins while there is room, the cap wins once there is too much.
// Both were written out eight times over, each with its own pair of numbers.
TEST (FittedFont, TheShareDecidesWhileThereIsRoom)
{
  EXPECT_FLOAT_EQ (fittedFontHeight (40.f * 0.45f, 24.f), 18.f);
}

TEST (FittedFont, TheCapDecidesOnceThereIsTooMuch)
{
  EXPECT_FLOAT_EQ (fittedFontHeight (400.f * 0.45f, 24.f), 24.f);
}

// A box can be empty for a frame while a layout settles. A font of zero
// throws inside juce, so the floor is not decoration.
TEST (FittedFont, AnEmptyBoxStillGivesAUsableFont)
{
  EXPECT_GE (fittedFontHeight (0.f, 24.f), 1.f);
}

}
