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

#include <juce_gui_basics/juce_gui_basics.h>

namespace a3
{

/** How a fixed number of equal columns is arranged in an area that may be too
 *  narrow for all of them side by side.
 *
 *  **New here, and the mixer is the occasion.** This codebase has proportions
 *  (every rectangle a fraction of the one it is given), thresholds
 *  (fingertipSize, minimumChannelWidth) and grow-to-fit
 *  (controllerPreferredHeight). It has had no reflow at all: the two
 *  thresholds were used in exactly one place each, to declare how small the
 *  window may get.
 *
 *  Four channel strips side by side is the widest demand the device makes —
 *  wider than the pads page, which has the same four columns but no controls
 *  inside them. So the rule arrives with the mixer, and as a unit of its own
 *  so the pads page can take it later rather than growing its own.
 *
 *  It knows nothing about mixers, channels or pads: an area, a count, and the
 *  two thresholds a cell must clear. */
struct ColumnBreak
{
  int columns;
  int rows;
  /** False when even one column per row cannot clear both thresholds. The
   *  caller then says so on screen rather than laying out targets nobody can
   *  hit. */
  bool fits;
};

/** The arrangement for `count` cells in `area`.
 *
 *  Halves the columns until a cell is at least `minimumWidth` wide and
 *  `minimumHeight` tall, or until there is one column left. The threshold
 *  itself is wide enough — a rule that broke one pixel early would reflow the
 *  device it was written for. */
ColumnBreak breakColumns (juce::Rectangle<int> area, int count,
                          int minimumWidth, int minimumHeight);

/** Cell `index` of that arrangement, in reading order: left to right, then
 *  down, so channel 1 is where a hand looks for it whatever the arrangement.
 *
 *  Empty for an index outside the arrangement, and for an empty area. */
juce::Rectangle<int> cellIn (juce::Rectangle<int> area, ColumnBreak broken,
                            int index);

}
