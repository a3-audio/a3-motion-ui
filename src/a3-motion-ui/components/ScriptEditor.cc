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
      // Where the drag will count from, before anything moves.
      _dragLine = getFirstLineOnScreen ();

      if (onStartEditing)
        onStartEditing ();
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
  // the text up and brings later lines into view. Counted from where the
  // finger came down rather than from the last event, so a slow drag does not
  // lose lines to rounding.
  auto const lineHeight = juce::jmax (1, getLineHeight ());
  scrollToLine (_dragLine - event.getDistanceFromDragStartY () / lineHeight);
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
