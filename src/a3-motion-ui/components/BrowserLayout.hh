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
#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/io/PadFunctions.hh>

#include <array>
#include <vector>

namespace a3
{

/** How many of the device's clips there are: every channel's every slot. */
constexpr int numBrowserFields = numChannelColumns * numPadSlots;

/** The browser page: where a clip is put, and what there is to put in it.
 *
 *  The eight fields down the left are the device's own clips, laid out the way
 *  the pads page lays them out -- channels across, slots down -- so the two
 *  pages describe the same eight things in the same arrangement. The list down
 *  the right is the library.
 *
 *  A field is chosen first and a clip second. That is the order the hand
 *  already knows: mid-set you know which deck you are filling before you know
 *  what you are filling it with, and choosing the clip first would leave it
 *  hanging with nowhere to go.
 */
struct BrowserLayout
{
  /** Which set is loaded, above the fields it filled. A session is the eight
   *  of them together, so it belongs over the eight rather than beside the
   *  library -- what you are choosing there is one clip, and here it is all of
   *  them at once. */
  juce::Rectangle<int> sessionField;

  /** The eight destinations, indexed [channel][slot] so this page and the
   *  pads page cannot come to disagree about which box is which clip. */
  std::array<std::array<juce::Rectangle<int>, numPadSlots>, numChannelColumns>
      fields;

  /** The library's own area, and the rows currently drawn in it. The rows are
   *  a window onto the library rather than all of it -- there are forty and
   *  more, and a row too short to hit is no use in a booth. */
  juce::Rectangle<int> listArea;
  std::vector<juce::Rectangle<int>> rows;
  /** How many rows the area has room for at this size. */
  int visibleRows = 0;
  int rowHeight = 0;

  /** What the list beside them is showing. Two words over the list rather
   *  than a mode you have to remember: clips are what a slot holds, actions
   *  are what ACT does to it, and both are chosen the same way. */
  juce::Rectangle<int> clipsTab;
  juce::Rectangle<int> actionsTab;

  /** The strip along the bottom of the list: what can be done to what is
   *  selected. */
  juce::Rectangle<int> renameButton;
  juce::Rectangle<int> saveSessionButton;
  juce::Rectangle<int> loadSessionButton;
};

/** Lays the browser out inside the bar's content area. Reads no theme, so it
 *  can be checked at sizes nobody has dialled in yet. */
BrowserLayout layOutBrowser (juce::Rectangle<int> bounds, int buttonHeight,
                             float bodySize);

}
