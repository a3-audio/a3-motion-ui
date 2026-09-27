/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include <a3-motion-ui/components/ActionEditing.hh>

using namespace a3;

namespace
{
juce::File const bloom ("/tmp/actions/Bloom.scd");
juce::File const ground ("/tmp/actions/Ground.scd");
}

// Save re-runs a script on every clip that fires it, not only the shown one:
// otherwise an edit is heard only after assigning it again.
TEST (ActionEditing, EveryClipFiringTheFileIsFound)
{
  std::vector<std::vector<juce::File>> const slots{
    { bloom, {} }, { ground, bloom }, { {}, {} }, { bloom, ground }
  };
  auto const firing = slotsFiring (bloom, slots);
  std::vector<SlotRef> const expected{ { 0, 0 }, { 1, 1 }, { 3, 0 } };
  EXPECT_EQ (firing, expected);
}

TEST (ActionEditing, NoFileIsFiredByNobody)
{
  std::vector<std::vector<juce::File>> const slots{ { {}, {} } };
  EXPECT_TRUE (slotsFiring (juce::File{}, slots).empty ());
}

// Unsaved changes hold the list: a tap on another row, Rename and Delete
// wait for Save or Cancel (maintainer, 2026-09-27).
TEST (ActionEditing, UnsavedScriptHoldsTheList)
{
  EXPECT_TRUE (listWaitsFor (true));
  EXPECT_FALSE (listWaitsFor (false));
}

// Save as re-points only the clip EDIT came from; opened from the tab, it
// only makes a copy.
TEST (ActionEditing, OnlyTheEditOriginIsRepointed)
{
  EXPECT_FALSE (slotToRepoint (std::nullopt).has_value ());
  auto const origin = slotToRepoint (SlotRef{ 2, 1 });
  ASSERT_TRUE (origin.has_value ());
  EXPECT_EQ (*origin, (SlotRef{ 2, 1 }));
}

// A copy is named after what it came from ("Bloom 2"), a new one "Action".
TEST (ActionEditing, ACopyIsNamedAfterItsOrigin)
{
  EXPECT_EQ (copyBaseFor (bloom), "Bloom");
  EXPECT_EQ (copyBaseFor (juce::File{}), "Action");
}
