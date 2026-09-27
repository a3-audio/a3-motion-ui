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

#include "ScriptPanelLayout.hh"

#include <a3-motion-ui/components/ControllerLayout.hh>

namespace a3
{

ScriptPanelLayout
layOutScriptPanel (juce::Rectangle<int> bounds, int buttonHeight,
                   int errorLines, int lineHeight)
{
  ScriptPanelLayout out;
  if (bounds.isEmpty ())
    return out;

  auto const gap = juce::jmax (2, bounds.getHeight () / 40);
  auto area = bounds;

  auto keys = area.removeFromBottom (juce::jmin (
      area.getHeight (), juce::jmax (fingertipSize, buttonHeight)));
  area.removeFromBottom (gap);

  auto const keyW = (keys.getWidth () - 3 * gap) / 4;
  out.fromClipButton = keys.removeFromLeft (keyW);
  keys.removeFromLeft (gap);
  out.cancelButton = keys.removeFromLeft (keyW);
  keys.removeFromLeft (gap);
  out.saveButton = keys.removeFromLeft (keyW);
  keys.removeFromLeft (gap);
  out.saveAsButton = keys;

  // Off the text, never over it, and never more than half of it.
  if (errorLines > 0)
    out.errorArea = area.removeFromBottom (
        juce::jmin (area.getHeight () / 2, errorLines * lineHeight));

  out.textArea = area;
  return out;
}

ScriptKeyStates
scriptKeysFor (bool unsaved, bool locked, bool hasFile, bool slotHolds)
{
  return { slotHolds, unsaved, unsaved && hasFile && !locked, unsaved };
}

}
