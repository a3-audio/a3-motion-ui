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

#include <UiSource.hh>

using namespace a3;

// The component that owns the engine cannot be built in the runner, so what
// it must do with the pilots' level is read from its source.

TEST (PilotWiring, LeavingFpvCallsTheGamesOffAndRestsThePilots)
{
  auto const body = a3::test::uiComponentBodyOf ("A3MotionUIComponent::setView (AppView view)");
  ASSERT_TRUE (body.isNotEmpty ());
  EXPECT_TRUE (body.contains ("_engine.callOffGames ()"));
  EXPECT_TRUE (body.contains ("_engine.setPilotLevel (fpv ? _pilotLevel : PilotLevel::Off)"));
  EXPECT_TRUE (body.contains ("_statusBar->setPilotLevel (_pilotLevel)"));
}

TEST (PilotWiring, TheKeyStepsTheLevelAndTheLevelIsSaved)
{
  auto const source = juce::File (A3_UI_SOURCE_DIR)
                          .getChildFile ("components/A3MotionUIComponent.cc")
                          .loadFileAsString ();
  EXPECT_TRUE (source.contains ("setPilotLevel (nextPilotLevel (_pilotLevel))"));
  EXPECT_TRUE (source.contains ("_pilotLevel = persisted.pilotLevel"));
  EXPECT_TRUE (a3::test::uiComponentBodyOf ("A3MotionUIComponent::persistSettings () const")
                   .contains ("settings.pilotLevel = _pilotLevel"));

  auto const set = a3::test::uiComponentBodyOf (
      "A3MotionUIComponent::setPilotLevel (PilotLevel level)");
  ASSERT_TRUE (set.isNotEmpty ());
  EXPECT_TRUE (set.contains ("_engine.setPilotLevel (_view == AppView::Fpv ? level : PilotLevel::Off)"));
  EXPECT_TRUE (set.contains ("pilotLevelReadout (level)"));
  EXPECT_TRUE (set.contains ("persistSettings ()"));
}
