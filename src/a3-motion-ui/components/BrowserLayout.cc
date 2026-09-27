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
layOutBrowser (juce::Rectangle<int> bounds, int buttonHeight, float bodySize,
               BrowserList list, int detailWidth)
{
  BrowserLayout out;

  if (bounds.isEmpty ())
    return out;

  // Half the bar's usual gap: over the sphere the height is three times the
  // bar's, and a fortieth of it between four keys took the characters a
  // script line needed (2026-09-27).
  auto const gap = juce::jmax (2, bounds.getHeight () / 80);
  auto const keyH
      = juce::jmin (bounds.getHeight (), juce::jmax (fingertipSize, buttonHeight));

  // The list's own keys: Load only on SETS, where a set is loaded on purpose;
  // a clip or a shape is put on a slot by a tap. Save and Save as are the
  // script's beside it, on every tab since 2026-09-27.
  auto const numKeys = list == BrowserList::Sessions ? 4 : 3;
  // Measured for the most keys any tab has, so the columns stay put when
  // the tab changes.
  constexpr int mostKeys = 4;
  auto const listMinW = mostKeys * fingertipSize + (mostKeys - 1) * gap;

  auto area = bounds;

  // The script column first, as wide as it asks -- a line of a shipped
  // action fits without scrolling sideways -- and the list keeps the rest,
  // never less than its keys need.
  if (detailWidth > 0)
    {
      auto const width = juce::jlimit (
          0, juce::jmax (0, area.getWidth () - listMinW - gap), detailWidth);
      out.detailArea = area.removeFromRight (width);
      area.removeFromRight (gap);
    }

  // The keys at the top, where back and close stood: one row with the
  // script's keys beside them, the same height (ScriptPanel::keyHeight).
  {
    auto keys = area.removeFromTop (keyH);
    area.removeFromTop (gap);

    auto const keyW = (keys.getWidth () - (numKeys - 1) * gap) / numKeys;
    if (list == BrowserList::Sessions)
      {
        out.loadButton = keys.removeFromLeft (keyW);
        keys.removeFromLeft (gap);
      }
    out.filterButton = keys.removeFromLeft (keyW);
    keys.removeFromLeft (gap);
    out.renameButton = keys.removeFromLeft (keyW);
    keys.removeFromLeft (gap);
    out.deleteButton = keys;
  }

  // The four folders two by two under them, in the order the work is done
  // in: a set holds clips, a clip holds a shape, and an action is what you
  // reach for once all three are standing. One size, so none of them reads
  // as the list's real subject.
  {
    auto const tabW = (area.getWidth () - gap) / 2;
    auto const tabRow = [&] (juce::Rectangle<int> &left,
                             juce::Rectangle<int> &right) {
      auto row = area.removeFromTop (juce::jmin (keyH, area.getHeight ()));
      left = row.removeFromLeft (tabW);
      right = row.withTrimmedLeft (gap).withWidth (tabW);
    };
    tabRow (out.setsTab, out.clipsTab);
    area.removeFromTop (gap);
    tabRow (out.shapesTab, out.actionsTab);
    area.removeFromTop (gap * 2);
  }

  out.listArea = area;

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
