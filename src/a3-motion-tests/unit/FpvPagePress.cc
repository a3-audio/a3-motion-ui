/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <a3-motion-ui/components/fpv/FpvPagePress.hh>

using namespace a3;

TEST (FpvPagePress, APlainPressToggles)
{
  EXPECT_EQ (fpvPagePress (false, std::nullopt), PageOutcome::Toggle);
  static_assert (fpvPagePress (false, std::nullopt) == PageOutcome::Toggle);
}

TEST (FpvPagePress, ATapOnABodyEscortsIt)
{
  EXPECT_EQ (fpvPagePress (true, 4), PageOutcome::Escort);
  EXPECT_EQ (fpvPagePress (true, 0), PageOutcome::Escort) << "id 0 is a body";
}

TEST (FpvPagePress, ATapOnTheFloorPatrols)
{
  EXPECT_EQ (fpvPagePress (true, std::nullopt), PageOutcome::Patrol);
}

// A hold that tapped is a modifier, never a toggle, whatever it hit.
TEST (FpvPagePress, AHoldThatTappedNeverToggles)
{
  for (auto body : { std::optional<int>{}, std::optional<int>{ 2 } })
    EXPECT_NE (fpvPagePress (true, body), PageOutcome::Toggle);
}
