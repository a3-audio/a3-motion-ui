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

  // The tempo takes the left half of the row and the readout the right half
  // of what is left. Both overlap the indicator's rectangle and always have
  // -- one is aligned left and the other right, so the text keeps clear of it
  // even where the bounds do not.
  //
  // This is the whole layout again. Between 2026-09-10 and 2026-09-12 it was
  // one branch of a negotiation over how much width nine meters could take
  // from the two labels and the indicator; the meters went, and what is left
  // is what the bar looked like before they arrived.
  auto rest = row;
  out.bpm = rest.removeFromLeft (rest.getWidth () / 2)
                .withTrimmedLeft (padding);
  out.readout = rest.withTrimmedLeft (rest.getWidth () / 2)
                    .withTrimmedRight (padding);

  return out;
}

}
