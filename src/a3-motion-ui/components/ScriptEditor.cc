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

namespace
{
/** JUCE keeps its two scroll bars private, but they are the editor's
 *  children, and they hold the view's position whoever moved it. */
juce::ScrollBar const *
scrollBarOf (juce::Component const &editor, bool vertical)
{
  for (auto *child : editor.getChildren ())
    if (auto const *bar = dynamic_cast<juce::ScrollBar const *> (child))
      if (bar->isVertical () == vertical)
        return bar;
  return nullptr;
}

/** How far along its range a scroll bar stands, 0 to 1; 0 with nowhere to go. */
float
fractionOf (juce::ScrollBar const *bar)
{
  if (bar == nullptr)
    return 0.f;
  auto const room = bar->getMaximumRangeLimit () - bar->getMinimumRangeLimit ()
                    - bar->getCurrentRangeSize ();
  if (room <= 0.0)
    return 0.f;
  return static_cast<float> (juce::jlimit (
      0.0, 1.0,
      (bar->getCurrentRangeStart () - bar->getMinimumRangeLimit ()) / room));
}

/** `from` moved by `delta`, never below 0 and -- moving forward -- never past
 *  `last`. A view a drag has carried beyond `last` is not pulled back by a
 *  turn the other way; it simply moves. */
int
steppedWithin (int from, int delta, int last)
{
  auto target = from + delta;
  if (delta > 0)
    target = juce::jmin (target, juce::jmax (from, last));
  return juce::jmax (0, target);
}
}

void
ScriptEditor::scrollLinesBy (int lineCount)
{
  auto const last = juce::jmax (0, getDocument ().getNumLines ()
                                       - getNumLinesOnScreen ());
  scrollToLine (steppedWithin (getFirstLineOnScreen (), lineCount, last));
}

void
ScriptEditor::scrollColumnsBy (int columnCount)
{
  auto const last = juce::jmax (0, getDocument ().getMaximumLineLength ()
                                       - getNumColumnsOnScreen ());
  scrollToColumn (steppedWithin (firstColumnOnScreen (), columnCount, last));
}

int
ScriptEditor::firstColumnOnScreen () const
{
  auto const *bar = scrollBarOf (*this, false);
  return bar != nullptr ? juce::roundToInt (bar->getCurrentRangeStart ()) : 0;
}

juce::Point<float>
ScriptEditor::scrollFractions () const
{
  return { fractionOf (scrollBarOf (*this, false)),
           fractionOf (scrollBarOf (*this, true)) };
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
      _dragColumn = firstColumnOnScreen ();
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

  // Counted from the column the view stood at when the finger came down --
  // the scroll bar's, so a turn of E6 in between is not undone.
  scrollToColumn (juce::jmax (
      0, _dragColumn
             - juce::roundToInt (event.getDistanceFromDragStartX ()
                                 / charWidth)));
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
