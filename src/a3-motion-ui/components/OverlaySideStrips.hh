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
