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

namespace a3
{

/** `source` with `~name` set to `written` (a value as a script writes it).
 *
 *  ACTION writes into the script (2026-09-29), and a person's file is not
 *  regenerated to do it: the one line changes, everything else stays.
 *  - A live line gets the new value; its comment stays.
 *  - A commented-out line (`//~name`) is uncommented.
 *  - No line: one is added at the end of the name's section, with the
 *    annotation every script carries, or at the end of the file.
 *  - Two lines: the last live one, as the reader lets the last one win.
 *  A line counts only when it starts with `~name` or `//~name` followed by
 *  a space or `=`: `~clipTop` is not `~clip`, and a comment mentioning
 *  `~spin` is not a `~spin` line. */
juce::String setScriptLine (juce::String const &source,
                            juce::String const &name,
                            juce::String const &written);

/** `source` with its live `~name` line commented out -- "as the clip is".
 *  Nothing live, nothing changes. */
juce::String unsetScriptLine (juce::String const &source,
                              juce::String const &name);

}
