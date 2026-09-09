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

#include <a3-motion-ui/components/OverlaySideStrips.hh>

using namespace a3;

// The two overlays that are a list: the strips walk it and turn the value of
// the row they are on.
TEST (OverlayStrips, TheMenuAndTheSkinEditorGetTheStrips)
{
  EXPECT_TRUE (sideStripsHaveAList (true, false, false));
  EXPECT_TRUE (sideStripsHaveAList (false, true, false));
}

// Nothing open, nothing to walk. The colour picker is not asked about at all:
// it has navigation of its own, so it is simply not one of the two.
TEST (OverlayStrips, WithNoListOpenThereAreNoStrips)
{
  EXPECT_FALSE (sideStripsHaveAList (false, false, false));
}

// The one this exists for. The MIX key is reachable whatever else is up, so
// the mixer can stand over an open menu -- and it is then the overlay in
// front. The strips are a fifth of the window each: the master column on the
// right and the first channel on the left. Left up, they took every touch
// meant for those two columns and gave it to the menu's highlighted row,
// which changed and was applied on release.
TEST (OverlayStrips, TheMixerInFrontTakesTheStripsAwayFromTheMenu)
{
  EXPECT_FALSE (sideStripsHaveAList (true, false, true));
  EXPECT_FALSE (sideStripsHaveAList (false, true, true));
  EXPECT_FALSE (sideStripsHaveAList (true, true, true));
}

// The mixer is opened from outside the overlay chain, so it can also be the
// only thing on screen. No list either way.
TEST (OverlayStrips, TheMixerAloneHasNoListEither)
{
  EXPECT_FALSE (sideStripsHaveAList (false, false, true));
}
