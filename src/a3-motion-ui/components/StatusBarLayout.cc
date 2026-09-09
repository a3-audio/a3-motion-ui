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

  // What the beat display would take if nothing else asked for room. Both
  // bounds are the ones resized() has always applied.
  auto const wantedTick = juce::jmin (
      juce::roundToInt (static_cast<float> (row.getWidth ())
                        * statusTickWidthOfRow),
      juce::roundToInt (static_cast<float> (barWidth)
                        * statusTickWidthOfBar));

  auto const cell = juce::jmax (
      1, juce::roundToInt (static_cast<float> (rowHeight)
                           * statusMeterCellOfRowHeight));
  auto const blockGap = juce::jmax (
      1, juce::roundToInt (static_cast<float> (rowHeight)
                           * statusMeterGapOfRowHeight));
  auto const minLabel = juce::roundToInt (
      static_cast<float> (rowHeight) * statusLabelMinWidthOfRowHeight);

  auto const inputWidth = cell * numChannelsInitial;
  auto const outputWidth = cell * numOutputMeters;

  // The indicator is centred on the whole bar, so the two sides of it are not
  // the same width: the row gives up its right end to the MIX and keyboard
  // icons and the output block is the one that has to live in what is left.
  // Solved as a half-width rather than as two edges for exactly that reason —
  // a half-width that satisfies the tighter side keeps the indicator centred,
  // where trimming one edge would slide it off centre.
  auto const centre = barWidth / 2;
  auto const leftAllows
      = (centre - row.getX ()) - (inputWidth + blockGap + minLabel);
  auto const rightAllows
      = (row.getRight () - centre) - (outputWidth + blockGap + minLabel);

  auto const halfTick
      = juce::jmin (wantedTick / 2, leftAllows, rightAllows);

  // The indicator gives width up, down to a share of what it would have had;
  // past that the meters go instead. See statusMeterMinTickShare.
  auto const fits
      = halfTick > 0
        && halfTick * 2 >= juce::roundToInt (static_cast<float> (wantedTick)
                                             * statusMeterMinTickShare);

  auto const tickWidth = fits ? halfTick * 2 : wantedTick;
  out.tick
      = juce::Rectangle<int> (tickWidth,
                              juce::roundToInt (
                                  static_cast<float> (rowHeight)
                                  * statusTickHeightOfRow))
            .withCentre ({ centre, row.getCentreY () });

  if (!fits)
    {
      // What the bar looked like before the meters existed: the tempo takes
      // the left half of the row and the readout the right half of what is
      // left. Both overlap the indicator's rectangle and always have — one is
      // aligned left and the other right, so the text keeps clear of it even
      // where the bounds do not.
      auto rest = row;
      out.bpm = rest.removeFromLeft (rest.getWidth () / 2)
                    .withTrimmedLeft (padding);
      out.readout = rest.withTrimmedLeft (rest.getWidth () / 2)
                        .withTrimmedRight (padding);
      return out;
    }

  auto const barGap = juce::jmax (
      1, juce::roundToInt (static_cast<float> (cell) * statusMeterGapOfCell));

  out.inputBlock = juce::Rectangle<int> (
      out.tick.getX () - blockGap - inputWidth, row.getY (), inputWidth,
      rowHeight);
  out.outputBlock = juce::Rectangle<int> (out.tick.getRight () + blockGap,
                                          row.getY (), outputWidth, rowHeight);

  // VuMeter's stepping, not a second copy of it. Both blocks show signals the
  // mixer already shows -- the four channels its strips meter and the five
  // outputs its master column meters -- and two steppings of one signal would
  // put it on two rasters at once.
  stepMeterBarsAcross (out.inputBlock, cell, barGap, out.inputMeters);
  stepMeterBarsAcross (out.outputBlock, cell, barGap, out.outputMeters);

  // The labels stop where the meters begin. They used to run under the
  // indicator, which was harmless while the space between them was empty —
  // one is aligned left and the other right. With nine bars standing in that
  // space it is no longer empty, and a label bound that reaches across a
  // meter is a text that will one day be drawn over one.
  out.bpm = row.withRight (out.inputBlock.getX () - blockGap)
                .withTrimmedLeft (padding);
  out.readout = row.withLeft (out.outputBlock.getRight () + blockGap)
                    .withTrimmedRight (padding);

  return out;
}

}
