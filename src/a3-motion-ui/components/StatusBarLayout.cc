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

#include "StatusBarLayout.hh"

namespace a3
{

StatusBarLayout
statusBarLayout (juce::Rectangle<int> row, int barWidth, int padding)
{
  StatusBarLayout out{};

  if (row.isEmpty () || barWidth <= 0)
    return out;

  auto const rowHeight = row.getHeight ();

  // What the beat display takes. Both bounds are the ones resized() has
  // always applied.
  auto const tickWidth = juce::jmin (
      juce::roundToInt (static_cast<float> (row.getWidth ())
                        * statusTickWidthOfRow),
      juce::roundToInt (static_cast<float> (barWidth)
                        * statusTickWidthOfBar));

  // Centred on the whole bar rather than on what the labels leave over: it is
  // the one thing here that is looked at rather than read.
  out.tick
      = juce::Rectangle<int> (tickWidth,
                              juce::roundToInt (
                                  static_cast<float> (rowHeight)
                                  * statusTickHeightOfRow))
            .withCentre ({ barWidth / 2, row.getCentreY () });

  // The keys at the ends, one size: wide enough for a word, as tall as the
  // row. CLOCK leads it, left of the tempo it decides; CLEAN, the on-screen
  // keyboard and MENU close it, MENU at the very edge where a thumb finds it.
  auto rest = row;
  auto const keyW = juce::jmin (rowHeight * statusKeyWidthOfHeight,
                                row.getWidth () / 8);
  out.menuKey = rest.removeFromRight (keyW);
  out.keyboardKey = rest.removeFromRight (keyW);
  out.cleanKey = rest.removeFromRight (keyW);
  // Before CLEAN: over to StemDeck, and the arrow listing every workspace.
  out.workspacesKey = rest.removeFromRight (keyW / 2);
  out.deckKey = rest.removeFromRight (keyW);
  out.clockKey = rest.removeFromLeft (keyW);

  // Still centred, but never under a key: on a narrow bar the display gives
  // way rather than the keys.
  auto const tickRoom = 2 * (out.deckKey.getX () - barWidth / 2);
  if (out.tick.getWidth () > tickRoom)
    out.tick = out.tick.withSizeKeepingCentre (juce::jmax (0, tickRoom),
                                               out.tick.getHeight ());

  // What is left of the display's left edge carries the two readings: the
  // tempo, then what was last done, standing against the display so the
  // right end is keys only. The readout gets the larger share, being the
  // longer text.
  auto left = rest.withRight (juce::jmin (rest.getRight (), out.tick.getX ()))
                  .withTrimmedLeft (padding)
                  .withTrimmedRight (padding);
  out.bpm = left.removeFromLeft (left.getWidth () * 2 / 5);
  out.readout = left;

  return out;
}

}
