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
#include <memory>

#include <a3-motion-ui/components/TouchControl.hh>

namespace a3
{

/** Whether the strips belong on screen at all, given what is open.
 *
 *  A predicate rather than a line inside A3MotionUIComponent::
 *  updateOverlayButtons, because it is the question the strips exist to
 *  answer and it was got wrong: the strips serve **the overlay in front**,
 *  and only if that one has a list to walk.
 *
 *  The mixer is in front whenever it is open. It is opened from the MIX key
 *  in the status bar, which is reachable whatever else is up, so it can stand
 *  over an open menu -- and toggleGlobalSettings() already treats it as the
 *  innermost room for exactly that reason. It is a grid, every control of
 *  which is touched directly, so it has no list.
 *
 *  What that cost while the menu's answer was used instead: the strips are a
 *  fifth of the window each, which is the mixer's master column on the right
 *  and its first channel on the left. Both were dead to the finger, and a
 *  drag over them armed the menu's highlighted row, changed its value and
 *  applied it on release -- the skin, the pattern folder or an OSC address --
 *  with no cue but a mixer control that did not move.
 *
 *  The colour picker sits in the same place one level down, and cost the
 *  same thing. openColourPicker() hides the skin editor but leaves
 *  `_skinEditorOpen` true -- it is still the page underneath, and closing the
 *  picker returns to it -- so a predicate that did not ask about the picker
 *  went on answering for the editor, and the strips came to the front over
 *  the picker sized to a panel nobody could see. A drag in the outer fifths
 *  scrolled that hidden list instead of moving hue.
 *
 *  So the flags are read as a stack rather than as one expression: the two
 *  that can stand in front are the mixer and the colour picker, in the order
 *  toggleGlobalSettings() closes them, and neither of them is a list -- the
 *  mixer is a grid touched control by control, the picker a field and a bar
 *  with navigation of its own. Only behind those two is the question worth
 *  asking, and there the answer is the menu or the skin editor.
 *
 *  Written this way rather than as `anyOpen && !picker && !mixer` because a
 *  chain of negations is a chain somebody adds a fourth link to and forgets;
 *  this one says which overlay is being answered for. */
constexpr bool
sideStripsHaveAList (bool globalSettingsOpen, bool skinEditorOpen,
                     bool colourPickerOpen, bool mixerOpen)
{
  if (mixerOpen || colourPickerOpen)
    return false;

  return globalSettingsOpen || skinEditorOpen;
}

/**
 * OverlaySideStrips
 *
 * The empty ground left and right of whatever page is open — the settings
 * menu, the skin editor, the colour picker — made into two drag zones.
 * Dragging in the **left** one walks the page's list; dragging in the
 * **right** one turns the value of whatever the list is on.
 *
 * Two reasons it is one component rather than one per page:
 *
 * - A page that grows a scrollbar should not have to grow a gesture too. The
 *   skin editor is a hundred rows long and the menu is six; the hand does the
 *   same thing on both, and where a list is long enough to need it is exactly
 *   where nobody wants to discover a different rule.
 * - **The hand does not cover what it is moving through.** Dragging over the
 *   rows hides the rows you are travelling past, which is the whole reason to
 *   put the gesture beside the page instead of on it.
 *
 * It knows nothing about pages. A3MotionUIComponent gives it the panel to sit
 * beside and points the callbacks at whichever page is open.
 */
class OverlaySideStrips : public juce::Component
{
public:
  OverlaySideStrips ();

  /** Dragged left of the panel: move through the page's list. Positive is
   *  further down the page, because the finger going down means going down. */
  std::function<void (int delta)> onBrowse;
  /** Dragged right of the panel: change the value the list is on. Positive
   *  is more, as everywhere else in this interface. */
  std::function<void (int delta)> onValue;
  /** The finger came off after such a drag — where a page has something to
   *  confirm, this is when. */
  std::function<void ()> onValueReleased;

  /** Put the strips either side of `panel`, in this component's own
   *  coordinates. A panel that fills the width leaves no strips, and the
   *  zones simply end up empty. */
  void setPanel (juce::Rectangle<int> panel);

private:
  std::unique_ptr<TouchControl> _browseZone;
  std::unique_ptr<TouchControl> _valueZone;
};

}
