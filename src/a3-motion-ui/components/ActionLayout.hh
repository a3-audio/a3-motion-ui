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

  /** Two envelopes and the mode that fires them, in reading order -- which is
   *  also the order the handler expects.
   *
   *  Row one is the accent: where the sound goes. Row two is the filter: freq
   *  and Q swept together. The mode closes the second row, because it says
   *  what pressing the key does to both of them. */
  std::array<juce::Rectangle<int>, 7> controls;

  /** The two bands the controls stand on. */
  juce::Rectangle<int> controlRow;
  juce::Rectangle<int> filterRow;

  /** Which envelope each row is, named beside it. */
  juce::Rectangle<int> accentLabel;
  juce::Rectangle<int> filterLabel;

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
