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
#include <cstddef>

#include <juce_gui_basics/juce_gui_basics.h>

#include <a3-motion-ui/components/VuMeter.hh>

namespace a3
{

/** A channel's meter as the desk shows it: its eight channel LEDs and its
 *  small displays (a3-mixer `a3_mixer_meters.py`, `display_panel.py`,
 *  decided 2026-10-07). One definition in Motion, for FPV's strips and
 *  FULL's channel meters; the desk's file is the source, and
 *  `DeskMeter.TheDesksOwnCodeAgrees` holds this copy to it.
 *
 *  The LED scale: LED n lights once the peak reaches its threshold. Not
 *  linear in dB -- the top half of the meter is the last 12 dB, where a DJ
 *  steers the gain. */
inline constexpr std::array<float, 8> deskLedThresholdsDb{
  -36.f, -24.f, -18.f, -12.f, -9.f, -6.f, -3.f, 0.f
};
constexpr int deskLedCount = static_cast<int> (deskLedThresholdsDb.size ());

/** Each LED's colour band (the firmware's channelLedColour): four green,
 *  two yellow, two red. */
inline constexpr std::array<std::size_t, 8> deskLedBands{
  vuGreenBand,  vuGreenBand,  vuGreenBand, vuGreenBand,
  vuYellowBand, vuYellowBand, vuRedBand,   vuRedBand,
};

/** Where the displays mark yellow and red beside a bar: the foot of the
 *  first yellow and of the first red LED's step, 4/8 and 6/8
 *  (YELLOW_FROM_FRACTION, RED_FROM_FRACTION). */
inline constexpr std::array<float, 2> deskMarkFractions{ 4.f / 8.f, 6.f / 8.f };

/** A mark's length across the bar, as a share of the bar's thickness
 *  (display_panel MARK_OF_METER). */
constexpr float deskMarkOfMeter = 0.1f;

/** How many LEDs a linear peak lights (the desk's channel_leds). */
int deskLedsLit (float amplitude);

/** How much of the bar a linear peak fills: exactly the LEDs it lights, n
 *  of 8 is n/8 -- the bar steps as the LEDs do (the desk's bar_fraction). */
float deskBarFraction (float amplitude);

/** Where one desk meter's parts are drawn, foot to head along `direction`. */
struct DeskMeterGeometry
{
  juce::Rectangle<int> well;  ///< the bar's recess, drawn empty or not
  juce::Rectangle<int> bar;   ///< the fill
  /** The fill cut at the marks: green, yellow, red. A band is a stretch of
   *  the bar, so a bar into the red is green, yellow and red at once. */
  std::array<juce::Rectangle<int>, numVuMeterBands> bands;
  juce::Rectangle<int> hold;  ///< the held peak's line; empty inside the bar
  /** The yellow and red ticks, beside the bar (left of a column, above a
   *  row), never on it: a scale, not a level. */
  std::array<juce::Rectangle<int>, 2> marks;
};

DeskMeterGeometry deskMeterGeometry (juce::Rectangle<int> bounds,
                                     VuReading reading,
                                     VuDirection direction = VuDirection::Right);

/** Whether the bar is drawn hatched: the held peak is over full scale. The
 *  desk hatches its displays' bar while a clip is held. */
bool deskMeterClips (VuReading reading);

/** The meter, painted: the well in the skin's `background`, the bar in the
 *  band colours (`vuBandColour`), hatched while it clips, the held peak as
 *  a line in `textPrimary` and the marks in `textMuted`. */
void paintDeskMeter (juce::Graphics &g, juce::Rectangle<int> bounds,
                     VuReading reading,
                     VuDirection direction = VuDirection::Right);

}
