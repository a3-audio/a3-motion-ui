/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

  SPDX-FileCopyrightText: 2026 Raphael Eismann
  SPDX-License-Identifier: GPL-3.0-or-later

*/

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace a3
{
struct PanelFollow
{
  enum class Action
  {
    none,
    /** Close the port at once. */
    release,
    /** Our workspace has been in front long enough: open the port. */
    takeAfterDelay
  };
};

/** Which deck holds the shared hardware panel on a3nuc2 (plan R9): the one
 *  whose i3 workspace is in front. StemDeck has the same rule, ported
 *  (its Source/PanelFollowsWorkspace.h); no code is shared across the repos.
 *
 *  Pure: the caller reads i3 and the clock and acts on the answer. */
class PanelFollowsWorkspace
{
public:
  /** `decks` are the workspaces of the programs that share the panel;
   *  a workspace outside them never changes anything. */
  explicit PanelFollowsWorkspace (
      std::string myWorkspace, double takeDelaySeconds = 0.75,
      std::vector<std::string> decks = { "1:MOTION", "2:STEMDECK" });

  PanelFollow::Action update (std::string const &focusedWorkspace,
                              double nowSeconds, bool holding);

private:
  std::string _mine;
  double _takeDelay;
  std::vector<std::string> _decks;
  /** When our workspace came to the front, while it is there. */
  std::optional<double> _frontSince;
};

/** The variable's value, or nothing when it is unset or empty: then the
 *  panel is held as it always was. */
std::optional<std::string> panelWorkspaceFromEnvironment (std::string const &value);
}
