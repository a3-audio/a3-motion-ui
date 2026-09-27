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

/** Which of the three folders the browser is listing.
 *
 *  It lived inside A3MotionUIComponent, which is what *decides* the list; this
 *  is what shows it, and a page that draws three tabs cannot be handed a bool
 *  saying which of two it is on. */
/** How far the library list is narrowed: everything, only what the performer
 *  made, only what the instrument shipped with. Beside BrowserList because it
 *  is the same kind of thing -- which list, and how much of it. */
enum class ClipFilter
{
  All,
  User,
  System
};

enum class BrowserList
{
  Clips,
  /** The figures the clips name. Choosing one swaps the slot's figure and
   *  leaves its values alone -- the same thing the picture on the CLIP page
   *  does, which is the other place a shape is chosen. */
  Shapes,
  /** Action clips -- what ACT does to a slot. Structurally a clip with no
   *  shape: a set of settings, kept in actions/ rather than clips/ because
   *  what it is for is different even though what it holds is the same. */
  Actions,
  /** The arrangement of all eight clips at once. */
  Sessions,
};

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
  /** The two grey tiles FILES stands on: the list's on the left, the
   *  script's on the right (2026-09-27). */
  juce::Rectangle<int> listTile;
  juce::Rectangle<int> detailTile;

  juce::Rectangle<int> listArea;
  std::vector<juce::Rectangle<int>> rows;
  /** Beside the list, what the chosen row holds: its text, on every tab
   *  since 2026-09-27 (ScriptPanel). */
  juce::Rectangle<int> detailArea;
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
  /** Load, and only the sets tab lights it. A tap on a row used to load the
   *  set outright -- all eight slots replaced, and whatever was running
   *  restarted -- which made the one gesture that *reaches* a set also the one
   *  that overwrites your arrangement. Its own key, pressed on purpose. */
  juce::Rectangle<int> loadButton;
  juce::Rectangle<int> filterButton;
  juce::Rectangle<int> renameButton;
  /** The script panel's four keys, in the list's tile since 2026-09-27:
   *  FROM under the folders, Cancel, Save and Save as at the foot. What they
   *  do is the panel's (ScriptPanel::pressSave and the rest). */
  juce::Rectangle<int> fromClipButton;
  juce::Rectangle<int> cancelButton;
  juce::Rectangle<int> saveButton;
  juce::Rectangle<int> saveAsButton;
  juce::Rectangle<int> deleteButton;
};

/** Lays FILES out over the sphere on two tiles: the script column on the
 *  right, as wide as `detailWidth` asks (none for 0); on the left, top to
 *  bottom, the four folders two by two, FROM, the filter, the list, and the
 *  other keys at the foot. Reads no theme, so it can be
 *  checked at sizes nobody has dialled in yet. */
BrowserLayout layOutBrowser (juce::Rectangle<int> bounds, int buttonHeight,
                             float bodySize, BrowserList list,
                             int detailWidth);

}
