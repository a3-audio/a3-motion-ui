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

#include <a3-motion-engine/TextFile.hh>

#include <set>

using namespace a3;

namespace
{

juce::File
scratchFile ()
{
  return juce::File::getSpecialLocation (juce::File::tempDirectory)
      .getChildFile ("a3-text-"
                     + juce::String (
                         juce::Random::getSystemRandom ().nextInt (1000000))
                     + ".txt");
}

}

// juce::File::replaceWithText takes a lineEndings argument that defaults to
// "\r\n". Everything this device writes is read by people and by git on a
// Linux machine, so every one of them wants "\n" -- and the default is what
// put CRLF into two of the shipped action scripts, which then showed up as
// every line changed in a diff that had altered nine numbers.
TEST (TextFile, ItWritesUnixLineEndings)
{
  auto const file = scratchFile ();
  ASSERT_TRUE (writeTextFile (file, "one\ntwo\nthree\n"));

  auto const written = file.loadFileAsString ();
  EXPECT_FALSE (written.containsChar ('\r')) << "carriage returns were written";
  EXPECT_EQ (written, "one\ntwo\nthree\n");

  file.deleteFile ();
}

TEST (TextFile, ItReplacesWhatWasThere)
{
  auto const file = scratchFile ();
  ASSERT_TRUE (writeTextFile (file, "the first text\n"));
  ASSERT_TRUE (writeTextFile (file, "the second\n"));

  EXPECT_EQ (file.loadFileAsString (), "the second\n");

  file.deleteFile ();
}

// ── One writer, so the default cannot come back ──────────────────────────
//
// The default was found and fixed once already, in Theme.cc, with a comment
// saying exactly what it does -- and the other thirteen call sites went on
// writing CRLF for months. A fix in one of fourteen places is not a fix; it
// is a note somebody left.

namespace
{

std::set<juce::String>
filesCallingJuceDirectly ()
{
  std::set<juce::String> found;

  for (auto const *dir : { A3_UI_SOURCE_DIR, A3_ENGINE_SOURCE_DIR })
    {
      juce::File const root (dir);
      for (auto const &entry : juce::RangedDirectoryIterator (
               root, true, "*.cc;*.hh", juce::File::findFiles))
        {
          auto const name = entry.getFile ().getFileName ();

          // The one place that may: it is what everywhere else calls.
          if (name == "TextFile.cc" || name == "TextFile.hh")
            continue;

          juce::StringArray lines;
          lines.addLines (entry.getFile ().loadFileAsString ());

          for (auto const &line : lines)
            {
              auto const code = line.upToFirstOccurrenceOf ("//", false, false);
              if (code.contains ("replaceWithText") || code.contains ("appendText"))
                found.insert (root.getChildFile (name).getFileName ());
            }
        }
    }

  return found;
}

}

TEST (TextFile, NothingElseWritesTextItself)
{
  auto const found = filesCallingJuceDirectly ();

  juce::StringArray names;
  for (auto const &name : found)
    names.add (name);

  EXPECT_TRUE (found.empty ())
      << "these write text without going through writeTextFile, so they "
         "inherit JUCE's CRLF default: "
      << names.joinIntoString (", ");
}

// Without this the test above passes over an empty tree -- a wrong define, a
// renamed folder -- and reports a migration that never happened.
TEST (TextFile, TheSourcesAreActuallyBeingRead)
{
  int count = 0;
  for (auto const *dir : { A3_UI_SOURCE_DIR, A3_ENGINE_SOURCE_DIR })
    {
      juce::File const root (dir);
      ASSERT_TRUE (root.isDirectory ()) << root.getFullPathName ();

      for (auto const &entry : juce::RangedDirectoryIterator (
               root, true, "*.cc", juce::File::findFiles))
        {
          juce::ignoreUnused (entry);
          ++count;
        }
    }

  EXPECT_GT (count, 30) << "far too few sources scanned to trust the result";
}
