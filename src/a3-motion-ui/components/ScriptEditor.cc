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

#include "ScriptEditor.hh"

namespace a3
{

ScriptEditor::ScriptEditor (juce::CodeDocument &document,
                            juce::CodeTokeniser *tokeniser)
    : juce::CodeEditorComponent (document, tokeniser)
{
  setLineNumbersShown (true);
  setReadOnly (true);
}

void
ScriptEditor::mouseDown (juce::MouseEvent const &event)
{
  if (isReadOnly ())
    {
      // Where a drag will count from, before anything moves. Nothing else
      // happens on the way down: a touch that turns out to be a drag has to
      // scroll, and one that turns out to be a tap starts the edit when the
      // finger comes up -- see mouseUp.
      _dragLine = getFirstLineOnScreen ();
      _dragColumn = _column;
      return;
    }

  juce::CodeEditorComponent::mouseDown (event);
}

void
ScriptEditor::mouseDrag (juce::MouseEvent const &event)
{
  if (!isReadOnly ())
    {
      juce::CodeEditorComponent::mouseDrag (event);
      return;
    }

  // The page follows the finger, the way it does on a phone: upwards carries
  // the text up and brings later lines into view, and sideways works the same
  // way -- a code editor does not wrap, so long lines have to be reachable.
  // Counted from where the finger came down rather than from the last event,
  // so a slow drag does not lose lines to rounding.
  auto const lineHeight = juce::jmax (1, getLineHeight ());
  auto const charWidth = juce::jmax (1.f, getCharWidth ());

  scrollToLine (_dragLine - event.getDistanceFromDragStartY () / lineHeight);

  // The editor keeps no public reckoning of which column it starts at, so
  // this one does: a drag counts from where the finger came down.
  _column = juce::jmax (0, _dragColumn
                               - juce::roundToInt (
                                   event.getDistanceFromDragStartX ()
                                   / charWidth));
  scrollToColumn (_column);
}

void
ScriptEditor::mouseUp (juce::MouseEvent const &event)
{
  if (!isReadOnly ())
    {
      juce::CodeEditorComponent::mouseUp (event);
      return;
    }

  // A tap, not a drag: reading becomes writing, and the caret lands where the
  // finger was -- which is why the press is handed to the base class now,
  // once it is allowed to place it.
  if (event.mouseWasDraggedSinceMouseDown ())
    return;

  if (onStartEditing)
    onStartEditing ();

  if (!isReadOnly ())
    {
      juce::CodeEditorComponent::mouseDown (event);
      juce::CodeEditorComponent::mouseUp (event);
    }
}

bool
ScriptEditor::keyPressed (juce::KeyPress const &key)
{
  if (key == juce::KeyPress::escapeKey && onEscape)
    {
      onEscape ();
      return true;
    }

  return juce::CodeEditorComponent::keyPressed (key);
}

}
