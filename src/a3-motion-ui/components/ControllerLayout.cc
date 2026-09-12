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

#include "ControllerLayout.hh"

namespace a3
{

namespace
{
/** The breathing room between two pads, and between the grid and the edges.
 *  Fixed rather than derived from the area, because the preferred height is
 *  worked out before there is an area to derive anything from. */
constexpr int padGap = 4;
constexpr int minPadding = 4;

int
headerRowHeight (float headerSize)
{
  return juce::jmax (18, static_cast<int> (headerSize * 1.4f));
}

/** Where a pad sits inside its clip's box, as a column and a row.
 *
 *  Read off the panel, not derived from the pad indices — those say which pad
 *  is which function, but only the hardware says where that function sits
 *  under a hand. The first arrangement here was taken from the index order
 *  and had action and stop the wrong way round; pressing the pad drawn as
 *  ACT reported STOP.
 *
 *  Play and stop on top, action and settings below. Keyed on the function so
 *  a pad is where it is because of what it does.
 */
juce::Point<int>
padCellInBox (index_t pad)
{
  switch (padFunctionByPadIndex[pad])
    {
    case PadFunction::PlayPause: return { 0, 0 };
    case PadFunction::Stop:      return { 1, 0 };
    case PadFunction::Action:    return { 0, 1 };
    case PadFunction::Settings:  return { 1, 1 };
    }

  return { 0, 0 };
}
}

int
controllerPreferredHeight (float headerSize, int)
{
  // Read straight back off layOutController's own arithmetic, bottom up:
  // two pad rows and the gap between them make a box, two boxes and a gap
  // make the grid, and above and below it sit the header, the modifier row
  // and the paddings. Anything else here would be a second guess at the
  // layout, and the two would drift.
  auto const slots = static_cast<int> (numPadSlots);
  auto const boxH = 2 * fingertipSize + padGap;
  auto const gridH = slots * boxH + (slots - 1) * padGap;

  return gridH + headerRowHeight (headerSize) + 2 * minPadding;
}

ControllerLayout
layOutController (juce::Rectangle<int> contentArea, float, int)
{
  ControllerLayout out;

  // What comes in is the clip part's *content* — the bar has already taken
  // its header row off (ClipSettingsLayout::clipContent). Working that height
  // out again here put it eleven pixels wrong and drew the top row of pads
  // under the tabs that switch to this page.
  auto area = contentArea.reduced (minPadding, minPadding);

  auto const gap = padGap;

  // Clamped at zero rather than trusted: a bar too small for the page is a
  // layout bug somewhere else, and it must arrive here as small rectangles,
  // not as ones whose right edge is left of their left.
  auto const boxW = juce::jmax (
      0, (area.getWidth () - (numChannelColumns - 1) * gap) / numChannelColumns);
  auto const slots = static_cast<int> (numPadSlots);
  auto const boxH
      = juce::jmax (0, (area.getHeight () - (slots - 1) * gap) / slots);

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t slot = 0; slot < numPadSlots; ++slot)
      out.clipBoxes[channel][slot] = juce::Rectangle<int> (
          area.getX () + static_cast<int> (channel) * (boxW + gap),
          area.getY () + static_cast<int> (slot) * (boxH + gap), boxW, boxH);

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
      {
        auto const box = out.clipBoxes[channel][slotForPadIndex[pad]];
        auto const cell = padCellInBox (pad);

        auto const padW = juce::jmax (0, (box.getWidth () - gap) / 2);
        auto const padH = juce::jmax (0, (box.getHeight () - gap) / 2);

        out.pads[channel][pad] = juce::Rectangle<int> (
            box.getX () + cell.x * (padW + gap),
            box.getY () + cell.y * (padH + gap), padW, padH);
      }

  return out;
}

}
