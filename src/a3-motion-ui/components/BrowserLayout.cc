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
  // bar's, and a fortieth of it between keys took the characters a script
  // line needed (2026-09-27). The tiles keep the same pad inside.
  auto const gap = juce::jmax (2, bounds.getHeight () / 80);
  auto const pad = gap;
  auto const keyH
      = juce::jmin (bounds.getHeight (), juce::jmax (fingertipSize, buttonHeight));

  // Never less than a row of three keys, so the foot of the list fits.
  constexpr int keysInARow = 3;
  auto const listMinW
      = keysInARow * fingertipSize + (keysInARow - 1) * gap + 2 * pad;

  auto area = bounds;

  // The script's tile first, as wide as it asks -- a line of a shipped action
  // fits without scrolling sideways -- and the list's tile the rest.
  if (detailWidth > 0)
    {
      auto const width = juce::jlimit (
          0, juce::jmax (0, area.getWidth () - listMinW - gap),
          detailWidth + 2 * pad);
      out.detailTile = area.removeFromRight (width);
      out.detailArea = out.detailTile.reduced (pad);
      area.removeFromRight (gap);
    }
  out.listTile = area;

  auto inside = area.reduced (pad);
  auto const row = [&inside, keyH] (int fromTop) {
    return fromTop > 0 ? inside.removeFromTop (juce::jmin (keyH, inside.getHeight ()))
                       : inside.removeFromBottom (juce::jmin (keyH, inside.getHeight ()));
  };
  // A row of `n` equal keys, left to right.
  auto const split = [gap] (juce::Rectangle<int> r, int n,
                            std::vector<juce::Rectangle<int> *> const &into) {
    auto const w = (r.getWidth () - (n - 1) * gap) / n;
    for (size_t i = 0; i < into.size (); ++i)
      {
        *into[i] = i + 1 < into.size () ? r.removeFromLeft (w) : r.withWidth (w);
        r.removeFromLeft (gap);
      }
  };

  // The four folders two by two, in the order the work is done in: a set
  // holds clips, a clip holds a shape, and an action is what you reach for
  // once all three are standing.
  split (row (1), 2, { &out.setsTab, &out.clipsTab });
  inside.removeFromTop (gap);
  split (row (1), 2, { &out.shapesTab, &out.actionsTab });
  inside.removeFromTop (gap);

  // FROM between the folders and the filter, the filter over the list it
  // narrows (maintainer, 2026-09-27).
  out.fromClipButton = row (1);
  inside.removeFromTop (gap);
  out.filterButton = row (1);
  inside.removeFromTop (gap * 2);

  // The rest of the keys at the foot: what the text is kept by at the very
  // bottom, what the row is renamed or deleted by over it. Load only on SETS.
  split (row (0), keysInARow,
         { &out.cancelButton, &out.saveButton, &out.saveAsButton });
  inside.removeFromBottom (gap);
  // Load on every tab (2026-09-27): a tap only chooses. So the keys are the
  // same on every tab and `list` does not change them any more.
  juce::ignoreUnused (list);
  split (row (0), keysInARow,
         { &out.renameButton, &out.deleteButton, &out.loadButton });
  inside.removeFromBottom (gap * 2);

  out.listArea = inside;

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
