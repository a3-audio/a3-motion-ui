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

#include <a3-motion-ui/components/KeyboardEncoders.hh>

#include <UiSource.hh>

using namespace a3;

namespace
{
using Job = KeyboardEncoderJob;

Job
typing (int column, int row)
{
  return keyboardEncoderJob (true, column, row, false);
}
}

// While KEYS is up (second round, 2026-10-16): E1 and E2 move the cursor,
// up/down and left/right; E5 chooses FILES' folder, E6 the file in it.
TEST (KeyboardEncoders, TheTopTwoAreTheCursorTheBottomTwoTheFiles)
{
  EXPECT_EQ (typing (0, 0), Job::CursorVertical);
  EXPECT_EQ (typing (1, 0), Job::CursorHorizontal);
  EXPECT_EQ (typing (0, 1), Job::Folder);
  EXPECT_EQ (typing (1, 1), Job::File);
}

// The other four do nothing -- they used to turn the hidden page's fields.
TEST (KeyboardEncoders, TheOtherFourDoNothing)
{
  EXPECT_EQ (typing (2, 0), Job::Nothing);
  EXPECT_EQ (typing (3, 0), Job::Nothing);
  EXPECT_EQ (typing (2, 1), Job::Nothing);
  EXPECT_EQ (typing (3, 1), Job::Nothing);
}

// SHIFT + encoder is FREQ / Q of its channel on every page, and the keyboard
// is no exception -- E1 included, which used to move the cursor instead.
TEST (KeyboardEncoders, ShiftTurnsTheChannelPotsOnAllEightIncludingEOne)
{
  for (int column = 0; column < 4; ++column)
    for (int row = 0; row < 2; ++row)
      EXPECT_EQ (keyboardEncoderJob (true, column, row, true), Job::ChannelPot)
          << column << "," << row;
}

TEST (KeyboardEncoders, WithTheKeyboardDownThePageDecides)
{
  for (int column = 0; column < 4; ++column)
    for (int row = 0; row < 2; ++row)
      for (bool shift : { false, true })
        EXPECT_EQ (keyboardEncoderJob (false, column, row, shift),
                   Job::PageControl);
}

// Clockwise is down and right, as on every list and fader of the device.
TEST (KeyboardEncoders, TheCursorFollowsTheTurn)
{
  EXPECT_EQ (cursorKeyOfDetent (Job::CursorVertical, 1),
             juce::KeyPress (juce::KeyPress::downKey));
  EXPECT_EQ (cursorKeyOfDetent (Job::CursorVertical, -2),
             juce::KeyPress (juce::KeyPress::upKey));
  EXPECT_EQ (cursorKeyOfDetent (Job::CursorHorizontal, 1),
             juce::KeyPress (juce::KeyPress::rightKey));
  EXPECT_EQ (cursorKeyOfDetent (Job::CursorHorizontal, -1),
             juce::KeyPress (juce::KeyPress::leftKey));
}

TEST (KeyboardEncoders, EachJobWithASignHasItsGlyph)
{
  EXPECT_EQ (keyboardEncoderGlyph (Job::CursorVertical), KnobGlyph::UpDown);
  EXPECT_EQ (keyboardEncoderGlyph (Job::CursorHorizontal),
             KnobGlyph::LeftRight);
  EXPECT_EQ (keyboardEncoderGlyph (Job::Folder), KnobGlyph::Folder);
  EXPECT_EQ (keyboardEncoderGlyph (Job::File), KnobGlyph::File);
  EXPECT_EQ (keyboardEncoderGlyph (Job::Nothing), KnobGlyph::None);
}

// The component that owns the encoders asks the table, SHIFT included -- the
// cursor check used to run first, so SHIFT + E1 moved the cursor instead of
// turning ch 1 FREQ. Read from the source: the runner cannot build it.
TEST (KeyboardEncoders, TheTurnAsksTheTableWithShift)
{
  auto const body = a3::test::uiComponentBodyOf (
      "A3MotionUIComponent::handleEncoderTurn (int column, int row, int "
      "increment)");
  ASSERT_TRUE (body.isNotEmpty ());

  auto const asked = body.indexOf ("keyboardEncoderJob (");
  ASSERT_GE (asked, 0);
  EXPECT_LT (asked, body.indexOf ("encoderTargetAt ("))
      << "the keyboard's table before the page's";
  EXPECT_TRUE (body.contains ("isButtonPressed (Button::Shift)"));
  EXPECT_FALSE (body.contains ("cursorKeyOfEncoder"));
}

// A press does nothing while the keyboard is up: it used to switch what the
// hidden page's encoder turns.
TEST (KeyboardEncoders, APressDoesNothingWhileTheKeyboardIsUp)
{
  auto const body = a3::test::uiComponentBodyOf (
      "A3MotionUIComponent::handleEncoderPress (int column, int row)");
  ASSERT_TRUE (body.isNotEmpty ());

  auto const guard = body.indexOf ("if (keyboardShown ())");
  ASSERT_GE (guard, 0);
  EXPECT_LT (guard, body.indexOf ("encoderClicksOfPage"));
}

namespace
{
juce::String
body (juce::String const &signature)
{
  return a3::test::uiComponentBodyOf (signature);
}
}

// E5 and E6 act on FILES the way a finger does: the folder through the same
// route as a tap on its tab, the file through the same as a tap on its row.
TEST (KeyboardEncoders, EFiveAndSixGoThroughTheFingersRoutes)
{
  auto const turn = body ("A3MotionUIComponent::handleEncoderTurn (int "
                          "column, int row, int increment)");
  ASSERT_TRUE (turn.isNotEmpty ());
  EXPECT_TRUE (turn.contains ("chooseBrowserList (steppedBrowserList ("));
  EXPECT_TRUE (turn.contains ("steppedEntry ("));
  EXPECT_TRUE (turn.contains ("cursorKeyOfDetent (keyboardJob, increment)"));
}

// The four folder tabs go through one route (they were four copies, one of
// which forgot to stop the editing), and that route asks FROM… first.
TEST (KeyboardEncoders, TheFourTabsShareOneRouteThatAsksFromFirst)
{
  auto const ctor = body ("A3MotionUIComponent::A3MotionUIComponent "
                          "(unsigned int const numChannels)");
  ASSERT_TRUE (ctor.isNotEmpty ());
  for (auto const *list : { "Sessions", "Clips", "Shapes", "Actions" })
    EXPECT_TRUE (ctor.contains (juce::String ("tapBrowserList (BrowserList::")
                                + list + ")"))
        << list;

  auto const tap = body ("A3MotionUIComponent::tapBrowserList (BrowserList "
                         "list)");
  ASSERT_TRUE (tap.isNotEmpty ());
  auto const asked = tap.indexOf ("_fromArmed");
  ASSERT_GE (asked, 0);
  EXPECT_LT (asked, tap.indexOf ("takeFrom (list)"));
}

// FROM… + a tab takes the shown slot's state for that tab; on ACTIONS it
// remembers where it came from, so Save as points the button at the copy.
TEST (KeyboardEncoders, FromTakesTheShownSlotsStateForTheTabTapped)
{
  auto const from = body ("A3MotionUIComponent::takeFrom (BrowserList list)");
  ASSERT_TRUE (from.isNotEmpty ());
  EXPECT_TRUE (from.contains ("fileTextHoldsTheList ()"));
  EXPECT_TRUE (from.contains ("chooseBrowserList (list)"));
  EXPECT_TRUE (from.contains ("currentStateText ()"));
  EXPECT_TRUE (from.contains ("_editOrigin"));
}

// ACTION's EDIT has no route of its own any more: FILES, then FROM… ACTIONS.
TEST (KeyboardEncoders, EditIsFromActions)
{
  auto const ctor = body ("A3MotionUIComponent::A3MotionUIComponent "
                          "(unsigned int const numChannels)");
  auto const edit = ctor.indexOf ("_action->onEditPressed");
  ASSERT_GE (edit, 0);
  auto const end = ctor.indexOf (edit, "};");
  EXPECT_TRUE (ctor.substring (edit, end).contains (
      "takeFrom (BrowserList::Actions)"));
}
