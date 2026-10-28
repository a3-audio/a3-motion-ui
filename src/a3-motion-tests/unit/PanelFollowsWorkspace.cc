/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

  SPDX-FileCopyrightText: 2026 Raphael Eismann
  SPDX-License-Identifier: GPL-3.0-or-later

*/

#include <gtest/gtest.h>

#include <a3-motion-ui/io/PanelFollowsWorkspace.hh>

using namespace a3;
using Action = PanelFollow::Action;

TEST (PanelFollowsWorkspace, ReleasesAtOnceWhenAnotherOfOursIsFocused)
{
  PanelFollowsWorkspace follow ("1:MOTION");

  EXPECT_EQ (follow.update ("2:STEMDECK", 10.0, true), Action::release);
}

TEST (PanelFollowsWorkspace, TakesOnlyAfterTheDelay)
{
  PanelFollowsWorkspace follow ("1:MOTION");

  EXPECT_EQ (follow.update ("1:MOTION", 10.0, false), Action::none);
  EXPECT_EQ (follow.update ("1:MOTION", 10.5, false), Action::none);
  EXPECT_EQ (follow.update ("1:MOTION", 10.75, false),
             Action::takeAfterDelay);
}

TEST (PanelFollowsWorkspace, ShortVisitTakesNothing)
{
  PanelFollowsWorkspace follow ("1:MOTION");

  EXPECT_EQ (follow.update ("1:MOTION", 10.0, false), Action::none);
  EXPECT_EQ (follow.update ("1:MOTION", 10.5, false), Action::none);
  EXPECT_EQ (follow.update ("2:STEMDECK", 10.6, false), Action::none);
  // Back again: the delay starts over, it does not remember the old visit.
  EXPECT_EQ (follow.update ("1:MOTION", 11.0, false), Action::none);
  EXPECT_EQ (follow.update ("1:MOTION", 11.5, false), Action::none);
}

TEST (PanelFollowsWorkspace, ForeignWorkspaceChangesNothing)
{
  PanelFollowsWorkspace follow ("1:MOTION");

  EXPECT_EQ (follow.update ("3:ENGINE", 10.0, true), Action::none);
  EXPECT_EQ (follow.update ("3:ENGINE", 20.0, false), Action::none);
}

TEST (PanelFollowsWorkspace, HoldingOwnWorkspaceChangesNothing)
{
  PanelFollowsWorkspace follow ("1:MOTION");

  EXPECT_EQ (follow.update ("1:MOTION", 10.0, true), Action::none);
  EXPECT_EQ (follow.update ("1:MOTION", 20.0, true), Action::none);
}

TEST (PanelFollowsWorkspace, WithoutTheVariableNothingChanges)
{
  EXPECT_EQ (panelWorkspaceFromEnvironment (""), std::nullopt);
  EXPECT_EQ (panelWorkspaceFromEnvironment ("1:MOTION"),
             std::optional<std::string> ("1:MOTION"));
}
