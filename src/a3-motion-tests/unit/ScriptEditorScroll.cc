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

#include <a3-motion-ui/components/ScriptEditor.hh>

using namespace a3;

// The view moves under E5 and E6 while KEYS is up (2026-10-16): scrolling,
// the way the scroll bars do it -- the caret stays where it was.

namespace
{
constexpr int numLines = 100;
constexpr int lineLength = 120;

struct Editor
{
  Editor ()
  {
    juce::String text;
    for (int line = 0; line < numLines; ++line)
      text << juce::String::repeatedString ("x", lineLength) << "\n";
    document.replaceAllContent (text.trimEnd ());
    editor.setBounds (0, 0, 300, 200);
    // From the top, as a script is opened.
    editor.moveCaretTo (juce::CodeDocument::Position (document, 0), false);
    editor.scrollToLine (0);
    editor.scrollToColumn (0);
  }

  juce::MouseEvent eventAt (juce::Point<float> at, juce::Point<float> down)
  {
    auto const source = juce::Desktop::getInstance ().getMainMouseSource ();
    return juce::MouseEvent (source, at, {},
                             juce::MouseInputSource::defaultPressure, 0.f,
                             0.f, 0.f, 0.f, &editor, &editor,
                             juce::Time::getCurrentTime (), down,
                             juce::Time::getCurrentTime (), 1, true);
  }

  juce::CodeDocument document;
  ScriptEditor editor{ document, nullptr };
};
}

TEST (ScriptEditorScroll, LinesMoveTheViewAndLeaveTheCaret)
{
  Editor e;
  auto const caret = e.editor.getCaretPos ().getPosition ();
  e.editor.scrollLinesBy (3);
  EXPECT_EQ (e.editor.getFirstLineOnScreen (), 3);
  EXPECT_EQ (e.editor.getCaretPos ().getPosition (), caret);

  e.editor.scrollLinesBy (-1);
  EXPECT_EQ (e.editor.getFirstLineOnScreen (), 2);
}

// The view stops with the last line at the bottom, not with one line left
// on an empty screen, and never above the first.
TEST (ScriptEditorScroll, LinesStopAtTheEnds)
{
  Editor e;
  e.editor.scrollLinesBy (-5);
  EXPECT_EQ (e.editor.getFirstLineOnScreen (), 0);

  e.editor.scrollLinesBy (10000);
  EXPECT_EQ (e.editor.getFirstLineOnScreen (),
             numLines - e.editor.getNumLinesOnScreen ());
}

TEST (ScriptEditorScroll, ColumnsMoveTheViewAndStopAtBothEnds)
{
  Editor e;
  auto const caret = e.editor.getCaretPos ().getPosition ();
  e.editor.scrollColumnsBy (-4);
  EXPECT_EQ (e.editor.firstColumnOnScreen (), 0);

  e.editor.scrollColumnsBy (8);
  EXPECT_EQ (e.editor.firstColumnOnScreen (), 8);
  EXPECT_EQ (e.editor.getCaretPos ().getPosition (), caret);

  e.editor.scrollColumnsBy (10000);
  EXPECT_EQ (e.editor.firstColumnOnScreen (),
             e.document.getMaximumLineLength ()
                 - e.editor.getNumColumnsOnScreen ());
}

// What the KEYS knobs show: 0 at the top or the left, 1 at the end.
TEST (ScriptEditorScroll, TheFractionsRunFromStartToEnd)
{
  Editor e;
  EXPECT_FLOAT_EQ (e.editor.scrollFractions ().x, 0.f);
  EXPECT_FLOAT_EQ (e.editor.scrollFractions ().y, 0.f);

  e.editor.scrollLinesBy (10000);
  e.editor.scrollColumnsBy (10000);
  EXPECT_FLOAT_EQ (e.editor.scrollFractions ().x, 1.f);
  EXPECT_FLOAT_EQ (e.editor.scrollFractions ().y, 1.f);
}

// A script shorter than the view has nowhere to scroll: it reads as the top.
TEST (ScriptEditorScroll, AShortScriptIsAtTheTop)
{
  juce::CodeDocument document;
  document.replaceAllContent ("~a = 1;");
  ScriptEditor editor (document, nullptr);
  editor.setBounds (0, 0, 300, 200);
  editor.scrollLinesBy (3);
  EXPECT_EQ (editor.getFirstLineOnScreen (), 0);
  EXPECT_FLOAT_EQ (editor.scrollFractions ().y, 0.f);
}

// The finger and the encoder share one truth: a drag after the encoder has
// moved the view starts from where the encoder left it, not from where the
// last drag ended.
TEST (ScriptEditorScroll, ADragContinuesFromWhereTheEncoderLeftIt)
{
  Editor e;
  e.editor.scrollColumnsBy (8);
  e.editor.scrollLinesBy (5);

  juce::Point<float> const at{ 100.f, 100.f };
  e.editor.mouseDown (e.eventAt (at, at));
  e.editor.mouseDrag (e.eventAt (at, at));

  EXPECT_EQ (e.editor.firstColumnOnScreen (), 8);
  EXPECT_EQ (e.editor.getFirstLineOnScreen (), 5);
}
