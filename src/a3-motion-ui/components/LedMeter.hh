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

#include <cstddef>

#include <juce_gui_basics/juce_gui_basics.h>

#include <a3-motion-ui/components/VuMeter.hh>

namespace a3
{

/** How many LEDs a segmented meter has.
 *
 *  Twenty, three decibels each over the meter's sixty: the two band ceilings
 *  (-18 and -6 dBFS) then fall exactly on segment edges, so no LED is half
 *  green and half yellow. A count, not a size -- the segments share whatever
 *  length the meter is given. */
constexpr int ledMeterSegments = 20;

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

/** The segments a reading lights. A segment lights once the level has
 *  entered it, as an LED on a desk does once its threshold is passed. */
LedMeterLights ledMeterLights (VuReading reading,
                               int segments = ledMeterSegments);

/** Which of the meter's colour bands a segment belongs to (vuGreenBand,
 *  vuYellowBand, vuRedBand), by where its middle sits on the scale. */
std::size_t ledSegmentBand (int segment, int segments = ledMeterSegments);

/** Where segment `segment` (0 at the foot) is drawn inside `bounds`: the
 *  well inset by a hair, cut into equal steps with a tight gap between. */
juce::Rectangle<float> ledSegment (juce::Rectangle<int> bounds, int segment,
                                   int segments = ledMeterSegments);

/** A meter of LEDs, foot at the left, in `bounds`.
 *
 *  The LED-segment meter StemDeck and the desk show: a recessed well in the
 *  skin's `background`, every segment drawn, the unlit ones as a ghost of
 *  their band's colour at `alphaOutline`, the lit ones in full, and the held
 *  peak as a sliver at the head of its segment. The bands' colours are
 *  vuBandColour's, so this meter and the continuous one say the same levels
 *  in the same colours. */
void paintLedMeter (juce::Graphics &g, juce::Rectangle<int> bounds,
                    VuReading reading);

}
