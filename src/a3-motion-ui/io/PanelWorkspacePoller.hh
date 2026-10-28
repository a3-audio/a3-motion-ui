/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

  SPDX-FileCopyrightText: 2026 Raphael Eismann
  SPDX-License-Identifier: GPL-3.0-or-later

*/

#pragma once

#include <JuceHeader.h>

#include <a3-motion-ui/io/InputOutputAdapter.hh>
#include <a3-motion-ui/io/PanelFollowsWorkspace.hh>

namespace a3
{
/** Asks i3 every 500 ms which workspace is in front and tells the adapter
 *  whether this program may hold the panel (plan R9). Its own thread, because
 *  asking i3 starts a process. The adapter must outlive it. */
class PanelWorkspacePoller : public juce::Thread
{
public:
  PanelWorkspacePoller (InputOutputAdapter &adapter, std::string myWorkspace);
  ~PanelWorkspacePoller () override { stopThread (2000); }

  void run () override;

private:
  InputOutputAdapter &_adapter;
  PanelFollowsWorkspace _rule;
};
}
