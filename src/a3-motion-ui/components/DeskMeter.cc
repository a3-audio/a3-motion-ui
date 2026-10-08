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

#include "DeskMeter.hh"

#include <algorithm>
#include <cmath>

#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
/** A level that arrives as linear amplitude and is turned back into dB can
 *  land a hair under the threshold it was sent at; this much is the same. */
constexpr float thresholdToleranceDb = 1.0e-4f;

/** One in this many diagonals of a clipping bar is cut out: the desk's
 *  displays hatch an over the same way (CLIP_HATCH). */
constexpr int hatchEvery = 3;

/** A stretch of the well from `from` to `to` pixels above its foot. */
juce::Rectangle<int>
stretch (juce::Rectangle<int> well, VuDirection direction, int from, int to)
{
  if (to <= from)
    return {};
  if (direction == VuDirection::Up)
    return { well.getX (), well.getBottom () - to, well.getWidth (), to - from };
  return { well.getX () + from, well.getY (), to - from, well.getHeight () };
}
}

int
deskLedsLit (float amplitude)
{
  if (!(amplitude > 0.f))
    return 0;
  auto const db = 20.f * std::log10 (amplitude) + thresholdToleranceDb;
  return static_cast<int> (
      std::count_if (deskLedThresholdsDb.begin (), deskLedThresholdsDb.end (),
                     [db] (float threshold) { return db >= threshold; }));
}

float
deskBarFraction (float amplitude)
{
  return static_cast<float> (deskLedsLit (amplitude))
         / static_cast<float> (deskLedCount);
}

DeskMeterGeometry
deskMeterGeometry (juce::Rectangle<int> bounds, VuReading reading,
                   VuDirection direction)
{
  DeskMeterGeometry out;
  if (bounds.isEmpty ())
    return out;

  // The marks' lane beside the bar, as long as a display's tick is: a tenth
  // of the meter's thickness, a pixel at least.
  auto const up = direction == VuDirection::Up;
  auto const thickness = up ? bounds.getWidth () : bounds.getHeight ();
  auto const lane = std::max (
      1, juce::roundToInt (static_cast<float> (thickness) * deskMarkOfMeter));
  auto well = bounds;
  auto const marks = up ? well.removeFromLeft (lane) : well.removeFromTop (lane);
  out.well = well;

  auto const length = up ? well.getHeight () : well.getWidth ();
  auto const pixels = [length] (float fraction) {
    return juce::roundToInt (fraction * static_cast<float> (length));
  };

  auto const bar = pixels (deskBarFraction (reading.bar));
  out.bar = stretch (well, direction, 0, bar);

  std::array<int, numVuMeterBands + 1> const edges{
    0, pixels (deskMarkFractions[0]), pixels (deskMarkFractions[1]), length
  };
  for (std::size_t i = 0; i < out.bands.size (); ++i)
    out.bands[i] = stretch (well, direction, std::min (edges[i], bar),
                            std::min (edges[i + 1], bar));

  // Where a bar at the held level would end, if that is past this bar.
  auto const held = pixels (deskBarFraction (reading.hold));
  if (held > bar)
    {
      auto const line
          = std::max (1, juce::roundToInt (theme ().strokeThick));
      out.hold = stretch (well, direction, std::max (bar, held - line), held);
    }

  // A tick in the first pixel past a bar filled to its share: the bar
  // reaches it exactly when an LED of its colour lights.
  auto const tick = std::max (1, juce::roundToInt (theme ().strokeThin));
  for (std::size_t i = 0; i < out.marks.size (); ++i)
    {
      auto const at = pixels (deskMarkFractions[i]);
      out.marks[i] = up ? juce::Rectangle<int> (marks.getX (),
                                                well.getBottom () - at - tick,
                                                marks.getWidth (), tick)
                        : juce::Rectangle<int> (well.getX () + at, marks.getY (),
                                                tick, marks.getHeight ());
    }
  return out;
}

bool
deskMeterClips (VuReading reading)
{
  return reading.hold > 1.f;
}

void
paintDeskMeter (juce::Graphics &g, juce::Rectangle<int> bounds,
                VuReading reading, VuDirection direction)
{
  if (bounds.isEmpty ())
    return;

  auto const &t = theme ();
  auto const m = deskMeterGeometry (bounds, reading, direction);

  g.setColour (toColour (t.background));
  g.fillRoundedRectangle (m.well.toFloat (), t.radiusTick);

  for (std::size_t i = 0; i < m.bands.size (); ++i)
    if (!m.bands[i].isEmpty ())
      {
        g.setColour (vuBandColour (t, i));
        g.fillRect (m.bands[i]);
      }

  // Hatched while it clips: every third diagonal of pixels cut out, as the
  // desk's displays draw an over. Pixel by pixel rather than as stroked
  // lines, which would blur into a darker bar instead of a hatch.
  if (deskMeterClips (reading) && !m.bar.isEmpty ())
    {
      juce::RectangleList<int> cuts;
      for (auto y = m.bar.getY (); y < m.bar.getBottom (); ++y)
        for (auto x = m.bar.getX (); x < m.bar.getRight (); ++x)
          if ((x + y) % hatchEvery == 0)
            cuts.addWithoutMerging ({ x, y, 1, 1 });
      g.setColour (toColour (t.background));
      g.fillRectList (cuts);
    }

  // White, as the desk's hold row is: never one of the band colours it may
  // stand beside.
  if (!m.hold.isEmpty ())
    {
      g.setColour (toColour (t.textPrimary));
      g.fillRect (m.hold);
    }

  // A scale, not a level: the caption grey, beside the bar.
  g.setColour (toColour (t.textMuted));
  for (auto const &mark : m.marks)
    g.fillRect (mark);
}

}
