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

#include <set>
#include <vector>

namespace
{

// A skin can only reach what asks the theme for its size. Every radius,
// inset or alpha left as a number in a component is a spot the skin does not
// cover — and one that will not announce itself: the app still builds, still
// runs, and merely ignores the skin in that one place.
//
// The list below is what has not been migrated yet. Like the colour ratchet
// next door it holds in both directions: a file missing from it may hold no
// literal, and a file on it must still hold one, so an entry cannot outlive
// its reason.

std::vector<char const *> const filesStillHoldingMetrics = {
  "components/A3MotionUIComponent.cc",
  "components/ActionLayout.cc",
  "components/BarKnob.cc",
  "components/ClipSettingsLayout.cc",
  "components/ColourPickerComponent.cc",
  "components/ControllerComponent.cc",
  "components/ElevationDisplay.cc",
  "components/FilterDisplay.cc",
  "components/GlobalSettingsComponent.cc",
  "components/LoopLengthDisplay.cc",
  "components/MotionComponent.cc",
  "components/OverlayButtons.cc",
  "components/SkinEditorComponent.cc",
  "components/StatusBar.cc",
};

/** The arguments of `call` in `text`, split at the commas that belong to it.
 *
 *  Depth-counted, because every second argument here is itself a call:
 *  `bounds.toFloat ()` would otherwise split into two. */
juce::StringArray
argumentsOf (juce::String const &text, juce::String const &call)
{
  auto const after = text.fromFirstOccurrenceOf (call + " (", false, false);
  if (after.isEmpty ())
    return {};

  juce::StringArray arguments;
  juce::String current;
  int depth = 0;

  for (auto character : after)
    {
      if (character == '(')
        ++depth;
      else if (character == ')')
        {
          if (depth == 0)
            break;
          --depth;
        }
      else if (character == ',' && depth == 0)
        {
          arguments.add (current.trim ());
          current = {};
          continue;
        }

      current += juce::String::charToString (character);
    }

  arguments.add (current.trim ());
  return arguments;
}

/** A number written out, rather than a name or an expression.
 *
 *  Zero is not a size: `.reduced (x, 0)` says "not in this axis", which no
 *  skin value should be able to change — `-0.f` is the same "not in this
 *  axis" and stays excluded too. A leading `-` is still a number written
 *  out: `-4.f` is exactly as fixed as `4.f`, only on the other side of
 *  zero. */
bool
isWrittenOutNumber (juce::String const &token)
{
  auto const unsigned_ = token.startsWith ("-") ? token.substring (1) : token;

  if (unsigned_.isEmpty () || !juce::CharacterFunctions::isDigit (unsigned_[0]))
    return false;

  auto const value = token.getFloatValue ();
  return value != 0.f;
}

/** A number written out, or a ternary that chooses between two of them.
 *
 *  `active ? 0.35f : 0.15f` is two bare alphas wearing a condition, not an
 *  expression the skin could ever resolve to a name — so either branch
 *  counts on its own, the same way a bare literal would. Split at depth 0
 *  the way `argumentsOf` splits commas, so a condition or branch that is
 *  itself a call does not confuse the split. */
bool
isOrChoosesAWrittenOutNumber (juce::String const &argument)
{
  if (isWrittenOutNumber (argument))
    return true;

  int depth = 0;
  int questionAt = -1;
  int colonAt = -1;

  for (int i = 0; i < argument.length (); ++i)
    {
      auto const character = argument[i];
      if (character == '(')
        ++depth;
      else if (character == ')')
        --depth;
      else if (depth == 0 && character == '?' && questionAt < 0)
        questionAt = i;
      else if (depth == 0 && character == ':' && questionAt >= 0
               && colonAt < 0)
        colonAt = i;
    }

  if (questionAt < 0 || colonAt < 0)
    return false;

  auto const thenBranch = argument.substring (questionAt + 1, colonAt).trim ();
  auto const elseBranch = argument.substring (colonAt + 1).trim ();

  return isWrittenOutNumber (thenBranch) || isWrittenOutNumber (elseBranch);
}

bool
windowHoldsAMetricLiteral (juce::String const &window)
{
  auto const fill = argumentsOf (window, "fillRoundedRectangle");
  if (fill.size () >= 2
      && isOrChoosesAWrittenOutNumber (fill[fill.size () - 1]))
    return true;

  auto const stroke = argumentsOf (window, "drawRoundedRectangle");
  if (stroke.size () >= 3)
    for (auto const &argument : { stroke[stroke.size () - 1],
                                  stroke[stroke.size () - 2] })
      if (isOrChoosesAWrittenOutNumber (argument))
        return true;

  auto const alpha = argumentsOf (window, "withAlpha");
  if (alpha.size () == 1 && isOrChoosesAWrittenOutNumber (alpha[0]))
    return true;

  auto const tinted = argumentsOf (window, "toColour");
  if (tinted.size () == 2 && isOrChoosesAWrittenOutNumber (tinted[1]))
    return true;

  for (auto const *inset : { "reduced", "expanded" })
    for (auto const &argument : argumentsOf (window, inset))
      if (isOrChoosesAWrittenOutNumber (argument))
        return true;

  return false;
}

juce::File
uiSourceDir ()
{
  return juce::File (A3_UI_SOURCE_DIR);
}

std::set<juce::String>
filesWithMetrics ()
{
  std::set<juce::String> found;
  auto const root = uiSourceDir ();

  for (auto const &entry : juce::RangedDirectoryIterator (
           root, true, "*.cc;*.hh", juce::File::findFiles))
    {
      auto const path
          = entry.getFile ().getRelativePathFrom (root).replace ("\\", "/");

      // theme/ is where a number becomes a role, and the shader carries its
      // own units — neither asks the theme for a pixel.
      if (path.startsWith ("theme/") || path.contains ("Shader"))
        continue;

      juce::StringArray lines;
      lines.addLines (entry.getFile ().loadFileAsString ());

      for (int i = 0; i < lines.size (); ++i)
        {
          auto const trimmed = lines[i].trimStart ();
          if (trimmed.startsWith ("//") || trimmed.startsWith ("*"))
            continue; // a number named in a comment is documentation

          // Three lines at a time: a call wrapped by clang-format is still
          // one call, and every one of these is routinely wrapped.
          juce::String window = lines[i];
          for (int ahead = 1; ahead <= 2 && i + ahead < lines.size (); ++ahead)
            window += " " + lines[i + ahead].trim ();

          if (windowHoldsAMetricLiteral (window))
            {
              found.insert (path);
              break;
            }
        }
    }

  return found;
}

TEST (NoMetricLiterals, NoFileOutsideTheListHoldsOne)
{
  std::set<juce::String> allowed (filesStillHoldingMetrics.begin (),
                                  filesStillHoldingMetrics.end ());

  for (auto const &path : filesWithMetrics ())
    EXPECT_TRUE (allowed.count (path) > 0)
        << path << " holds a metric literal the skin cannot reach. Take it "
                   "from the theme, or add the file to the list if it is "
                   "waiting its turn.";
}

TEST (NoMetricLiterals, TheListHasNoStaleEntries)
{
  auto const found = filesWithMetrics ();

  for (auto const *path : filesStillHoldingMetrics)
    EXPECT_TRUE (found.count (juce::String (path)) > 0)
        << path << " is clean but still on the list. Remove the entry — that "
                   "is what makes the list shrink.";
}

// Without this the two tests above could both pass over an empty tree — a
// wrong A3_UI_SOURCE_DIR, a renamed folder — and report the migration
// finished. Counts files scanned, not files found holding a literal: the
// latter is migration progress and is meant to fall to zero, which would
// make this test fail on a correct setup once the ratchet has done its job.
TEST (NoMetricLiterals, TheSourcesAreActuallyBeingRead)
{
  ASSERT_TRUE (uiSourceDir ().isDirectory ())
      << uiSourceDir ().getFullPathName ();

  int count = 0;
  for (auto const &entry : juce::RangedDirectoryIterator (
           uiSourceDir (), true, "*.cc;*.hh", juce::File::findFiles))
    {
      juce::ignoreUnused (entry);
      ++count;
    }

  EXPECT_GT (count, 20) << "far too few sources scanned to trust the result";
}

}
