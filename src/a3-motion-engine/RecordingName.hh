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

#include <functional>

namespace a3
{

/** What a take made on this day is called, before it is numbered.
 *
 *  The day rather than the second. A take is looked for by when it was made,
 *  and "the second one from the 8th" is something somebody can say out loud
 *  and find in a list; `Rec_144918` is a number nobody remembers and which
 *  sorts a session's takes among every other session's.
 */
juce::String recordingBaseName (juce::Time const &when);

/** The base with the first free running number appended, two digits.
 *
 *  Always numbered, including the first of the day: a bare name beside a
 *  counted one reads as two different kinds of thing, and the first take of a
 *  day is not a different kind of thing. Padded so ten sorts after nine —
 *  the library is listed by the name that is shown.
 *
 *  The number belongs to the *name*, not only to the file. `saveUserPattern`
 *  already avoids overwriting a file, but a set names its takes
 *  (`indexForName`) rather than pointing at a path, so two takes carrying one
 *  name is a set that cannot say which it meant.
 *
 *  `taken` answers whether a name is already in the library. A predicate
 *  rather than the library itself, so the rule can be checked without one.
 */
juce::String
freeRecordingName (juce::String const &base,
                   std::function<bool (juce::String const &)> const &taken);

}
