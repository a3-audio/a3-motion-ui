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

#include <a3-motion-ui/theme/SkinNameEntry.hh>

using namespace a3;

namespace
{

// Typing a name with one encoder: turning moves along the name, pressing
// arms a position, turning then walks its letter through the alphabet.

TEST (SkinNameEntry, ItStartsFromTheNameItIsGiven)
{
  SkinNameEntry entry{ "neutral" };

  EXPECT_EQ (entry.name (), "neutral");
  EXPECT_EQ (entry.cursor (), 0);
}

TEST (SkinNameEntry, TheCursorStopsAtBothEnds)
{
  SkinNameEntry entry{ "abc" };

  entry.moveCursor (-5);
  EXPECT_EQ (entry.cursor (), 0);

  entry.moveCursor (500);
  EXPECT_EQ (entry.cursor (), SkinNameEntry::maxLength - 1);
}

TEST (SkinNameEntry, ALetterWalksThroughTheAlphabet)
{
  SkinNameEntry entry{ "neutral" };

  entry.changeCharacter (1); // n -> o

  EXPECT_EQ (entry.name (), "oeutral");
}

// Shortening a name has to be possible with the same one control: turning a
// letter down past 'a' blanks it, and the name ends there.
TEST (SkinNameEntry, BlankingAPositionCutsTheNameShort)
{
  SkinNameEntry entry{ "neutral" };

  entry.moveCursor (3);
  while (entry.characterAtCursor () != ' ')
    entry.changeCharacter (-1);

  EXPECT_EQ (entry.name (), "neu");
}

TEST (SkinNameEntry, TypingPastTheEndMakesTheNameLonger)
{
  SkinNameEntry entry{ "neu" };

  entry.moveCursor (3);
  EXPECT_EQ (entry.characterAtCursor (), ' ') << "past the end is blank";

  entry.changeCharacter (1); // blank -> a

  EXPECT_EQ (entry.name (), "neua");
}

// The alphabet holds exactly what a skin name may hold, so nothing that is
// dialled in can produce an unusable name.
TEST (SkinNameEntry, EveryLetterItCanReachIsAllowedInAName)
{
  SkinNameEntry entry{ "a" };

  for (int i = 0; i < 200; ++i)
    {
      entry.changeCharacter (1);
      auto const name = entry.name ();
      if (name.isNotEmpty ())
        EXPECT_TRUE (isUsableSkinName (name)) << name.toStdString ();
    }
}

TEST (SkinNameEntry, TheAlphabetDoesNotWrapPastItsEnds)
{
  SkinNameEntry entry{ "a" };

  entry.changeCharacter (-5);
  EXPECT_EQ (entry.characterAtCursor (), ' ');

  entry.changeCharacter (500);
  EXPECT_NE (entry.characterAtCursor (), ' ');
  EXPECT_TRUE (isUsableSkinName (entry.name ()));
}

// A name longer than the row it is shown in helps nobody, and a file name
// this long is a sign something went wrong rather than a long name.
TEST (SkinNameEntry, ANameCannotGrowPastTheLimit)
{
  SkinNameEntry entry{ "abcdefghijklmnop" };

  EXPECT_LE (entry.name ().length (), SkinNameEntry::maxLength);
}

}
