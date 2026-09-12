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

#include <a3-motion-ui/theme/TextInput.hh>

using namespace a3;

namespace
{

// Typing a name with one encoder: turning moves along the name, pressing
// arms a position, turning then walks its letter through the alphabet.

TEST (TextInput, ItStartsFromTheNameItIsGiven)
{
  TextInput entry{ "neutral" };

  EXPECT_EQ (entry.name (), "neutral");
  EXPECT_EQ (entry.cursor (), 7) << "the caret starts after the text, the way "
                                    "every text field does — typing carries "
                                    "on rather than overwriting the front";
}

TEST (TextInput, TheCursorStopsAtBothEnds)
{
  TextInput entry{ "abc" };

  entry.moveCursor (-5);
  EXPECT_EQ (entry.cursor (), 0);

  entry.moveCursor (500);
  EXPECT_EQ (entry.cursor (), TextInput::maxLength - 1);
}

TEST (TextInput, ALetterWalksThroughTheAlphabet)
{
  TextInput entry{ "neutral" };
  entry.moveCursor (-7); // to the first letter

  entry.changeCharacter (1); // n -> o

  EXPECT_EQ (entry.name (), "oeutral");
}

// Shortening a name has to be possible with the same one control: turning a
// letter down past 'a' blanks it, and the name ends there.
TEST (TextInput, BlankingAPositionCutsTheNameShort)
{
  TextInput entry{ "neutral" };

  entry.moveCursor (-4); // from the end back to the fourth letter
  while (entry.characterAtCursor () != ' ')
    entry.changeCharacter (-1);

  EXPECT_EQ (entry.name (), "neu");
}

TEST (TextInput, TypingPastTheEndMakesTheNameLonger)
{
  TextInput entry{ "neu" };

  EXPECT_EQ (entry.characterAtCursor (), ' ') << "past the end is blank";

  entry.changeCharacter (1); // blank -> a

  EXPECT_EQ (entry.name (), "neua");
}

// The alphabet holds exactly what a skin name may hold, so nothing that is
// dialled in can produce an unusable name.
TEST (TextInput, EveryLetterItCanReachIsAllowedInAName)
{
  TextInput entry{ "a" };
  entry.moveCursor (-1);

  for (int i = 0; i < 200; ++i)
    {
      entry.changeCharacter (1);
      auto const name = entry.name ();
      if (name.isNotEmpty ())
        EXPECT_TRUE (isUsableSkinName (name)) << name.toStdString ();
    }
}

TEST (TextInput, TheAlphabetDoesNotWrapPastItsEnds)
{
  TextInput entry{ "a" };
  entry.moveCursor (-1);

  entry.changeCharacter (-5);
  EXPECT_EQ (entry.characterAtCursor (), ' ');

  entry.changeCharacter (500);
  EXPECT_NE (entry.characterAtCursor (), ' ');
  EXPECT_TRUE (isUsableSkinName (entry.name ()));
}

// A name longer than the row it is shown in helps nobody, and a file name
// this long is a sign something went wrong rather than a long name.
TEST (TextInput, ANameCannotGrowPastTheLimit)
{
  TextInput entry{ "abcdefghijklmnop" };

  EXPECT_LE (entry.name ().length (), TextInput::maxLength);
}


// The same name, typed on the touchscreen instead of dialled in. A key
// appends at the cursor and moves it on, so typing reads left to right the
// way it looks.

TEST (TextInput, TypingAKeyAppendsAndMovesOn)
{
  TextInput entry{ "" };

  entry.type ('a');
  entry.type ('b');

  EXPECT_EQ (entry.name (), "ab");
  EXPECT_EQ (entry.cursor (), 2);
}

TEST (TextInput, TypingOverwritesFromTheCursor)
{
  TextInput entry{ "neutral" };

  entry.moveCursor (-7); // to the first letter
  entry.type ('s');

  EXPECT_EQ (entry.name (), "seutral");
}

TEST (TextInput, BackspaceTakesTheCharacterBeforeTheCursor)
{
  TextInput entry{ "neu" };

  entry.backspace ();

  EXPECT_EQ (entry.name (), "ne");
  EXPECT_EQ (entry.cursor (), 2);
}

TEST (TextInput, BackspaceAtTheStartDoesNothing)
{
  TextInput entry{ "neu" };
  entry.moveCursor (-3);

  entry.backspace ();

  EXPECT_EQ (entry.name (), "neu");
  EXPECT_EQ (entry.cursor (), 0);
}

// A key that a name may not hold is not typed at all — the keyboard offers
// none, but nothing else may sneak one in either.
TEST (TextInput, AKeyOutsideTheAlphabetIsIgnored)
{
  TextInput entry{ "neu" };
  entry.moveCursor (3);

  entry.type ('/');
  entry.type ('A');

  EXPECT_EQ (entry.name (), "neu");
}

TEST (TextInput, TypingStopsAtTheLimit)
{
  TextInput entry{ "" };

  for (int i = 0; i < TextInput::maxLength + 5; ++i)
    entry.type ('a');

  EXPECT_EQ (entry.name ().length (), TextInput::maxLength);
}

}
