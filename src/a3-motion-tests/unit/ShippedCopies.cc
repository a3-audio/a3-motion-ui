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

#include <JuceHeader.h>

#include <a3-motion-engine/PatternLibrary.hh>
#include <a3-motion-engine/SplitFolder.hh>
#include <a3-motion-ui/theme/Theme.hh>

// The seed puts a newer shipped file beside one the performer changed, as
// <name>.shipped (a3-motion-ui-seed, 2026-10-08). The app must not see it.

namespace
{
struct Scratch
{
  juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getNonexistentChildFile ("a3-shipped-copies", "", false);
  Scratch () { dir.createDirectory (); }
  ~Scratch () { dir.deleteRecursively (); }
};
} // namespace

TEST (ShippedCopies, ASkinsShippedCopyIsNoSkin)
{
  Scratch s;
  auto const skins = s.dir.getChildFile ("skins");
  skins.createDirectory ();
  skins.getChildFile ("ember.json").replaceWithText ("{}");
  skins.getChildFile ("ember.json.shipped").replaceWithText ("{}");
  EXPECT_EQ (a3::availableSkins (s.dir), juce::StringArray ({ "ember" }));
}

TEST (ShippedCopies, AShippedCopyIsNoLibraryEntry)
{
  Scratch s;
  auto const system = s.dir.getChildFile ("system");
  system.createDirectory ();
  system.getChildFile ("Speed Half.scd").replaceWithText ("mine");
  system.getChildFile ("Speed Half.scd.shipped").replaceWithText ("factory");
  auto const entries = a3::listFilesIn (s.dir, ".scd");
  ASSERT_EQ (entries.size (), 1u);
  EXPECT_EQ (entries[0].name, juce::String ("Speed Half"));
}

TEST (ShippedCopies, ALooseShippedCopyIsNotMovedIntoTheUserHalf)
{
  Scratch s;
  s.dir.getChildFile ("Default.json.shipped").replaceWithText ("{}");
  EXPECT_EQ (a3::splitLooseFilesIn (s.dir, ".json"), 0);
  EXPECT_TRUE (s.dir.getChildFile ("Default.json.shipped").existsAsFile ());
}

TEST (ShippedCopies, AShippedCopyIsNoShape)
{
  Scratch s;
  auto const system = s.dir.getChildFile ("system");
  system.createDirectory ();
  juce::String const svg ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                          "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                          "viewBox=\"-1 -1 2 2\" data-name=\"Line\" "
                          "data-beats=\"8\" data-ppqn=\"128\">"
                          "<path d=\"M -0.5 0 L 0.5 0\"/></svg>");
  system.getChildFile ("08_Line.svg").replaceWithText (svg);
  system.getChildFile ("08_Line.svg.shipped").replaceWithText (svg);

  a3::PatternLibrary library (s.dir);
  library.refresh ();

  EXPECT_EQ (library.getNumEntries (), 2); // Empty + the one shape
}

TEST (ShippedCopies, AShippedCopyIsNoClipOrSetInEitherHalf)
{
  Scratch s;
  for (auto const *half : { "system", "user" })
    {
      auto const dir = s.dir.getChildFile (half);
      dir.createDirectory ();
      dir.getChildFile (juce::String ("Mine ") + half + ".json")
          .replaceWithText ("{}");
      dir.getChildFile (juce::String ("Mine ") + half + ".json.shipped")
          .replaceWithText ("{}");
    }
  EXPECT_EQ (a3::listFilesIn (s.dir, ".json").size (), 2u);
}
