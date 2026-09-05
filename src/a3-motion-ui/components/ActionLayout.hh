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

#include <a3-motion-ui/components/ClipSettingsLayout.hh>

#include <array>

namespace a3
{

/** Where the ACTION page puts things.
 *
 *  A whole page rather than a section, so the controls sit at whatever size it
 *  can afford -- which is a lot, since nothing else is competing for the room.
 *  Above them, which action clip this slot fires.
 */
struct ActionLayout
{
  /** Which action clip the ACT key fires on this slot, named. Chosen in the
   *  file menu beside the clips, so what stands here is a reading rather than
   *  a control. */
  juce::Rectangle<int> actionField;

  /** The mode, up beside the name rather than at the end of a row.
   *
   *  It says what a press does to all three envelopes, so it belongs to none
   *  of them -- and it is a state you want to find without reading, which the
   *  tail of the bottom row is not. */
  juce::Rectangle<int> actModeField;

  /** Three envelopes' worth of attack, decay and ceiling, in reading order --
   *  which is also the order the handler expects. Accent first, because it is
   *  what ACT has always done, then the cutoff, then the resonance. */
  static constexpr int numRows = 3;
  std::array<juce::Rectangle<int>, numRows * 3> controls;

  /** The bands the controls stand on, top to bottom. */
  std::array<juce::Rectangle<int>, numRows> rows;

  /** Which envelope each row is, named in the column beside it. */
  std::array<juce::Rectangle<int>, numRows> rowLabels;

  /** Knob size and text sizes, worked out for these cells -- the same
   *  numbers the clip bar hands its own knobs, so the two pages draw one
   *  knob rather than two similar ones. */
  ControlMetrics metrics{ 0, 0.f, 0.f };
};

/** @param headerSize the theme's header size, which the control row is
 *                    measured against.
 *  @param bodySize   the theme's body size, which sets the knob and its
 *                    captions. */
ActionLayout layOutActionPage (juce::Rectangle<int> bounds,
                               float headerSize, float bodySize,
                               float potSizeScale);

}
