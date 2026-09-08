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

// The filter splits the instrument's own from the performer's, and three
// lists now have that split: the shapes always did, and the actions and the
// sets were given one -- two directories each, the same shape.
TEST (LibraryKeys, EveryListWithTwoHalvesCanBeFiltered)
{
  for (auto const list : { BrowserList::Shapes, BrowserList::Actions,
                           BrowserList::Sessions })
    EXPECT_TRUE (libraryKeysFor (list, plenty ()).filter)
        << "tab " << static_cast<int> (list);
}

// And what the split buys besides the filter: one of the instrument's own
// cannot be written over, wherever it lives.
TEST (LibraryKeys, NothingShippedCanBeWrittenOver)
{
  auto facts = plenty ();
  facts.chosenIsSystem = true;

  for (auto const list : { BrowserList::Shapes, BrowserList::Actions,
                           BrowserList::Sessions })
    EXPECT_FALSE (libraryKeysFor (list, facts).save)
        << "tab " << static_cast<int> (list);
}

// Not the clips: a clip carries Category::Clip and is neither system nor
// user, so narrowing them by System would empty the list rather than shorten
// it. The key used to light here, where it does nothing, and stay dark on the
// shapes, where the narrowing actually runs.
TEST (LibraryKeys, TheClipsOfferNoFilter)
{
  EXPECT_FALSE (libraryKeysFor (BrowserList::Clips, plenty ()).filter);
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
