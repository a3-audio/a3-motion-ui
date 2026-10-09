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

#include "SegmentMeter.hh"

#include <algorithm>
#include <cmath>

#include <a3-motion-ui/components/DeskMeter.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
constexpr int mostSegments = 24;

/** Under this a segment and its gap blur into a smear; StemDeck's floor. */
constexpr int leastSegmentPitch = 3;

/** An unlit segment: its zone's colour this faint over the well, so the
 *  scale is there to read before anything lights. */
constexpr float unlitAlpha = 0.12f;

/** The held peak's line, as a share of its segment. */
constexpr float holdShareOfSegment = 0.4f;

/** A level that arrives as linear amplitude and is turned back into dB can
 *  land a hair under the threshold it was sent at; this much is the same. */
constexpr float toleranceDb = 1.0e-4f;

/** Where the bar ends: the first LED step's slope run down to nothing. */
constexpr float floorDb
    = 2.f * deskLedThresholdsDb[0] - deskLedThresholdsDb[1];

/** The segment `index` of `segments` along `well`, a gap before its end and
 *  a hair in from the well across. */
juce::Rectangle<float>
segmentAt (juce::Rectangle<float> well, VuDirection direction, int index,
           int segments, float inset)
{
  auto const up = direction == VuDirection::Up;
  auto const pitch
      = (up ? well.getHeight () : well.getWidth ()) / static_cast<float> (segments);
  auto const gap = std::max (1.f, pitch / 3.f);
  auto const along = pitch * static_cast<float> (index);
  if (up)
    return { well.getX () + inset, well.getBottom () - along - pitch + gap,
             well.getWidth () - 2.f * inset, pitch - gap };
  return { well.getX () + along, well.getY () + inset, pitch - gap,
           well.getHeight () - 2.f * inset };
}

/** The leading `share` of a segment along the direction it fills. */
juce::Rectangle<float>
leading (juce::Rectangle<float> segment, VuDirection direction, float share)
{
  if (direction == VuDirection::Up)
    return segment.removeFromTop (std::max (1.f, segment.getHeight () * share));
  return segment.removeFromRight (std::max (1.f, segment.getWidth () * share));
}
}

namespace
{
float
fractionOfDb (float db)
{
  if (db <= floorDb)
    return 0.f;

  auto lower = floorDb;
  for (int led = 0; led < deskLedCount; ++led)
    {
      auto const upper = deskLedThresholdsDb[static_cast<std::size_t> (led)];
      if (db <= upper)
        return (static_cast<float> (led) + (db - lower) / (upper - lower))
               / static_cast<float> (deskLedCount);
      lower = upper;
    }
  return 1.f;
}

float
decibels (float amplitude)
{
  return 20.f * std::log10 (amplitude);
}
}

float
segmentMeterFraction (float amplitude)
{
  return amplitude > 0.f ? fractionOfDb (decibels (amplitude)) : 0.f;
}

int
segmentsLit (float amplitude, int segments)
{
  // The epsilon keeps a level exactly on a segment's end from rounding below it.
  if (!(amplitude > 0.f))
    return 0;
  auto const fraction = fractionOfDb (decibels (amplitude) + toleranceDb);
  auto const lit = static_cast<int> (
      std::floor (fraction * static_cast<float> (segments) + 1.0e-4f));
  return std::clamp (lit, 0, segments);
}

std::size_t
segmentBand (int index, int segments)
{
  auto const end = static_cast<float> (index + 1) / static_cast<float> (segments);
  if (end > deskMarkFractions[1])
    return vuRedBand;
  if (end > deskMarkFractions[0])
    return vuYellowBand;
  return vuGreenBand;
}

juce::Colour
segmentZoneColour (Theme const &theme, std::size_t band)
{
  if (band == vuRedBand)
    return toColour (theme.meterOver);
  if (band == vuYellowBand)
    return toColour (theme.meterHot);
  return toColour (theme.meterNormal);
}

int
segmentMeterCount (int lengthPixels)
{
  return std::clamp (lengthPixels / leastSegmentPitch, 1, mostSegments);
}

int
SegmentMeter::length () const
{
  return _direction == VuDirection::Up ? getHeight () : getWidth ();
}

void
SegmentMeter::countSegments ()
{
  _segments = segmentMeterCount (length ());
  _lit = segmentsLit (_reading.bar, _segments);
  _held = segmentsLit (_reading.hold, _segments);
}

void
SegmentMeter::setDirection (VuDirection direction)
{
  _direction = direction;
  countSegments ();
  repaint ();
}

SegmentMeter::SegmentMeter ()
{
  setOpaque (true);
}

void
SegmentMeter::resized ()
{
  countSegments ();
}

bool
SegmentMeter::setReading (VuReading reading)
{
  _reading = reading;
  auto const lit = segmentsLit (reading.bar, _segments);
  auto const held = segmentsLit (reading.hold, _segments);
  if (lit == _lit && held == _held)
    return false;

  _lit = lit;
  _held = held;
  repaint ();
  return true;
}

void
SegmentMeter::paint (juce::Graphics &g)
{
  auto const &t = theme ();
  auto const bounds = getLocalBounds ().toFloat ();

  // The card behind the well, so the rounded corners of an opaque component
  // are not left unpainted.
  g.setColour (toColour (t.surface));
  g.fillRect (bounds);
  g.setColour (toColour (t.background));
  g.fillRoundedRectangle (bounds, t.radiusTick);

  auto const inset = std::max (1.f, t.strokeThin);
  for (int i = 0; i < _segments; ++i)
    {
      auto const colour = segmentZoneColour (t, segmentBand (i, _segments));
      g.setColour (i < _lit ? colour : colour.withAlpha (unlitAlpha));
      g.fillRect (segmentAt (bounds, _direction, i, _segments, inset));
    }

  // The held peak, only where it stands above the bar.
  if (_held > _lit)
    {
      auto const index = _held - 1;
      g.setColour (segmentZoneColour (t, segmentBand (index, _segments)));
      g.fillRect (leading (segmentAt (bounds, _direction, index, _segments, inset),
                           _direction, holdShareOfSegment));
    }
}

}
