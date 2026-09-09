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

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

#include <a3-motion-engine/Config.hh>

#include <a3-motion-ui/components/VuMeter.hh>

namespace a3
{

/** Everything the status bar puts between its left edge and the two icons at
 *  its right end.
 *
 *  A pure function with a test of its own, the way ClipSettingsLayout and
 *  ControllerLayout are, so paint() draws into exactly the rectangles the
 *  test checks. This bar is the one that never goes away, which makes the
 *  rectangles worth pinning twice over: they decide where the meters are and
 *  they decide what a refresh costs.
 */
struct StatusBarLayout
{
  /** The tempo reading, at the left end. */
  juce::Rectangle<int> bpm;

  /** The four input meters, left of the beat display.
   *
   *  Empty on a bar too narrow to hold them -- see `statusMeterMinTickShare`
   *  for what "too narrow" means here. */
  std::array<juce::Rectangle<int>, numChannelsInitial> inputMeters;

  /** The beat display, centred on the whole bar rather than on what the rest
   *  leaves over: it is the one thing here that is looked at rather than
   *  read, and an off-centre one reads as a mistake. */
  juce::Rectangle<int> tick;

  /** The five output meters, right of the beat display. */
  std::array<juce::Rectangle<int>, numOutputMeters> outputMeters;

  /** What was last turned, at the right end. */
  juce::Rectangle<int> readout;

  /** The two rectangles a meter refresh is clipped to.
   *
   *  **This is the difference between these meters and the mixer's.** Those
   *  ran only while their page was on screen; this bar is always there, so a
   *  refresh that took the whole bar would redraw the beat display and two
   *  labels twenty-five times a second for the life of the device. Each block
   *  is exactly its own bars' bounding box and contains nothing else, which
   *  is what lets `repaint(block)` be both correct and cheap. */
  juce::Rectangle<int> inputBlock;
  juce::Rectangle<int> outputBlock;
};

/** One meter's cell -- its bar plus the air beside it -- as a share of the
 *  row's height.
 *
 *  A third, so nine of them plus their gaps come to about three rows' width:
 *  what these have to say is "something is arriving on this input", and that
 *  is a question of a bar being lit at all rather than of reading a number
 *  off it. The mixer's own meters, which are read, keep the full height of a
 *  strip for exactly the opposite reason. */
constexpr float statusMeterCellOfRowHeight = 1.f / 3.f;

/** The air between two bars of a block, as a share of one bar's cell.
 *
 *  A quarter, wider in proportion than the mixer's eighth: at this size an
 *  eighth of a cell rounds to a pixel, and four bars separated by one pixel
 *  read as one wide bar with lines scratched in it. */
constexpr float statusMeterGapOfCell = 1.f / 4.f;

/** The air between a block and the beat display, as a share of the row's
 *  height.
 *
 *  Half a row, which is several times the air inside a block: the two have to
 *  read as a block of meters standing *beside* the indicator rather than as
 *  its outermost ticks. */
constexpr float statusMeterGapOfRowHeight = 1.f / 2.f;

/** How much width a reading is guaranteed, as a multiple of the row's height.
 *
 *  Three rows is "BPM 120.0" at the shipped header size. The two labels are
 *  what the meters are taking their room from, and a reading squeezed to
 *  nothing by an addition to the bar is the addition being wrong, not the
 *  reading. */
constexpr float statusLabelMinWidthOfRowHeight = 3.f;

/** The beat display's width, as a share of the row and of the whole bar.
 *
 *  Both bounds are what resized() has always applied -- two fifths of the row
 *  is the old "four fifths of the half the labels left over", restated
 *  without the chain. Kept here so the meters are laid out against the same
 *  number the indicator is. */
constexpr float statusTickWidthOfRow = 2.f / 5.f;
constexpr float statusTickWidthOfBar = 1.f / 2.f;

/** The beat display's height, as a share of the row. */
constexpr float statusTickHeightOfRow = 3.f / 5.f;

/** How much of its width the beat display may give up to the meters.
 *
 *  It gives width up rather than a label being dropped, and four and five
 *  thin bars need little -- but only down to three quarters of what it would
 *  otherwise have had. Past that the meters are dropped instead: they are the
 *  addition here, where the tempo, the beat and the readout were on this bar
 *  first, and a beat display crushed to a stripe answers no question at all. */
constexpr float statusMeterMinTickShare = 3.f / 4.f;

/** Where everything on the bar goes.
 *
 *  `row` is the band the bar lays out in -- padded top and bottom, with the
 *  MIX and keyboard icons already taken off its right end, so staying inside
 *  it is what keeps the meters clear of them. `barWidth` is the bar's full
 *  width, because the beat display is centred on the bar and not on the row.
 *  `padding` is the air the labels are held off the bar's ends by. */
StatusBarLayout statusBarLayout (juce::Rectangle<int> row, int barWidth,
                                 int padding);

}
