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

#include <a3-motion-ui/components/ScriptBuffer.hh>

using namespace a3;

// ── Holding text ─────────────────────────────────────────────────────────

TEST (ScriptBuffer, ItStartsEmptyWithOneLine)
{
  ScriptBuffer buffer;

  EXPECT_EQ (buffer.numLines (), 1);
  EXPECT_EQ (buffer.text (), "");
  EXPECT_EQ (buffer.caretLine (), 0);
  EXPECT_EQ (buffer.caretColumn (), 0);
}

TEST (ScriptBuffer, TextComesApartIntoLinesAndBackTogether)
{
  ScriptBuffer buffer ("~spin = 4;\n~reach = 0.5;\n");

  EXPECT_EQ (buffer.numLines (), 3) << "the trailing newline opens a line";
  EXPECT_EQ (buffer.line (0), "~spin = 4;");
  EXPECT_EQ (buffer.line (1), "~reach = 0.5;");
  EXPECT_EQ (buffer.text (), "~spin = 4;\n~reach = 0.5;\n");
}

// Whichever way the file was written. A script edited on a desk and carried
// over on a stick must not gain a row of stray characters.
TEST (ScriptBuffer, WindowsLineEndingsAreLineEndings)
{
  ScriptBuffer buffer ("~spin = 4;\r\n~reach = 0.5;");

  EXPECT_EQ (buffer.numLines (), 2);
  EXPECT_EQ (buffer.line (0), "~spin = 4;");
}

// ── Typing ───────────────────────────────────────────────────────────────

TEST (ScriptBuffer, TypingPutsCharactersAtTheCaret)
{
  ScriptBuffer buffer;

  for (auto const c : juce::String ("~spin"))
    buffer.type (c);

  EXPECT_EQ (buffer.text (), "~spin");
  EXPECT_EQ (buffer.caretColumn (), 5);
}

TEST (ScriptBuffer, ReturnSplitsTheLineAtTheCaret)
{
  ScriptBuffer buffer ("~spin = 4;");
  buffer.placeCaret (0, 5);
  buffer.type ('\n');

  EXPECT_EQ (buffer.numLines (), 2);
  EXPECT_EQ (buffer.line (0), "~spin");
  EXPECT_EQ (buffer.line (1), " = 4;");
  EXPECT_EQ (buffer.caretLine (), 1);
  EXPECT_EQ (buffer.caretColumn (), 0);
}

TEST (ScriptBuffer, BackspaceTakesTheCharacterBefore)
{
  ScriptBuffer buffer ("~spin");
  buffer.placeCaret (0, 5);
  buffer.backspace ();

  EXPECT_EQ (buffer.text (), "~spi");
  EXPECT_EQ (buffer.caretColumn (), 4);
}

// At the start of a line it joins that line to the one above, with the caret
// landing where the join is -- which is where the next character goes.
TEST (ScriptBuffer, BackspaceAtTheStartOfALineJoinsIt)
{
  ScriptBuffer buffer ("~spin\n= 4;");
  buffer.placeCaret (1, 0);
  buffer.backspace ();

  EXPECT_EQ (buffer.numLines (), 1);
  EXPECT_EQ (buffer.text (), "~spin= 4;");
  EXPECT_EQ (buffer.caretLine (), 0);
  EXPECT_EQ (buffer.caretColumn (), 5);
}

TEST (ScriptBuffer, BackspaceAtTheVeryStartDoesNothing)
{
  ScriptBuffer buffer ("~spin");
  buffer.placeCaret (0, 0);
  buffer.backspace ();

  EXPECT_EQ (buffer.text (), "~spin");
}

// ── Moving about ─────────────────────────────────────────────────────────

TEST (ScriptBuffer, TheCaretWalksTheLinesAndTheColumns)
{
  ScriptBuffer buffer ("one\ntwo\nthree");
  buffer.placeCaret (0, 0);

  buffer.moveCaret (1, 0);
  EXPECT_EQ (buffer.caretLine (), 1);

  buffer.moveCaret (0, 2);
  EXPECT_EQ (buffer.caretColumn (), 2);

  buffer.moveCaret (-1, 0);
  EXPECT_EQ (buffer.caretLine (), 0);
}

// Off the end of a short line it lands on that line's end rather than in
// mid-air, which is what every editor does and what a finger expects.
TEST (ScriptBuffer, MovingOntoAShorterLineLandsOnItsEnd)
{
  ScriptBuffer buffer ("a longer line\nab");
  buffer.placeCaret (0, 13);

  buffer.moveCaret (1, 0);
  EXPECT_EQ (buffer.caretLine (), 1);
  EXPECT_EQ (buffer.caretColumn (), 2);
}

TEST (ScriptBuffer, TheCaretCannotLeaveTheText)
{
  ScriptBuffer buffer ("one\ntwo");

  buffer.placeCaret (0, 0);
  buffer.moveCaret (-5, -5);
  EXPECT_EQ (buffer.caretLine (), 0);
  EXPECT_EQ (buffer.caretColumn (), 0);

  buffer.moveCaret (9, 9);
  EXPECT_EQ (buffer.caretLine (), 1);
  EXPECT_EQ (buffer.caretColumn (), 3);
}

TEST (ScriptBuffer, PlacingTheCaretPastTheEndClampsToIt)
{
  ScriptBuffer buffer ("one\ntwo");
  buffer.placeCaret (7, 40);

  EXPECT_EQ (buffer.caretLine (), 1);
  EXPECT_EQ (buffer.caretColumn (), 3);
}

// ── Scrolling ────────────────────────────────────────────────────────────

// The window follows the caret and moves as little as it can: a line brought
// into view from below scrolls by one, not into the middle. Jumping the text
// under a finger that is typing is how you lose your place.
TEST (ScriptBuffer, TheWindowFollowsTheCaretByAsLittleAsPossible)
{
  ScriptBuffer buffer;
  for (int i = 0; i < 20; ++i)
    {
      buffer.type ('x');
      buffer.type ('\n');
    }

  buffer.placeCaret (0, 0);
  buffer.bringCaretIntoView (5);
  EXPECT_EQ (buffer.firstVisibleLine (), 0);

  buffer.placeCaret (5, 0);
  buffer.bringCaretIntoView (5);
  EXPECT_EQ (buffer.firstVisibleLine (), 1) << "scrolled further than it had to";

  buffer.placeCaret (4, 0);
  buffer.bringCaretIntoView (5);
  EXPECT_EQ (buffer.firstVisibleLine (), 1) << "already in view, so it stayed";

  buffer.placeCaret (0, 0);
  buffer.bringCaretIntoView (5);
  EXPECT_EQ (buffer.firstVisibleLine (), 0);
}

TEST (ScriptBuffer, ScrollingByHandStaysInsideTheText)
{
  ScriptBuffer buffer ("one\ntwo\nthree\nfour");

  buffer.scrollBy (100, 2);
  EXPECT_LE (buffer.firstVisibleLine (), 3);
  EXPECT_GE (buffer.firstVisibleLine (), 0);

  buffer.scrollBy (-100, 2);
  EXPECT_EQ (buffer.firstVisibleLine (), 0);
}

// ── Knowing when it has been touched ─────────────────────────────────────

// The page marks an edited script and the file is written from that, so a
// buffer that could not say whether it had changed would be one that either
// wrote on every keystroke or lost the last one.
TEST (ScriptBuffer, ItKnowsWhetherItHasBeenEdited)
{
  ScriptBuffer buffer ("~spin = 4;");
  EXPECT_FALSE (buffer.isEdited ());

  buffer.type ('x');
  EXPECT_TRUE (buffer.isEdited ());

  buffer.markSaved ();
  EXPECT_FALSE (buffer.isEdited ());

  buffer.placeCaret (0, 4);
  EXPECT_FALSE (buffer.isEdited ()) << "moving about is not an edit";

  // Somewhere there is actually something to take: at the very start
  // backspace does nothing, and doing nothing is not an edit either.
  buffer.backspace ();
  EXPECT_TRUE (buffer.isEdited ());
}

TEST (ScriptBuffer, LoadingNewTextIsNotAnEdit)
{
  ScriptBuffer buffer ("~spin = 4;");
  buffer.type ('x');
  ASSERT_TRUE (buffer.isEdited ());

  buffer.setText ("~reach = 0.5;");
  EXPECT_FALSE (buffer.isEdited ());
  EXPECT_EQ (buffer.text (), "~reach = 0.5;");
  EXPECT_EQ (buffer.caretLine (), 0);
}
