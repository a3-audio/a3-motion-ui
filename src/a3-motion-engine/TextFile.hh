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

namespace a3
{

/** Write `text` over `file`, with the line endings this device writes.
 *
 *  Everything written here -- scripts, clips, sets, skins, the config, the
 *  migration ledger -- is read by a person in an editor and by git on a Linux
 *  machine, so all of it wants "\n". `juce::File::replaceWithText` takes a
 *  `lineEndings` argument that defaults to **"\r\n"**, which is easy to miss
 *  and silent when missed: the file is written, the app reads it back, and
 *  nothing says a word until a diff shows every line changed.
 *
 *  That had already been found once and fixed in `Theme.cc` alone, with a
 *  comment explaining it, while the other thirteen call sites went on writing
 *  CRLF -- which is how two shipped action scripts came to have it. A fix in
 *  one of fourteen places is a note somebody left, not a fix, so this is the
 *  only place in the project that may call JUCE's writer, and a test says so.
 *
 *  @returns whether the file was written, exactly as JUCE reports it.
 */
bool writeTextFile (juce::File const &file, juce::String const &text);

/** Write `json` over `file` as JSON, with its keys in order and a newline at
 *  the end.
 *
 *  Both are about the diff rather than about tidiness. JUCE writes an
 *  object's keys in the order they were set, so a field added in the middle
 *  of a writer moves every line below it; and a file without a closing
 *  newline makes its last line change whenever anything is appended. A clip
 *  whose diff is always the whole file cannot say whether somebody turned
 *  something -- which is how two hand-edited action scripts went unnoticed
 *  for weeks.
 *
 *  Nested objects are sorted too. Not for `config.json`, which is arranged by
 *  hand and stays in the order it is written in.
 */
bool writeJsonFile (juce::File const &file, juce::var const &json);

/** A float as a number that prints only the digits a float really carries.
 *
 *  `juce::var` holds a float as a double, so 0.7f arrives in JSON as
 *  0.699999988079071 -- seventeen digits claiming a precision the value never
 *  had, and a fresh diff on every save even when nothing moved. This returns
 *  the shortest decimal that still reads back as the same float.
 */
juce::var shortFloat (float value);

}
