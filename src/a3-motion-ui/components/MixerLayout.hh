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

#include <a3-motion-ui/components/ClipSettingsLayout.hh>
#include <a3-motion-ui/components/ColumnBreak.hh>
#include <a3-motion-ui/components/MixerControls.hh>

namespace a3
{

/** Where the mixer overlay's controls sit.
 *
 *  One calculation, the way every other page here is laid out: paint() draws
 *  into it and resized() puts the hit areas on it, so the picture and the
 *  target cannot disagree — the lesson ClipSettingsLayout stands on.
 *
 *  Four channel strips over one row that belongs to the whole mixer: the
 *  summing section and the one filter. The strips go through ColumnBreak
 *  rather than assuming four fit side by side, which is what makes this the
 *  widest demand the device makes and the reason that rule exists at all. */
struct MixerLayout
{
  ColumnBreak strips;
  std::array<std::array<juce::Rectangle<int>, numMixerControls>,
             static_cast<std::size_t> (numChannelsInitial)>
      controls;
  std::array<juce::Rectangle<int>, numMasterControls> master;
  std::array<juce::Rectangle<int>, numFilterControls> filter;
  /** False when a control would come out under a fingertip. The overlay then
   *  says so in one line of text rather than drawing targets nobody can hit —
   *  a mixer that cannot be operated is worse than a sentence saying the
   *  window is too small. */
  bool fits;
};

/** The arrangement for `area`.
 *
 *  `metrics` is the skin's, and the only thing taken from it is how big a
 *  knob wants to be: a row has to hold both a fingertip and the pot drawn in
 *  it, and how big that pot is is the performer's setting rather than this
 *  file's. */
MixerLayout layOutMixerOverlay (juce::Rectangle<int> area,
                                ControlMetrics metrics);

/** The same arrangement for the bar's MIX tab: one channel, laid across.
 *
 *  The tab has three times the overlay strip's width for a quarter of its
 *  content, so the seven controls go left to right rather than down — and
 *  the cells come out tall and narrow, which is the shape the fader wants
 *  anyway. `mixerControlOrder` still decides the order; left to right is the
 *  reading order here.
 *
 *  `master` and `filter` stay empty. The tab is about one channel, and the
 *  whole mixer is one tap away on the status bar's MIX key — a summing
 *  section squeezed into a quarter of the bar would be neither.
 *
 *  It returns the same struct as the overlay's so the two components draw
 *  from one shape. Two structs would be two pictures of the same seven
 *  controls, and those drift. */
MixerLayout layOutMixerStrip (juce::Rectangle<int> area,
                              ControlMetrics metrics);

}
