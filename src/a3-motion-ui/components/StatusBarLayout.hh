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


namespace a3
{

/** Everything the status bar puts between its left edge and the two icons at
 *  its right end.
 *
 *  A pure function with a test of its own, the way ClipSettingsLayout and
 *  ControllerLayout are, so paint() draws into exactly the rectangles the
 *  test checks.
 *
 *  **It carried nine VU meters until 2026-09-12** -- four inputs left of the
 *  beat display, five outputs right of it -- and most of this file was the
 *  negotiation over who gave width up to whom. They are gone: "die vu-meter
 *  in der statusleiste sind too much. das machts unuebersichtlich."
 *
 *  The inputs moved somewhere better rather than away. A channel's level is
 *  now a dot on that channel's own face in the bar below (VuMeter.hh's
 *  vuDot), which is where a hand looking for a channel is already looking.
 *  The five outputs needed no new home: the MIX page's master column has had
 *  the same five, full height, all along.
 */
struct StatusBarLayout
{
  /** The tempo reading, at the left end. */
  juce::Rectangle<int> bpm;

  /** The beat display, centred on the whole bar rather than on what the rest
   *  leaves over: it is the one thing here that is looked at rather than
   *  read, and an off-centre one reads as a mistake. */
  juce::Rectangle<int> tick;

  /** What was last turned, at the right end. */
  juce::Rectangle<int> readout;
};

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
