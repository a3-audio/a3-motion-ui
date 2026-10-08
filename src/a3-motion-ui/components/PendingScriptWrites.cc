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

#include "PendingScriptWrites.hh"

#include <a3-motion-engine/TextFile.hh>

namespace a3
{

void
PendingScriptWrites::put (juce::File const &file, juce::String const &text)
{
  for (auto &waiting : _waiting)
    if (waiting.first == file)
      {
        waiting.second = text;
        return;
      }
  _waiting.emplace_back (file, text);
}

std::vector<std::pair<juce::File, juce::String> >
PendingScriptWrites::take ()
{
  auto out = std::move (_waiting);
  _waiting.clear ();
  return out;
}

juce::StringArray
writeAll (std::vector<std::pair<juce::File, juce::String> > const &writes)
{
  juce::StringArray failed;
  for (auto const &[file, text] : writes)
    // writeTextFile, not File::replaceWithText: that one writes \r\n by
    // default, and a script turned on ACTION would come back with Windows
    // line endings in every line.
    if (!writeTextFile (file, text))
      failed.add (file.getFileNameWithoutExtension ());
  return failed;
}

std::vector<std::pair<int, int> >
buttonsHoldingFile (
    std::vector<std::array<juce::File, numActionButtons> > const &files,
    juce::File const &file)
{
  std::vector<std::pair<int, int> > holding;
  for (size_t channel = 0; channel < files.size (); ++channel)
    for (size_t button = 0; button < files[channel].size (); ++button)
      if (file != juce::File{} && files[channel][button] == file)
        holding.emplace_back (static_cast<int> (channel),
                              static_cast<int> (button));
  return holding;
}

juce::File
ScriptRewire::panelFileAfter (juce::File const &shown) const
{
  return shown == from ? to : shown;
}

ScriptRewire
rewireButtons (
    std::vector<std::array<juce::File, numActionButtons> > const &files,
    juce::File const &file, juce::File const &target)
{
  return { buttonsHoldingFile (files, file), target != file, file, target };
}

}
