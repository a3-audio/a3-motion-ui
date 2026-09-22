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

#include <a3-motion-ui/components/SceneLaunch.hh>

using namespace a3;

// A scene's Play pad starts the clips of its row that stand still and leaves
// the running ones running. A single Play pad toggles; a scene that toggled
// would start half a row and stop the other half, which mid-set is a surprise
// nobody asked for.

TEST (SceneLaunch, AClipThatStandsStillIsStarted)
{
  EXPECT_TRUE (sceneStartsClip (Pattern::Status::Idle));
}

TEST (SceneLaunch, ARunningOrWaitingClipIsLeftAlone)
{
  EXPECT_FALSE (sceneStartsClip (Pattern::Status::Playing));
  EXPECT_FALSE (sceneStartsClip (Pattern::Status::ScheduledForPlaying));
  EXPECT_FALSE (sceneStartsClip (Pattern::Status::Recording));
}

TEST (SceneLaunch, AnEmptySlotHasNothingToStart)
{
  EXPECT_FALSE (sceneStartsClip (Pattern::Status::Empty));
}
