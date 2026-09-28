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

#include <juce_core/juce_core.h>

#include <a3-motion-engine/ClipSettings.hh>

#include <array>
#include <utility>
#include <vector>

namespace a3
{

/** Scripts ACTION has changed and not yet written (2026-09-29).
 *
 *  A turn is a dozen detents and one change of mind, and each detent changes
 *  the script: the file is written once the hand stops. One entry per file,
 *  the latest text, in the order first changed. */
class PendingScriptWrites
{
public:
  void put (juce::File const &file, juce::String const &text);
  bool empty () const { return _waiting.empty (); }
  /** Everything waiting, and nothing left waiting. */
  std::vector<std::pair<juce::File, juce::String> > take ();

private:
  std::vector<std::pair<juce::File, juce::String> > _waiting;
};

/** Writes each; returns the names of the files that would not take it. */
juce::StringArray
writeAll (std::vector<std::pair<juce::File, juce::String> > const &writes);

/** Every (channel, button) whose script is `file` -- the buttons a change to
 *  that script reaches, since a script is written in place. */
std::vector<std::pair<int, int> > buttonsHoldingFile (
    std::vector<std::array<juce::File, numActionButtons> > const &files,
    juce::File const &file);

}
