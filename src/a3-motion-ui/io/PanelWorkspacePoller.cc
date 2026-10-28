/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

  SPDX-FileCopyrightText: 2026 Raphael Eismann
  SPDX-License-Identifier: GPL-3.0-or-later

*/

#include "PanelWorkspacePoller.hh"

#include <a3-motion-ui/io/Workspaces.hh>

namespace a3
{
namespace
{
constexpr int pollMs = 500;

/** "1:MOTION", the way the env var and i3's config spell it. */
std::string
focusedName (std::vector<workspaces::Workspace> const &all)
{
  for (auto const &workspace : all)
    if (workspace.current)
      return std::to_string (workspace.number) + ":"
             + workspace.label.toStdString ();
  return {};
}
}

PanelWorkspacePoller::PanelWorkspacePoller (InputOutputAdapter &adapter,
                                            std::string myWorkspace)
    : juce::Thread ("PanelWorkspacePoller"), _adapter (adapter),
      _rule (std::move (myWorkspace))
{
}

void
PanelWorkspacePoller::run ()
{
  using Action = PanelFollow::Action;

  while (!threadShouldExit ())
    {
      auto const holding = _adapter.isPanelHeld ();
      auto const focused = focusedName (workspaces::list ());
      // No i3 to ask, or nothing in front: leave the panel where it is.
      if (!focused.empty ())
        {
          auto const now = juce::Time::getMillisecondCounterHiRes () / 1000.0;
          switch (_rule.update (focused, now, holding))
            {
            case Action::release:
              _adapter.setPanelHeld (false);
              break;
            case Action::takeAfterDelay:
              _adapter.setPanelHeld (true);
              break;
            case Action::none:
              break;
            }
        }

      wait (pollMs);
    }
}
}
