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


namespace a3
{

ScriptPanelLayout
layOutScriptPanel (juce::Rectangle<int> bounds, int errorLines,
                   int lineHeight)
{
  ScriptPanelLayout out;
  if (bounds.isEmpty ())
    return out;

  auto area = bounds;
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

ScriptLanguage
languageFor (juce::File const &file)
{
  return file.hasFileExtension ("svg") ? ScriptLanguage::Xml
                                       : ScriptLanguage::CLike;
}

char const *
fromKeyLabelFor (BrowserList list)
{
  return list == BrowserList::Sessions ? "from set" : "from clip";
}

}
