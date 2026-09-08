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

#include <a3-motion-ui/components/BrowserComponent.hh>

#include <vector>

namespace a3
{

/** One of the browser's four lists: a folder of named files you pick from.
 *
 *  There is one BrowserComponent and there were four implementations behind
 *  it -- the same decisions taken again in eight places, as a switch or an
 *  `if`, once per list. Every change to the browser on 2026-09-08 went wrong
 *  the same way: a list added to six of the eight, a key lit by one place and
 *  refused by another, a question generalised for one list and not the three
 *  beside it. None of them announced anything; a forgotten branch simply
 *  returns nothing and the key goes dark.
 *
 *  So the decisions live here, once each, and a list is an object rather than
 *  a value to switch on. A new list is a new class and the compiler asks for
 *  every method. There is no eighth place left to forget.
 */
struct LibraryRow
{
  juce::String name;
  bool isShipped;
};

class LibraryList
{
public:
  virtual ~LibraryList () = default;

  /** What this list shows, narrowed and in the order it is shown. */
  virtual std::vector<LibraryRow> rows (ClipFilter filter) const = 0;

  /** Whether the chosen row has a file behind it -- a row the list made up,
   *  like the actions' "Empty", has neither a name to change nor a file to
   *  throw away. */
  virtual bool hasFileAt (int row) const = 0;

  /** Whether the chosen row came with the instrument. Nothing shipped may be
   *  written over. */
  virtual bool isShippedAt (int row) const = 0;

  /** Whether there is a file to write back to *and* something to put in it. */
  virtual bool canSaveInPlace () const = 0;

  /** Put the chosen row on the shown slot. */
  virtual void assign (int row) = 0;

  virtual void rename (int row, juce::String const &name) = 0;
  virtual void remove (int row) = 0;
  virtual void saveInPlace () = 0;

  /** Keep what the slot holds as a new file of this list's kind, and say what
   *  it ended up called. */
  virtual juce::String saveAsCopy () = 0;

  /** What throwing the chosen row away would cost, for the second press to
   *  say before it happens. Most lists cost nothing to say. */
  virtual juce::String costOfRemoving (int row) const;

  /** Which entry of the PatternLibrary a row of this list stands for, or -1.
   *
   *  Every list can answer it and most answer "none" -- the actions and the
   *  sets are folders of their own and have no library entry behind them. On
   *  the base rather than reached for with a dynamic_cast, because a question
   *  every kind can be asked belongs to all of them. */
  virtual int libraryEntryAt (int row) const;
};

}
