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

// The editor's view, as the caret, a drag and the scroll bars move it.

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

// Moving the caret scrolls the editor to keep it in view (E1 and E2 on
// KEYS); the column the view starts at is the scroll bar's, so it knows.
TEST (ScriptEditorScroll, TheColumnFollowsTheCaret)
{
  Editor e;
  e.editor.moveCaretTo (juce::CodeDocument::Position (e.document, 0,
                                                      lineLength),
                        false);
  EXPECT_GT (e.editor.firstColumnOnScreen (), 0);
}

// The finger and the editor share one truth: a drag after the view has moved
// on its own -- the caret kept in view -- starts from where the view is, not
// from where the last drag ended.
TEST (ScriptEditorScroll, ADragContinuesFromWhereTheViewIs)
{
  Editor e;
  e.editor.scrollToColumn (8);
  e.editor.scrollToLine (5);

  juce::Point<float> const at{ 100.f, 100.f };
  e.editor.mouseDown (e.eventAt (at, at));
  e.editor.mouseDrag (e.eventAt (at, at));

  EXPECT_EQ (e.editor.firstColumnOnScreen (), 8);
  EXPECT_EQ (e.editor.getFirstLineOnScreen (), 5);
}
