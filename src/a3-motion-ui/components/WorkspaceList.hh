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

#pragma once

#include <JuceHeader.h>

#include <a3-motion-ui/io/Workspaces.hh>

#include <functional>
#include <vector>

namespace a3
{
/** How the switch looks: StemDeck's keys, in both apps (asked for on
 *  2026-09-30: "das stemdeck design gefällt mir gut"). The switch belongs
 *  to the rig rather than to a skin, so it wears the same colours whichever
 *  skin is up -- StemDeck's Theme.h and its juce LookAndFeel_V4 buttons,
 *  drawn the way that look draws them. */
namespace switcherLook
{
/** A key: the app key and the arrow on the bar, and each key in the list.
 *  `current` is the workspace on the screen now. */
void paintKey (juce::Graphics &g, juce::Rectangle<int> area,
               juce::String const &label, bool current);
/** What the list's keys stand on. */
void paintPanel (juce::Graphics &g, juce::Rectangle<int> area);
}

/** The rig's workspaces as a column of keys, opened from the bar's arrow.
 *
 *  Drawn inside the window rather than as a juce::PopupMenu: a menu is a
 *  window of its own, and on the rig -- i3, no compositor -- StemDeck's came
 *  up as a black screen (2026-09-30). Laid over the whole window, so a tap
 *  beside the keys lands here and only closes it. */
class WorkspaceList : public juce::Component
{
public:
  WorkspaceList ();

  /** Shows nothing when there is nothing to offer. The column stands
   *  where StemDeck's does, in the window's coordinates -- see
   *  workspaces::switcherGeometry(). */
  void show (std::vector<workspaces::Workspace> entries);

  /** A key was tapped: go to that workspace. The list has closed. */
  std::function<void (int number)> onChosen;

  int keyCount () const;
  /** The panel the keys stand on. */
  juce::Rectangle<int> columnArea () const;
  juce::Rectangle<int> keyArea (int index) const;

  void paint (juce::Graphics &g) override;
  void mouseUp (juce::MouseEvent const &event) override;

private:
  std::vector<workspaces::Workspace> _entries;
};
}
