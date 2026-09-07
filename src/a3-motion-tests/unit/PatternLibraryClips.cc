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

#include <a3-motion-engine/ClipFile.hh>
#include <a3-motion-engine/PatternLibrary.hh>

using namespace a3;

namespace
{
/** A library root with one shape in it, and nothing else. */
juce::File
aRootHolding (juce::String const &dirName, juce::String const &shapeFileName)
{
  auto const root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile (dirName);
  root.deleteRecursively ();
  root.getChildFile ("system").createDirectory ();

  // data-name is what the library keys an entry on -- a file without one is
  // skipped as invalid, which is worth knowing when writing fixtures.
  auto const name = shapeFileName.fromFirstOccurrenceOf ("_", false, false)
                        .upToLastOccurrenceOf (".svg", false, false);

  root.getChildFile ("system").getChildFile (shapeFileName).replaceWithText (
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"-1 -1 2 2\""
      " data-name=\""
      + name
      + "\" data-beats=\"16\" data-ppqn=\"128\">"
        "<path d=\"M -0.5 0 C -0.25 0.5 0.25 -0.5 0.5 0\"/></svg>");

  return root;
}
}

// The point of the split: the shape says where the sound goes, the clip says
// how it is played *and* which shape that is. Both halves are reachable from
// the clip's own row -- the values off its file, the figure by the name it
// carries -- which is what lets one tap fill a slot with the whole thing.
TEST (PatternLibraryClips, AClipCarriesItsValuesAndNamesItsShape)
{
  auto const root = aRootHolding ("a3-library-clips", "16_Wave.svg");

  Clip clip;
  clip.name = "Wave slow";
  clip.svg = "Wave";
  clip.settings.spin = 3;
  clip.settings.speedLog2 = -2;
  clip.settings.endAction = EndAction::Bounce;
  ASSERT_TRUE (
      ClipFile::save (clip, root.getChildFile ("clips/Wave slow.json")));

  PatternLibrary library (root);
  library.refresh ();

  auto const index = library.indexForName ("Wave slow");
  ASSERT_GT (index, 0) << "the clip is not in the library";

  auto const &entry = library.getEntry (index);
  EXPECT_EQ (entry.category, PatternLibrary::Category::Clip);
  EXPECT_EQ (entry.svg, "Wave");

  // The values, off the file the entry points at.
  auto const read = ClipFile::load (entry.clipFile);
  ASSERT_TRUE (read.has_value ());
  EXPECT_EQ (read->settings.spin, 3);
  EXPECT_EQ (read->settings.speedLog2, -2);
  EXPECT_EQ (read->settings.endAction, EndAction::Bounce);

  // The figure, by the name it carries.
  auto const shape = library.indexForName (entry.svg);
  ASSERT_GT (shape, 0) << "the shape the clip names is not reachable";
  EXPECT_NE (library.loadPattern (shape), nullptr);

  root.deleteRecursively ();
}

// A shape with no clip beside it still loads. Somebody dropping an SVG into
// the folder by hand is a reasonable thing to do, and it should show up.
TEST (PatternLibraryClips, AShapeWithoutAClipStillLoads)
{
  auto const root = aRootHolding ("a3-library-noclip", "16_Bare.svg");

  PatternLibrary library (root);
  library.refresh ();

  auto const index = library.indexForName ("Bare");
  ASSERT_GT (index, 0);

  auto const pattern = library.loadPattern (index);
  ASSERT_NE (pattern, nullptr);
  EXPECT_EQ (pattern->getSpin (), ClipSettings{}.spin)
      << "no clip means the defaults, not something else";

  root.deleteRecursively ();
}

// A clip naming a shape that is not there is still a file somebody has, so it
// is still a row. Hiding it would be a file that vanished because one of the
// names in it went -- and it is the browser's job to show what is there. What
// it must not do is invent a figure: the name it carries resolves to nothing,
// and applying it leaves the slot's own figure alone.
TEST (PatternLibraryClips, AClipPointingAtNothingIsStillListed)
{
  auto const root = aRootHolding ("a3-library-dangling", "16_Wave.svg");

  Clip clip;
  clip.name = "Ghost";
  clip.svg = "NotHere";
  ASSERT_TRUE (ClipFile::save (clip, root.getChildFile ("clips/Ghost.json")));

  PatternLibrary library (root);
  library.refresh ();

  auto const index = library.indexForName ("Ghost");
  ASSERT_GT (index, 0) << "a clip is a file somebody has, and rows show those";
  EXPECT_EQ (library.getEntry (index).svg, "NotHere");
  EXPECT_LE (library.indexForName ("NotHere"), 0)
      << "the missing shape must not be invented";

  root.deleteRecursively ();
}

// A clip with no shape is a settings preset: it says how a slot is played and
// leaves what it plays alone. The library is otherwise shape-first -- one
// entry per SVG file -- so without this pass a settings preset is a file
// nothing enumerates, which is exactly how twenty generated presets ended up
// invisible in the browser.
TEST (PatternLibraryClips, ASettingsPresetIsListedWithoutAShape)
{
  auto const root = aRootHolding ("a3-library-settings", "16_Wave.svg");

  Clip preset;
  preset.name = "Breathe";
  preset.settings.reachLfo = 4;
  preset.settings.envelopeAttack = 6;
  ASSERT_TRUE (
      ClipFile::save (preset, root.getChildFile ("clips/Breathe.json")));

  PatternLibrary library (root);
  library.refresh ();

  auto const index = library.indexForName ("Breathe");
  ASSERT_GT (index, 0) << "a clip without a shape never reaches the list";

  auto const &entry = library.getEntry (index);
  EXPECT_EQ (entry.category, PatternLibrary::Category::Clip);
  EXPECT_TRUE (entry.svgPathData.empty ())
      << "a settings preset has no shape to draw";
  EXPECT_TRUE (entry.clipFile.existsAsFile ())
      << "the entry has to say which file its values come from";
}

// A clip and the shape it names are two entries, on two lists: the browser
// shows them under two tabs, and choosing one does a different thing from
// choosing the other. The clip used to be swallowed by the shape, which made
// what a row did depend on which row it was.
TEST (PatternLibraryClips, AClipAndTheShapeItNamesAreBothListed)
{
  auto const root = aRootHolding ("a3-library-both", "16_Wave.svg");

  Clip clip;
  clip.name = "Wave slow";
  clip.svg = "Wave";
  ASSERT_TRUE (
      ClipFile::save (clip, root.getChildFile ("clips/Wave slow.json")));

  PatternLibrary library (root);
  library.refresh ();

  auto const shape = library.indexForName ("Wave");
  auto const asClip = library.indexForName ("Wave slow");
  ASSERT_GT (shape, 0);
  ASSERT_GT (asClip, 0);
  EXPECT_NE (shape, asClip);

  EXPECT_EQ (library.getEntry (shape).category,
             PatternLibrary::Category::System);
  EXPECT_EQ (library.getEntry (asClip).category,
             PatternLibrary::Category::Clip);

  // The clip says which figure it is played on, by the name the library
  // resolves -- the shape is not asked to know about the clip.
  EXPECT_EQ (library.getEntry (asClip).svg, "Wave");
  EXPECT_TRUE (library.getEntry (shape).clipFile == juce::File{})
      << "a shape no longer goes looking for a clip named after its file";

  // Each file once.
  int rows = 0;
  for (int i = 1; i < library.getNumEntries (); ++i)
    if (library.getEntry (i).name == "Wave"
        || library.getEntry (i).name == "Wave slow")
      ++rows;
  EXPECT_EQ (rows, 2);
}

// The highlight in the browser says which of a hundred rows a slot came from.
// It cannot be worked out from the shape's name once settings presets exist: a
// preset leaves the shape alone, so every slot would point back at the shape
// row instead of the preset just chosen -- and the list would scroll away from
// the presets after every pick. The slot's clip file is what actually says it.
TEST (PatternLibraryClips, AClipFileSaysWhichRowItCameFrom)
{
  auto const root = aRootHolding ("a3-library-rows", "16_Wave.svg");

  Clip shaped;
  shaped.name = "Wave slow";
  shaped.svg = "Wave";
  ASSERT_TRUE (
      ClipFile::save (shaped, root.getChildFile ("clips/Wave slow.json")));

  Clip preset;
  preset.name = "Breathe";
  ASSERT_TRUE (
      ClipFile::save (preset, root.getChildFile ("clips/Breathe.json")));

  PatternLibrary library (root);
  library.refresh ();

  EXPECT_EQ (
      library.indexForClipFile (root.getChildFile ("clips/Wave slow.json")),
      library.indexForName ("Wave slow"));
  EXPECT_EQ (
      library.indexForClipFile (root.getChildFile ("clips/Breathe.json")),
      library.indexForName ("Breathe"));
  EXPECT_EQ (library.indexForClipFile (juce::File{}), 0)
      << "a slot that came from no clip points at no row";
  EXPECT_EQ (library.indexForClipFile (root.getChildFile ("clips/Gone.json")),
             0);
}

// The app polls this fingerprint and reloads the library when it changes, so
// a file dropped into the folder shows up without a restart. Clips have to be
// in it or settings presets are the one kind of entry you cannot add while the
// device is running -- which is exactly the kind you make most of.
TEST (PatternLibraryClips, TheFingerprintNoticesAClip)
{
  auto const root = aRootHolding ("a3-library-fingerprint", "16_Wave.svg");

  PatternLibrary library (root);
  library.refresh ();

  auto const before = library.getDirectoryFingerprint ();

  Clip preset;
  preset.name = "Breathe";
  ASSERT_TRUE (
      ClipFile::save (preset, root.getChildFile ("clips/Breathe.json")));

  auto const added = library.getDirectoryFingerprint ();
  EXPECT_NE (added, before) << "a new preset went unnoticed";

  ASSERT_TRUE (root.getChildFile ("clips/Breathe.json").deleteFile ());
  EXPECT_NE (library.getDirectoryFingerprint (), added)
      << "a deleted preset went unnoticed";
}

// The list is read, so it has to be sorted by what is read. Shapes were sorted
// by file name, and a shape's file name carries its beat count -- 04_Zigzag
// stood before 16_Arc, which looks like no order at all to anyone who cannot
// see the prefix. Sorted within each category, so the grouping the row's dot
// makes visible survives.
TEST (PatternLibraryClips, EachCategoryIsSortedByTheNameThatIsShown)
{
  auto const root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-library-sorted");
  root.deleteRecursively ();
  root.getChildFile ("system").createDirectory ();

  auto const shape = [&root] (juce::String const &fileName,
                              juce::String const &name) {
    root.getChildFile ("system").getChildFile (fileName).replaceWithText (
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"-1 -1 2 2\""
        " data-name=\""
        + name
        + "\" data-beats=\"16\" data-ppqn=\"128\">"
          "<path d=\"M -0.5 0 L 0.5 0\"/></svg>");
  };

  shape ("04_Zigzag.svg", "Zigzag");
  shape ("16_Arc.svg", "Arc");
  shape ("08_Muster.svg", "Muster");

  Clip preset;
  preset.name = "Breathe";
  ASSERT_TRUE (
      ClipFile::save (preset, root.getChildFile ("clips/Breathe.json")));
  preset.name = "Anvil";
  ASSERT_TRUE (ClipFile::save (preset, root.getChildFile ("clips/Anvil.json")));

  PatternLibrary library (root);
  library.refresh ();

  std::vector<std::string> shown;
  for (int i = 1; i < library.getNumEntries (); ++i)
    shown.push_back (library.getEntry (i).name);

  ASSERT_EQ (shown.size (), 5u);

  // Shapes first, in the order they read; the presets after them, likewise.
  EXPECT_EQ (shown[0], "Arc");
  EXPECT_EQ (shown[1], "Muster");
  EXPECT_EQ (shown[2], "Zigzag");
  EXPECT_EQ (shown[3], "Anvil");
  EXPECT_EQ (shown[4], "Breathe");
}
