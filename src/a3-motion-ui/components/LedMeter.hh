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

/** The desk's channel LED scale (a3-mixer `a3_mixer_meters.py`,
 *  CHANNEL_LED_THRESHOLDS_DB, decided by the maintainer 2026-10-07), in dBFS
 *  peak, foot to head: LED n lights once the peak reaches its threshold.
 *
 *  A copy, because the desk's numbers live only in a3-mixer's code and the
 *  truth carries the meters' timing but not their scale. `LedMeter` tests
 *  hold it to the desk's file wherever an a3-mixer checkout is beside this
 *  one. Not dB-linear: the top half of the meter is the last 12 dB, where a
 *  DJ steers the gain. */
inline constexpr std::array<float, 8> ledThresholdsDb{ -36.f, -24.f, -18.f, -12.f,
                                                      -9.f,  -6.f,  -3.f,  0.f };

/** One segment per desk LED. */
constexpr int ledMeterSegments = static_cast<int> (ledThresholdsDb.size ());

/** What a segmented meter lights.
 *
 *  `lit` counts segments from the foot. `held` is the segment the hold line
 *  stands in, counted the same way (1 is the first), and 0 when the hold is
 *  not above the bar -- then the bar's own head already says it. */
struct LedMeterLights
{
  int lit = 0;
  int held = 0;
};

/** The LEDs a reading lights, as the desk counts them: every threshold the
 *  bar's peak has reached, and the held peak's LED likewise. */
LedMeterLights ledMeterLights (VuReading reading);

/** An LED's colour band, the desk's channelLedColour: four green, two
 *  yellow, two red. */
std::size_t ledSegmentBand (int segment);

/** Where segment `segment` (0 at the foot) is drawn inside `bounds`: the
 *  well inset by a hair, cut into equal steps with a tight gap between. */
juce::Rectangle<float> ledSegment (juce::Rectangle<int> bounds, int segment,
                                   int segments = ledMeterSegments);

/** A meter of LEDs, foot at the left, in `bounds`.
 *
 *  The desk's channel meter on the screen: a recessed well in the skin's
 *  `background`, every LED drawn, the unlit ones as a ghost of their band's
 *  colour at `alphaOutline`, the lit ones in full, and the held peak's LED
 *  lit whole, as the desk's firmware lights it. The bands' colours are
 *  vuBandColour's, so this meter and the continuous one say the same levels
 *  in the same colours. */
void paintLedMeter (juce::Graphics &g, juce::Rectangle<int> bounds,
                    VuReading reading);

}
