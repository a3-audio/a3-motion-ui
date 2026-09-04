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

#include "BrowserLayout.hh"

namespace a3
{

BrowserLayout
layOutBrowser (juce::Rectangle<int> bounds, int buttonHeight, float bodySize)
{
  BrowserLayout out;

  if (bounds.isEmpty ())
    return out;

  auto const gap = juce::jmax (2, bounds.getHeight () / 40);

  // Just under half to the destinations. The list needs the wider half: a
  // pattern's name is a word and a field's is a word plus a channel, but the
  // list is the thing being read rather than glanced at.
  auto area = bounds;
  auto fieldArea = area.removeFromLeft (area.getWidth () * 9 / 20);
  area.removeFromLeft (gap * 2);
  out.listArea = area;

  // Channels across, slots down -- the pads page's arrangement, because these
  // are the same eight clips.
  {
    auto const colGap = juce::jmax (2, fieldArea.getWidth () / 60);
    auto const colW
        = (fieldArea.getWidth () - (numChannelColumns - 1) * colGap)
          / numChannelColumns;
    auto const rowH
        = (fieldArea.getHeight () - (numPadSlots - 1) * gap) / numPadSlots;

    for (index_t channel = 0; channel < numChannelColumns; ++channel)
      {
        auto column = fieldArea.removeFromLeft (colW);
        if (channel + 1 < numChannelColumns)
          fieldArea.removeFromLeft (colGap);

        for (index_t slot = 0; slot < numPadSlots; ++slot)
          {
            out.fields[channel][slot] = column.removeFromTop (rowH);
            if (slot + 1 < numPadSlots)
              column.removeFromTop (gap);
          }
      }
  }

  // The strip along the bottom first, so the list is whatever is left rather
  // than the list deciding how much room the buttons get.
  {
    auto strip = out.listArea.removeFromBottom (
        juce::jmin (out.listArea.getHeight () / 3, buttonHeight));
    out.listArea.removeFromBottom (gap);

    auto const buttonW = (strip.getWidth () - 2 * gap) / 3;
    out.renameButton = strip.removeFromLeft (buttonW);
    strip.removeFromLeft (gap);
    out.saveSessionButton = strip.removeFromLeft (buttonW);
    strip.removeFromLeft (gap);
    out.loadSessionButton = strip;
  }

  // A row is hit with a finger, so it is a fingertip tall whatever the font
  // says -- and the list shows as many as fit rather than squeezing all of
  // them in, which at forty patterns would be four pixels each.
  out.rowHeight = juce::jmax (fingertipSize,
                              static_cast<int> (bodySize * 1.8f));
  out.visibleRows
      = juce::jmax (1, out.listArea.getHeight () / out.rowHeight);

  auto rows = out.listArea;
  for (int i = 0; i < out.visibleRows; ++i)
    out.rows.push_back (rows.removeFromTop (out.rowHeight));

  return out;
}

}
