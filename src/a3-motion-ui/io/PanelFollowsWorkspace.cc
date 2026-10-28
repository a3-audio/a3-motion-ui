/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

  SPDX-FileCopyrightText: 2026 Raphael Eismann
  SPDX-License-Identifier: GPL-3.0-or-later

*/

#include "PanelFollowsWorkspace.hh"

#include <algorithm>

namespace a3
{
PanelFollowsWorkspace::PanelFollowsWorkspace (std::string myWorkspace,
                                              double takeDelaySeconds,
                                              std::vector<std::string> decks)
    : _mine (std::move (myWorkspace)), _takeDelay (takeDelaySeconds),
      _decks (std::move (decks))
{
}

PanelFollow::Action
PanelFollowsWorkspace::update (std::string const &focusedWorkspace,
                               double nowSeconds, bool holding)
{
  using Action = PanelFollow::Action;

  if (focusedWorkspace == _mine)
    {
      if (holding)
        return Action::none;

      if (!_frontSince)
        _frontSince = nowSeconds;

      // Longer than one poll of the other side, so its port is closed by
      // the time ours opens.
      return nowSeconds - *_frontSince >= _takeDelay ? Action::takeAfterDelay
                                                     : Action::none;
    }

  _frontSince.reset ();

  auto const isOtherDeck
      = std::find (_decks.begin (), _decks.end (), focusedWorkspace)
        != _decks.end ();
  return isOtherDeck && holding ? Action::release : Action::none;
}

std::optional<std::string>
panelWorkspaceFromEnvironment (std::string const &value)
{
  if (value.empty ())
    return std::nullopt;
  return value;
}
}
