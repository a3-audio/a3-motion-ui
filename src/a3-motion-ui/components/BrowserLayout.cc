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

  // The whole area is the library. The eight destinations that used to take
  // the left of it are the header's channel faces now -- see BrowserLayout's
  // own note -- and what is being read here is a list of names, which is the
  // one thing on this page that gets better with width.
  auto area = bounds;

  // Three words over the list: one question -- which folder -- asked of three
  // folders, so all three are the same size. Any one of them drawn larger
  // would read as the list's real subject with the others as afterthoughts.
  auto tabRow = area.removeFromTop (
      juce::jmin (area.getHeight () / 4, buttonHeight));
  auto const tabW = (tabRow.getWidth () - gap * 2) / 3;
  out.clipsTab = tabRow.removeFromLeft (tabW);
  tabRow.removeFromLeft (gap);
  out.actionsTab = tabRow.removeFromLeft (tabW);
  tabRow.removeFromLeft (gap);
  out.setsTab = tabRow.removeFromLeft (tabW);

  area.removeFromTop (gap * 2);
  out.listArea = area;

  // The strip along the bottom first, so the list is whatever is left rather
  // than the list deciding how much room the buttons get.
  {
    auto strip = out.listArea.removeFromBottom (
        juce::jmin (out.listArea.getHeight () / 3, buttonHeight));
    out.listArea.removeFromBottom (gap);

    auto const buttonW = (strip.getWidth () - 2 * gap) / 3;
    out.renameButton = strip.removeFromLeft (buttonW);
    strip.removeFromLeft (gap);
    out.saveButton = strip.removeFromLeft (buttonW);
    strip.removeFromLeft (gap);
    out.deleteButton = strip;
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
