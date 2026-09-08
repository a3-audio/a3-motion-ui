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
  // The filter splits the instrument's own from the performer's, and three
  // lists have that split: the shapes always did, and the actions and the
  // sets were given one -- a system/ and a user/ directory each, the same
  // shape, so a reader learns it once.
  //
  // Not the clips. A clip carries Category::Clip and is neither system nor
  // user, so narrowing them by System would empty the list rather than
  // shorten it. The key used to light there, where it does nothing, and stay
  // dark on the shapes, where the narrowing runs.
  auto const canFilter = list != BrowserList::Clips;

  // Nothing shipped may be written over, wherever it lives: that is what the
  // split buys besides the filter, and it is the half that matters. Before
  // it, every action and every set the device ships with could be overwritten
  // in silence.
  if (facts.chosenIsSystem)
    return { canFilter, facts.chosenHasFile, false,
             list == BrowserList::Sessions ? true : facts.slotHolds,
             facts.chosenHasFile };

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

  return { canFilter, facts.chosenHasFile, mayWriteBack, somethingToCopy,
           facts.chosenHasFile };
}

}
