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

#include <gtest/gtest.h>

#include <a3-motion-ui/components/LibraryKeys.hh>

using namespace a3;

namespace
{
/** Everything a key could want, so a dark key in these tests is dark because
 *  the rule says so and not because the facts were thin. */
LibraryKeyFacts
plenty ()
{
  return { true, false, true, true };
}
}

// The test this whole unit exists for. Every tab has to be decided --
// the SVG tab was added and not one of the six switches that answer these
// questions learned about it, so all five of its keys went dark and stayed
// dark. A tab that reaches no rule must fail here rather than on the device.
TEST (LibraryKeys, EveryTabIsDecided)
{
  for (auto const list : { BrowserList::Clips, BrowserList::Shapes,
                           BrowserList::Actions, BrowserList::Sessions })
    {
      auto const keys = libraryKeysFor (list, plenty ());
      EXPECT_TRUE (keys.rename && keys.remove)
          << "tab " << static_cast<int> (list)
          << " cannot rename or delete a row that has a file";
    }
}

// Every list is split into what the instrument ships with and what the
// performer made, and every one of them can be narrowed by it. The clips were
// last, and only because a clip's origin had nowhere to live.
TEST (LibraryKeys, EveryListCanBeFiltered)
{
  for (auto const list : { BrowserList::Clips, BrowserList::Shapes,
                           BrowserList::Actions, BrowserList::Sessions })
    EXPECT_TRUE (libraryKeysFor (list, plenty ()).filter)
        << "tab " << static_cast<int> (list);
}

// And what the split buys besides the filter: one of the instrument's own
// cannot be written over, wherever it lives.
TEST (LibraryKeys, NothingShippedCanBeWrittenOver)
{
  auto facts = plenty ();
  facts.chosenIsSystem = true;

  for (auto const list : { BrowserList::Clips, BrowserList::Shapes,
                           BrowserList::Actions, BrowserList::Sessions })
    EXPECT_FALSE (libraryKeysFor (list, facts).save)
        << "tab " << static_cast<int> (list);
}



// The instrument's own shapes are not working material. Writing a recorded
// figure over one would change what every clip naming it plays, on a device
// where the shape is the thing that ships.
TEST (LibraryKeys, ASystemShapeCannotBeWrittenOver)
{
  auto facts = plenty ();
  facts.chosenIsSystem = true;

  EXPECT_FALSE (libraryKeysFor (BrowserList::Shapes, facts).save);
}

TEST (LibraryKeys, AShapeOfYourOwnCanBeWrittenOver)
{
  auto facts = plenty ();
  facts.chosenIsSystem = false;

  EXPECT_TRUE (libraryKeysFor (BrowserList::Shapes, facts).save);
}

// Save as needs something to write, not somewhere to write it back to.
TEST (LibraryKeys, SaveAsNeedsASlotWithSomethingInIt)
{
  auto facts = plenty ();
  facts.slotHolds = false;

  EXPECT_FALSE (libraryKeysFor (BrowserList::Shapes, facts).saveAs);
  EXPECT_TRUE (libraryKeysFor (BrowserList::Shapes, plenty ()).saveAs);
}

// A row the library made up -- "Empty" -- has nothing to rename or delete.
TEST (LibraryKeys, ARowWithNoFileBehindItCanBeNeitherRenamedNorDeleted)
{
  auto facts = plenty ();
  facts.chosenHasFile = false;

  auto const keys = libraryKeysFor (BrowserList::Shapes, facts);
  EXPECT_FALSE (keys.rename);
  EXPECT_FALSE (keys.remove);
}

// ── Loading a set is a key, not a tap ────────────────────────────────────────
//
// A tap on a set row used to load it outright, which replaces all eight slots
// and restarts what was running. The maintainer's verdict: *"das ist sonst
// etwas überraschend wenn direkt das set geladen wird."* Worse, it was the
// only way to *reach* a set -- so renaming or deleting one meant loading it
// first, and losing the arrangement you were working on to do it.

TEST (LibraryKeys, OnlyASetCanBeLoaded)
{
  auto facts = plenty ();

  EXPECT_TRUE (libraryKeysFor (BrowserList::Sessions, facts).load);

  for (auto const list : { BrowserList::Clips, BrowserList::Shapes,
                           BrowserList::Actions })
    EXPECT_FALSE (libraryKeysFor (list, facts).load) << "list";
}

TEST (LibraryKeys, ASetWithNoFileCannotBeLoaded)
{
  auto facts = plenty ();
  facts.chosenHasFile = false;

  EXPECT_FALSE (libraryKeysFor (BrowserList::Sessions, facts).load);
}

// The instrument's own sets load like any other -- they are there to be
// played. It is writing over them that the split forbids.
TEST (LibraryKeys, AShippedSetLoadsToo)
{
  auto facts = plenty ();
  facts.chosenIsSystem = true;

  EXPECT_TRUE (libraryKeysFor (BrowserList::Sessions, facts).load);
}

// ── The drift dot in the list ───────────────────────────────────────
//
// Asked for on 2026-09-19: *"der clip soll durch einen gelben punkt markiert
// sein, wenn sich die einstellungen zum original geändert haben."* The mark
// the clip field on the CLIP page already carries, on the row the clip came
// from, so FILES answers the same question the page next to it does.
//
// Drift is a property of the *slot* against the file it was loaded from, not
// of a row. So at most one row can ever carry it -- the one the slot's values
// came from -- and asking any other row is a category error.

TEST (LibraryDriftDot, TheChosenClipCarriesItWhenTheSlotHasDrifted)
{
  EXPECT_EQ (5, driftedRowIn (BrowserList::Clips, 5, true));
}

TEST (LibraryDriftDot, NothingIsMarkedWhileTheSlotMatchesItsFile)
{
  EXPECT_EQ (-1, driftedRowIn (BrowserList::Clips, 5, false));
}

// Row zero is the library's "Empty" -- a row it made up so that a slot can be
// given nothing. It has no file, so there is nothing for values to differ
// from.
TEST (LibraryDriftDot, TheEmptyRowIsNeverMarked)
{
  EXPECT_EQ (-1, driftedRowIn (BrowserList::Clips, 0, true));
}

TEST (LibraryDriftDot, NoRowChosenIsNoRowMarked)
{
  EXPECT_EQ (-1, driftedRowIn (BrowserList::Clips, -1, true));
}

// The other three tabs answer a different question. A shape is a figure and
// has no settings to drift; an action and a set are written whole. Marking a
// row there would be a dot that means something else on every tab, which is
// the thing a glance cannot survive.
TEST (LibraryDriftDot, OnlyTheClipsTabCanShowIt)
{
  for (auto const list : { BrowserList::Shapes, BrowserList::Actions,
                           BrowserList::Sessions })
    EXPECT_EQ (-1, driftedRowIn (list, 5, true));
}
