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
#include <a3-motion-ui/components/VuMeter.hh>

namespace a3
{

/** Where the mixer overlay's controls sit.
 *
 *  One calculation, the way every other page here is laid out: paint() draws
 *  into it and resized() puts the hit areas on it, so the picture and the
 *  target cannot disagree — the lesson ClipSettingsLayout stands on.
 *
 *  **Five vertical strips over one row.** The four channels, the master
 *  beside them as the fifth, and the global filter across the foot — so the
 *  eye runs across five levels instead of jumping between two arrangements,
 *  which is what a row of summing controls under the strips made it do.
 *
 *  The four go through ColumnBreak rather than assuming they fit side by
 *  side, which is what makes this the widest demand the device makes and the
 *  reason that rule exists at all. The master does not: its column takes its
 *  share of the width first, and only the four break. */
struct MixerLayout
{
  ColumnBreak strips;
  std::array<std::array<juce::Rectangle<int>, numMixerControls>,
             static_cast<std::size_t> (numChannelsInitial)>
      controls;
  std::array<juce::Rectangle<int>, numMasterControls> master;
  std::array<juce::Rectangle<int>, numFilterControls> filter;
  /** Each channel's input meter, standing in the volume row to the left of
   *  the control whose level it belongs to. Not one of `controls`: a meter is
   *  read, never touched, so it carries no target and must not be counted
   *  among the things one is put on. */
  std::array<juce::Rectangle<int>,
             static_cast<std::size_t> (numChannelsInitial)>
      channelMeter;
  /** The output block -- the subwoofer and the four speakers -- in the two
   *  rows the master's five controls leave free. Empty on the bar's tab,
   *  which carries one channel and no summing section. */
  std::array<juce::Rectangle<int>, numOutputMeters> outputMeters;
  /** The word under that block. Its own rectangle rather than a share of the
   *  block, because the bars are stepped across an integer cell width and a
   *  caption solved twice is a caption that drifts. */
  juce::Rectangle<int> outputMeterCaption;
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
 *  file's.
 *
 *  The master's five rectangles sit on the channels' own rows — its volume on
 *  the line their volumes are on — and the two rows that leaves it are empty
 *  by design: the output level meters go where a channel's two keys are. */
MixerLayout layOutMixerOverlay (juce::Rectangle<int> area,
                                ControlMetrics metrics);

/** The same arrangement for the bar's MIX tab: one channel, laid across.
 *
 *  The tab has three times the overlay strip's width for a quarter of its
 *  content, so the seven controls go left to right rather than down.
 *  `mixerControlOrder` still decides the order; left to right is the reading
 *  order here.
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
