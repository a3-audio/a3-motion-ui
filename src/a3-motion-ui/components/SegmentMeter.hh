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

/** A channel meter drawn as StemDeck's: a row of LED segments in a dark well,
 *  the unlit ones faintly in their zone's colour, and the held peak as a
 *  short bright line in the segment it stands on. Where the DeskMeter is a
 *  solid bar, this one reads as a lit scale at a glance.
 *
 *  The scale is StemDeck's: the desk's LED thresholds (-36 -24 -18 -12 -9 -6
 *  -3 0 dBFS) stand at k/8 of the length and the length is linear in dB
 *  between them, so a bar and the desk's channel LEDs agree on where a level
 *  is. The zones follow the same eighths: green to the -12 LED, yellow at -9
 *  and -6, red above. */

/** How much of the meter a linear peak fills, 0..1. Not a number, or at or
 *  below silence, is empty. */
float segmentMeterFraction (float amplitude);

/** How many of `segments` a linear peak lights: a segment once the level
 *  reaches its end, as an LED at its threshold. */
int segmentsLit (float amplitude, int segments);

/** A segment's zone, as an index into the band colours, by which LED's
 *  eighth its end stands in. */
std::size_t segmentBand (int index, int segments);

/** The colour of a zone: StemDeck's LED colours, not the skin's, so the
 *  meters on both screens read alike. */
juce::Colour segmentZoneColour (std::size_t band);

/** How many segments a meter this long draws: 24 where there is room, fewer
 *  where a segment would shrink under what can be seen, never none. */
int segmentMeterCount (int lengthPixels);

/** A meter of segments for one channel. Opaque and a rectangle of its own, so
 *  a level change repaints this and nothing around it; it asks for that only
 *  when a segment lights or goes dark, not for every reading. Allocates
 *  nothing while painting. */
class SegmentMeter : public juce::Component
{
public:
  SegmentMeter ();

  /** The direction it fills in: Right is a row, Up a column. */
  void setDirection (VuDirection direction);

  /** Takes a reading; true if the drawing changed and a repaint was asked. */
  bool setReading (VuReading reading);

  void paint (juce::Graphics &) override;
  void resized () override;

private:
  int length () const;
  void countSegments ();

  VuDirection _direction = VuDirection::Right;
  VuReading _reading;
  int _segments = 1;
  int _lit = 0;
  int _held = 0;
};

}
