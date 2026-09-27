/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include "ActionEditing.hh"

#include <a3-motion-engine/ActionScript.hh>

namespace a3
{

std::vector<SlotRef>
slotsFiring (juce::File const &file,
             std::vector<std::vector<juce::File>> const &slotFiles)
{
  std::vector<SlotRef> firing;
  if (file == juce::File{})
    return firing;

  for (index_t channel = 0; channel < slotFiles.size (); ++channel)
    for (index_t slot = 0; slot < slotFiles[channel].size (); ++slot)
      if (slotFiles[channel][slot] == file)
        firing.push_back ({ channel, slot });
  return firing;
}

bool
listWaitsFor (bool scriptHasUnsavedChanges)
{
  return scriptHasUnsavedChanges;
}

std::optional<SlotRef>
takeEditOrigin (std::optional<SlotRef> &editOrigin)
{
  auto const taken = editOrigin;
  editOrigin.reset ();
  return taken;
}

PanelSync
panelSyncFor (juce::File const &chosen, juce::File const &inPanel,
              bool unsaved)
{
  if (chosen == inPanel)
    return PanelSync::Keep;
  return unsaved ? PanelSync::HoldRow : PanelSync::Reload;
}

juce::StringArray
scriptErrorsOf (juce::String const &script)
{
  // Run against the defaults with a fixed seed: only what is wrong with the
  // text is wanted here, not what it would do to any clip.
  return runActionScript (script, ClipSettings{}, 0).errors;
}

juce::String
copyBaseFor (juce::File const &from)
{
  return from == juce::File{} ? juce::String{ "Action" }
                              : from.getFileNameWithoutExtension ();
}

}
