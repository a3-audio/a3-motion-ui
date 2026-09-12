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

#include <a3-motion-ui/components/BrowserComponent.hh>

namespace a3
{

/** What the browser knows about the row that is chosen and the slot that is
 *  shown. Booleans rather than the objects they came from, so the rule can be
 *  checked without a library, a slot or a screen. */
struct LibraryKeyFacts
{
  /** A row with a file behind it. "Empty" is a row the library made up and
   *  has neither a name to change nor a file to remove. */
  bool chosenHasFile;
  /** The instrument's own, as opposed to the performer's. Only the two
   *  library tabs can tell -- it is the two directories the entries are
   *  scanned from. */
  bool chosenIsSystem;
  /** The shown slot holds something. There is nothing to copy out of an
   *  empty one. */
  bool slotHolds;
  /** There is a file this row came from and something new to put in it. */
  bool canSaveInPlace;
};

/** Which of the five library keys may be pressed. */
struct LibraryKeyStates
{
  bool filter;
  bool rename;
  bool save;
  bool saveAs;
  bool remove;
};

/** One rule for all four tabs, in one place.
 *
 *  It was six switches over BrowserList, in six functions, and the SVG tab
 *  was added to none of them -- so all five of its keys were dark and stayed
 *  dark for as long as the tab has existed. Six places that each have to
 *  learn about a new tab is six chances to forget; one place is one, and the
 *  test walks every tab and insists each is answered.
 */
LibraryKeyStates libraryKeysFor (BrowserList list,
                                 LibraryKeyFacts const &facts);

}
