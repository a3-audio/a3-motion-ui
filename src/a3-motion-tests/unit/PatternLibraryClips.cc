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
// how it is played, and loading has to bring both. Without this the settings
// would live in a file nobody reads.
TEST (PatternLibraryClips, LoadingAClipBringsItsSettingsWithIt)
{
  auto const root = aRootHolding ("a3-library-clips", "16_Wave.svg");

  Clip clip;
  clip.name = "Wave";
  clip.svg = "16_Wave";
  clip.settings.spin = 3;
  clip.settings.speedLog2 = -2;
  clip.settings.endAction = EndAction::Bounce;
  ASSERT_TRUE (ClipFile::save (clip, root.getChildFile ("clips/Wave.json")));

  PatternLibrary library (root);
  library.refresh ();

  auto const index = library.indexForName ("Wave");
  ASSERT_GT (index, 0) << "the clip's shape is not in the library";

  auto const pattern = library.loadPattern (index);
  ASSERT_NE (pattern, nullptr);
  EXPECT_EQ (pattern->getSpin (), 3);
  EXPECT_EQ (pattern->getSpeedLog2 (), -2);
  EXPECT_EQ (pattern->getEndAction (), EndAction::Bounce);

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

// A clip naming a shape that is not there must not invent one. The slot stays
// empty and the library says nothing is reachable under that name.
TEST (PatternLibraryClips, AClipPointingAtNothingReachesNothing)
{
  auto const root = aRootHolding ("a3-library-dangling", "16_Wave.svg");

  Clip clip;
  clip.name = "Ghost";
  clip.svg = "16_NotHere";
  ASSERT_TRUE (ClipFile::save (clip, root.getChildFile ("clips/Ghost.json")));

  PatternLibrary library (root);
  library.refresh ();

  EXPECT_LE (library.indexForName ("Ghost"), 0)
      << "a clip whose shape is missing must not appear playable";

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
  EXPECT_EQ (entry.category, PatternLibrary::Category::Settings);
  EXPECT_TRUE (entry.svgPathData.empty ())
      << "a settings preset has no shape to draw";
  EXPECT_TRUE (entry.clipFile.existsAsFile ())
      << "the entry has to say which file its values come from";
}

// The two passes must not overlap: a clip that names a shape is already in the
// list through that shape, and listing it again would put the same clip in the
// browser twice under one name.
TEST (PatternLibraryClips, AClipWithAShapeIsListedOnce)
{
  auto const root = aRootHolding ("a3-library-once", "16_Wave.svg");

  Clip clip;
  clip.name = "Wave";
  clip.svg = "16_Wave";
  ASSERT_TRUE (ClipFile::save (clip, root.getChildFile ("clips/Wave.json")));

  PatternLibrary library (root);
  library.refresh ();

  int found = 0;
  for (int i = 1; i < library.getNumEntries (); ++i)
    if (library.getEntry (i).name == "Wave")
      ++found;

  EXPECT_EQ (found, 1);
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
  shaped.name = "Wave";
  shaped.svg = "16_Wave";
  ASSERT_TRUE (ClipFile::save (shaped, root.getChildFile ("clips/Wave.json")));

  Clip preset;
  preset.name = "Breathe";
  ASSERT_TRUE (
      ClipFile::save (preset, root.getChildFile ("clips/Breathe.json")));

  PatternLibrary library (root);
  library.refresh ();

  EXPECT_EQ (library.indexForClipFile (root.getChildFile ("clips/Wave.json")),
             library.indexForName ("Wave"));
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
