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

#include "LibraryKeys.hh"

namespace a3
{

LibraryKeyStates
libraryKeysFor (BrowserList list, LibraryKeyFacts const &facts)
{
  // The filter narrows the library, and only the library. Clips and shapes
  // are both scanned out of a system directory and a user one, so the split
  // is there to offer; actions and sets land shipped and hand-written in one
  // folder each with nothing marking which is which, and a key showing a word
  // that would do nothing is worse than a key that is dark.
  auto const isLibrary
      = list == BrowserList::Clips || list == BrowserList::Shapes;

  // Writing over the instrument's own shape would change what every clip
  // naming it plays. On the clips tab the same question is answered by
  // canSaveInPlace, which already knows whether the slot has drifted from
  // the file it came from.
  auto const mayWriteBack = list == BrowserList::Shapes
                                ? facts.slotHolds && !facts.chosenIsSystem
                                : facts.canSaveInPlace;

  // A set can always be put away -- there is always an arrangement to keep --
  // where a clip, an action and a shape are made out of what a slot holds.
  auto const somethingToCopy
      = list == BrowserList::Sessions ? true : facts.slotHolds;

  return { isLibrary, facts.chosenHasFile, mayWriteBack, somethingToCopy,
           facts.chosenHasFile };
}

}
