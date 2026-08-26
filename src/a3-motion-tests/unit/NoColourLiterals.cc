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

#include <algorithm>
#include <set>

namespace
{

// A skin can only reach what asks the theme for its colour. Every literal left
// in a component is a spot the skin does not cover — and one that will not
// announce itself: the app still builds, still runs, and merely ignores the
// skin in that one place.
//
// The list below is what has not been migrated yet, and it is a ratchet. A file
// missing from it may hold no literal; a file on it must still hold one, so an
// entry cannot be left behind after its file is clean. It shrinks to nothing as
// the migration proceeds, and this test is what keeps it from growing again.

char const *const filesStillHoldingLiterals[] = {
  "components/A3MotionUIComponent.cc",
  "components/DirectivityComponent.cc",
  "components/ElevationDisplay.cc",
  "components/ElevationDisplay.hh",
  "components/FilterDisplay.hh",
  "components/LoopLengthDisplay.cc",
  "components/LoopLengthDisplay.hh",
  "components/MotionComponent.cc",
  "components/PadRowDisplay.cc",
  "components/PadRowDisplay.hh",
  "components/TickIndicator.cc",
  "components/TrajectoryIcon.cc",
  "io/InputOutputAdapterV3.cc",
};

/** A named JUCE colour, or a juce::Colour built from a number.
 *
 *  transparentBlack and transparentWhite are not colour choices — they are the
 *  way JUCE says "no colour", and stay allowed everywhere. */
bool
lineHoldsAColourLiteral (juce::String const &line)
{
  auto const trimmed = line.trimStart ();
  if (trimmed.startsWith ("//") || trimmed.startsWith ("*"))
    return false; // a literal named in a comment is documentation

  if (trimmed.contains ("juce::Colours::")
      && !trimmed.contains ("juce::Colours::transparent"))
    return true;

  auto const afterCtor = trimmed.fromFirstOccurrenceOf ("juce::Colour (", false,
                                                        false);

  return afterCtor.isNotEmpty ()
         && (afterCtor.startsWith ("0x")
             || juce::CharacterFunctions::isDigit (afterCtor[0]));
}

juce::File
uiSourceDir ()
{
  return juce::File (A3_UI_SOURCE_DIR);
}

std::set<juce::String>
filesWithLiterals ()
{
  std::set<juce::String> found;
  auto const root = uiSourceDir ();

  for (auto const &entry : juce::RangedDirectoryIterator (
           root, true, "*.cc;*.hh", juce::File::findFiles))
    {
      auto const path
          = entry.getFile ().getRelativePathFrom (root).replace ("\\", "/");

      // The theme is where the built-in defaults live; literals are its job.
      if (path.startsWith ("theme/"))
        continue;

      juce::StringArray lines;
      lines.addLines (entry.getFile ().loadFileAsString ());

      for (auto const &line : lines)
        if (lineHoldsAColourLiteral (line))
          {
            found.insert (path);
            break;
          }
    }

  return found;
}

TEST (NoColourLiterals, NoFileOutsideTheListHoldsOne)
{
  std::set<juce::String> allowed (std::begin (filesStillHoldingLiterals),
                                  std::end (filesStillHoldingLiterals));

  for (auto const &path : filesWithLiterals ())
    EXPECT_TRUE (allowed.count (path) > 0)
        << path << " holds a colour literal the skin cannot reach. Take it "
                   "from the theme, or add the file to the list if it is "
                   "waiting its turn.";
}

TEST (NoColourLiterals, TheListHasNoStaleEntries)
{
  auto const found = filesWithLiterals ();

  for (auto const *path : filesStillHoldingLiterals)
    EXPECT_TRUE (found.count (juce::String (path)) > 0)
        << path << " is clean but still on the list. Remove the entry — that "
                   "is what makes the list shrink.";
}

// Without this the two tests above could both pass over an empty tree — a
// wrong A3_UI_SOURCE_DIR, a renamed folder — and report the migration finished.
TEST (NoColourLiterals, TheSourcesAreActuallyBeingRead)
{
  ASSERT_TRUE (uiSourceDir ().isDirectory ())
      << uiSourceDir ().getFullPathName ();

  int count = 0;
  for (auto const &entry : juce::RangedDirectoryIterator (
           uiSourceDir (), true, "*.cc", juce::File::findFiles))
    {
      juce::ignoreUnused (entry);
      ++count;
    }

  EXPECT_GT (count, 20) << "far too few sources scanned to trust the result";
}

}
