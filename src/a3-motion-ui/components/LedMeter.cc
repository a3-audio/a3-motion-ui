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

#include "LedMeter.hh"

#include <algorithm>
#include <cmath>

#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
/** A level exactly on a segment's edge has not entered it. Float rounding
 *  puts -18 dBFS a hair above or below its edge; this says which. */
constexpr float segmentEdgeTolerance = 1.0e-3f;

/** The held peak's sliver, as a share of its segment -- StemDeck's. */
constexpr float holdOfSegment = 0.4f;

int
segmentsReached (float amplitude, int segments)
{
  auto const fraction = vuMeterFraction (amplitude);
  if (fraction <= 0.f || segments <= 0)
    return 0;
  auto const reached = static_cast<int> (std::ceil (
      fraction * static_cast<float> (segments) - segmentEdgeTolerance));
  return std::clamp (reached, 0, segments);
}
}

LedMeterLights
ledMeterLights (VuReading reading, int segments)
{
  LedMeterLights lights;
  lights.lit = segmentsReached (reading.bar, segments);
  auto const held = segmentsReached (reading.hold, segments);
  lights.held = held > lights.lit ? held : 0;
  return lights;
}

std::size_t
ledSegmentBand (int segment, int segments)
{
  auto const middle = (static_cast<float> (segment) + 0.5f)
                      / static_cast<float> (std::max (1, segments));
  if (middle <= vuFractionForDb (vuGreenCeilingDb))
    return vuGreenBand;
  if (middle <= vuFractionForDb (vuYellowCeilingDb))
    return vuYellowBand;
  return vuRedBand;
}

juce::Rectangle<float>
ledSegment (juce::Rectangle<int> bounds, int segment, int segments)
{
  // On whole pixels: an LED with a soft, half-covered edge reads as a blur,
  // and the gaps between them would come out uneven.
  auto const well = bounds.toFloat ().reduced (theme ().paddingHair);
  auto const pitch = well.getWidth () / static_cast<float> (std::max (1, segments));
  auto const left = std::round (well.getX () + pitch * static_cast<float> (segment));
  auto const right = std::round (well.getX () + pitch * static_cast<float> (segment + 1))
                     - std::round (theme ().paddingTight);
  return { left, std::round (well.getY ()), std::max (0.f, right - left),
           std::round (well.getHeight ()) };
}

void
paintLedMeter (juce::Graphics &g, juce::Rectangle<int> bounds,
               VuReading reading)
{
  if (bounds.isEmpty ())
    return;

  auto const &t = theme ();
  g.setColour (toColour (t.background));
  g.fillRoundedRectangle (bounds.toFloat (), t.radiusTick);

  // Every LED drawn, the dark ones as a ghost of their band: the meter can be
  // found, and its scale read, before anything plays.
  auto const lights = ledMeterLights (reading);
  for (int i = 0; i < ledMeterSegments; ++i)
    {
      auto const colour = vuBandColour (t, ledSegmentBand (i));
      g.setColour (i < lights.lit ? colour : colour.withAlpha (t.alphaOutline));
      g.fillRect (ledSegment (bounds, i));
    }

  if (lights.held > 0)
    {
      auto const held = lights.held - 1;
      auto sliver = ledSegment (bounds, held);
      g.setColour (vuBandColour (t, ledSegmentBand (held)));
      g.fillRect (sliver.removeFromRight (sliver.getWidth () * holdOfSegment));
    }
}

}
