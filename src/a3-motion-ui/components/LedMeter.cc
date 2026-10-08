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
/** The desk's channelLedColour, LED by LED. */
constexpr std::array<std::size_t, ledThresholdsDb.size ()> ledBands{
  vuGreenBand,  vuGreenBand,  vuGreenBand, vuGreenBand,
  vuYellowBand, vuYellowBand, vuRedBand,   vuRedBand,
};

/** The desk's channel_leds(): how many thresholds a linear peak reaches. */
int
ledsReached (float amplitude)
{
  if (!(amplitude > 0.f))
    return 0;
  auto const db = 20.f * std::log10 (amplitude);
  auto const reached = std::count_if (ledThresholdsDb.begin (), ledThresholdsDb.end (),
                                      [db] (float threshold) { return db >= threshold; });
  return static_cast<int> (reached);
}
}

LedMeterLights
ledMeterLights (VuReading reading)
{
  LedMeterLights lights;
  lights.lit = ledsReached (reading.bar);
  auto const held = ledsReached (reading.hold);
  lights.held = held > lights.lit ? held : 0;
  return lights;
}

std::size_t
ledSegmentBand (int segment)
{
  return ledBands[static_cast<std::size_t> (
      std::clamp (segment, 0, ledMeterSegments - 1))];
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

  // The held peak's LED whole, in its own colour, as the desk's firmware
  // lights it above the bar.
  if (lights.held > 0)
    {
      auto const held = lights.held - 1;
      g.setColour (vuBandColour (t, ledSegmentBand (held)));
      g.fillRect (ledSegment (bounds, held));
    }
}

}
