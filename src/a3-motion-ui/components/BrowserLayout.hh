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

#include <a3-motion-ui/components/ClipSettingsLayout.hh>
// fingertipSize: the floor for anything hit in a hurry, and a library row is.
#include <a3-motion-ui/components/ControllerLayout.hh>

#include <array>
#include <vector>

namespace a3
{

/** The browser page: what there is to put in the clip on show.
 *
 *  It used to carry the device's eight clips down the left, laid out the way
 *  the pads page lays them out, and you chose one before choosing what to put
 *  in it. The four channel faces in the bar's header do that now, for every
 *  view of the settings area at once -- so a second set of destinations here
 *  meant two selections that could point at different slots, with the bar
 *  describing one while the list filled the other. The page is the library,
 *  and the library fills whatever the faces have chosen.
 */
struct BrowserLayout
{
  /** The library's own area, and the rows currently drawn in it. The rows are
   *  a window onto the library rather than all of it -- there are forty and
   *  more, and a row too short to hit is no use in a booth. */
  juce::Rectangle<int> listArea;
  std::vector<juce::Rectangle<int>> rows;
  /** How many rows the area has room for at this size. */
  int visibleRows = 0;
  int rowHeight = 0;

  /** Which of the three folders the list is showing. Words over the list
   *  rather than a mode you have to remember: clips are what a slot holds,
   *  actions are what ACT does to it, and a set is the arrangement of all
   *  eight at once. All three are chosen the same way, so all three are tabs.
   *
   *  A set used to be reached by touching the strip that named the loaded one
   *  -- a control that looked like a label, in a corner of the page that has
   *  gone with the destination fields. Which set is loaded is now the
   *  highlighted row of the set list, which is where you would look for it. */
  juce::Rectangle<int> clipsTab;
  /** The figures themselves, beside the clips that name them. Their own tab
   *  because they are their own kind of thing: a clip fills a slot with a
   *  figure and every value it is played with, a shape swaps only the figure
   *  and leaves the values where the hand put them. */
  juce::Rectangle<int> shapesTab;
  juce::Rectangle<int> actionsTab;
  juce::Rectangle<int> setsTab;

  /** The strip along the bottom of the list. The first key changes *what is
   *  listed*, the other three act on the row that is chosen in it -- which is
   *  why it stands first, at the end the reading starts from: narrow the
   *  list, then do something to a row of it. */
  juce::Rectangle<int> filterButton;
  juce::Rectangle<int> renameButton;
  juce::Rectangle<int> saveButton;
  /** Beside Save, not instead of it: one writes what is on show back where it
   *  came from, the other writes it somewhere new, and which of the two you
   *  meant is not a thing to work out from a modifier. */
  juce::Rectangle<int> saveAsButton;
  juce::Rectangle<int> deleteButton;
};

/** Lays the browser out inside the bar's content area. Reads no theme, so it
 *  can be checked at sizes nobody has dialled in yet. */
BrowserLayout layOutBrowser (juce::Rectangle<int> bounds, int buttonHeight,
                             float bodySize);

}
