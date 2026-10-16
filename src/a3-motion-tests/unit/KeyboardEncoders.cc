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

// While KEYS is up: E1 is the cursor, E5 scrolls the lines, E6 the columns
// (decided 2026-10-16).
TEST (KeyboardEncoders, EOneIsTheCursorEFiveAndSixScroll)
{
  EXPECT_EQ (typing (0, 0), Job::Cursor);
  EXPECT_EQ (typing (0, 1), Job::ScrollLines);
  EXPECT_EQ (typing (1, 1), Job::ScrollColumns);
}

// The other five do nothing -- they used to turn the hidden page's fields.
TEST (KeyboardEncoders, TheOtherFiveDoNothing)
{
  EXPECT_EQ (typing (1, 0), Job::Nothing);
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

// Clockwise is positive: down and right. One line, four columns a detent.
TEST (KeyboardEncoders, ADetentIsOneLineOrFourColumns)
{
  EXPECT_EQ (keyboardScrollStep (Job::ScrollLines, 1), 1);
  EXPECT_EQ (keyboardScrollStep (Job::ScrollLines, -3), -3);
  EXPECT_EQ (keyboardScrollStep (Job::ScrollColumns, 1), 4);
  EXPECT_EQ (keyboardScrollStep (Job::ScrollColumns, -2), -8);
  EXPECT_EQ (keyboardScrollStep (Job::Cursor, 1), 0);
  EXPECT_EQ (keyboardScrollStep (Job::Nothing, 5), 0);
}

TEST (KeyboardEncoders, TheCursorFollowsTheTurn)
{
  EXPECT_EQ (cursorKeyOfDetent (1), juce::KeyPress (juce::KeyPress::rightKey));
  EXPECT_EQ (cursorKeyOfDetent (-2), juce::KeyPress (juce::KeyPress::leftKey));
}

TEST (KeyboardEncoders, EachJobWithASignHasItsGlyph)
{
  EXPECT_EQ (keyboardEncoderGlyph (Job::Cursor), KnobGlyph::Cursor);
  EXPECT_EQ (keyboardEncoderGlyph (Job::ScrollLines), KnobGlyph::ScrollVertical);
  EXPECT_EQ (keyboardEncoderGlyph (Job::ScrollColumns),
             KnobGlyph::ScrollHorizontal);
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
