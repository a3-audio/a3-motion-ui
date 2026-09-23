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

#pragma once

#include <JuceHeader.h>

#include <functional>

namespace a3

{

/** The script editor: JUCE's own, with the three things a touchscreen in a
 *  booth needs on top of it.
 *
 *  A `CodeEditorComponent` rather than text we draw ourselves, because it
 *  already has the line numbers, the syntax colours, the caret, the
 *  selection, the undo and the scrolling that a hand-rolled buffer was
 *  slowly growing. What it does not have is a finger:
 *
 *  - **Reading and writing are two states.** The editor sits read-only until
 *    it is touched, so the script can be read without a keyboard coming up
 *    over the page.
 *  - **A drag reads rather than selects.** While it is read-only, dragging
 *    scrolls the way a page does under a thumb; once it is being typed into,
 *    a drag selects as it should.
 *  - **Escape leaves.** The page takes the keyboard away with it.
 */
class ScriptEditor : public juce::CodeEditorComponent
{
public:
  ScriptEditor (juce::CodeDocument &document, juce::CodeTokeniser *tokeniser);

  /** A touch on a script that is only being read. */
  std::function<void ()> onStartEditing;
  /** Escape, while it is being typed into. */
  std::function<void ()> onEscape;

  void mouseDown (juce::MouseEvent const &event) override;
  void mouseDrag (juce::MouseEvent const &event) override;
  void mouseUp (juce::MouseEvent const &event) override;
  bool keyPressed (juce::KeyPress const &key) override;

private:
  int _dragLine = 0;
  int _dragColumn = 0;
  /** Which column the view starts at, as this class has scrolled it. */
  int _column = 0;
};

}
