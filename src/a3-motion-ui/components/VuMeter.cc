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

#include "VuMeter.hh"

#include <cmath>

#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
/** The meter's share of the strip's width.
 *
 *  Measured off REAPER's own mixer on this machine (v7.78, one
 *  "1-channelbus" strip, 92 px wide, read by pixel profile): the meter takes
 *  28 of those 92 px. Written as the measurement rather than as 0.3f so the
 *  next reader can check it against the same picture rather than having to
 *  take the rounded number on trust. */
constexpr float meterWidthOfStrip = 28.f / 92.f;

/** The air between the meter and the controls beside it.
 *
 *  A fifth of the meter's own width, so it keeps its proportion as the strip
 *  grows. What it buys is that the two read as two things rather than as one
 *  wide field with a line in it; at a pixel they would touch, and a meter
 *  touching a control looks like part of that control. */
constexpr float meterGapOfMeterWidth = 1.f / 5.f;

/** How thick the peak's mark is, as a share of the track it lies across.
 *
 *  A sixty-fourth: on the device's own overlay, where a channel's track comes
 *  out around 260 px tall, that is four pixels -- a line laid across the bar
 *  rather than a second block of fill: what it reports is where the signal
 *  reached, and a reading is a mark rather than an area.
 *  Floored at a pixel, since a mark that vanished on a short meter would say
 *  "no transient", which is the one thing a meter must not say untruthfully. */
constexpr float peakMarkOfTrackHeight = 1.f / 64.f;

/** The air between two bars of the output block.
 *
 *  An eighth of a bar's own cell. REAPER separates the bars of a
 *  multi-channel meter by a hairline; a share rather than a count of pixels
 *  so five bars still read as five however wide the master's column comes
 *  out. */
constexpr float outputBarGapOfCell = 1.f / 8.f;

/** The word under the output block, as a share of the block's height.
 *
 *  A fifth, so the word under the meters reads as the same kind of caption as
 *  the word under every control beside them rather than as a heading over
 *  them.
 *
 *  **Never less than half a knob**, which is the floor this device puts
 *  under any part of a control that has to be read: a caption that shrank
 *  with its cell would become unreadable long before the cell itself was too
 *  small to draw. Pot Size is the nearest thing here to a statement of how big
 *  a thing has to be to be read and aimed at, so half of it is what a part of
 *  a control gets when its share of the cell comes out smaller. */
constexpr float outputCaptionOfBlock = 1.f / 5.f;

/** Where each band ends, foot to head, as a share of the track's height.
 *
 *  The head one is the whole bar rather than the top band's own ceiling:
 *  there is nothing above full scale for a fourth band to occupy, and stating
 *  it as 1 is what makes the three exhaust the track between them. */
constexpr std::array<float, numVuMeterBands> bandCeilings{
  vuFractionForDb (vuGreenCeilingDb),
  vuFractionForDb (vuYellowCeilingDb),
  1.f,
};

/** The colour each of those bands is filled in.
 *
 *  Looked up rather than guessed at: `highlight` is the skin's yellow at
 *  255, 214, 10, where `warning` is an orange — and green/orange/red is not
 *  the banding a hand reads without looking. */
juce::Colour
bandColour (Theme const &t, std::size_t band)
{
  if (band == vuGreenBand)
    return toColour (t.accent);
  if (band == vuYellowBand)
    return toColour (t.highlight);
  return toColour (t.danger);
}

/** The fraction of the block that is one bar plus its gap. */
int
outputBarCellWidth (juce::Rectangle<int> bars)
{
  return bars.getWidth () / numOutputMeters;
}
}

juce::int64
vuNowMs ()
{
  return static_cast<juce::int64> (juce::Time::getMillisecondCounterHiRes ());
}

float
vuMeterFraction (float amplitude)
{
  // Written as "not greater than" rather than "less than or equal", so a NaN
  // -- which compares false against everything -- lands on the foot instead
  // of falling through into the logarithm. A malformed message can carry
  // anything, and a meter is the wrong place to find that out.
  if (!(amplitude > 0.f))
    return 0.f;

  auto const db = 20.f * std::log10 (amplitude);
  auto const fraction = (db - vuMeterFloorDb) / (0.f - vuMeterFloorDb);

  return juce::jlimit (0.f, 1.f, fraction);
}

VuMeterGeometry
vuMeterGeometry (juce::Rectangle<int> bounds, VuLevel level)
{
  VuMeterGeometry out{};

  if (bounds.isEmpty ())
    return out;

  out.track = bounds;

  auto const height = bounds.getHeight ();

  auto const rmsHeight = juce::roundToInt (
      static_cast<float> (height) * vuMeterFraction (level.rms));
  if (rmsHeight > 0)
    out.rms = bounds.withTop (bounds.getBottom () - rmsHeight);

  // Every edge in the meter is placed by this one rounding, the fill's foot
  // and the bands' boundaries alike. Rounded separately they would disagree
  // by a pixel and leave a hairline of bare track showing through a solid
  // fill.
  auto const yFor = [&bounds, height] (float fraction) {
    return bounds.getBottom ()
           - juce::roundToInt (static_cast<float> (height) * fraction);
  };

  auto foot = 0.f;
  for (std::size_t i = 0; i < static_cast<std::size_t> (numVuMeterBands); ++i)
    {
      auto const top = yFor (bandCeilings[i]);
      auto const zone = juce::Rectangle<int> (bounds.getX (), top,
                                              bounds.getWidth (),
                                              yFor (foot) - top);

      // The band is the *fill* cut by the zone, never the zone itself: what
      // makes this a meter rather than three lamps is that a band is only
      // drawn as far as the signal has actually reached into it.
      out.bands[i] = zone.getIntersection (out.rms);
      foot = bandCeilings[i];
    }

  auto const peakFraction = vuMeterFraction (level.peak);
  if (peakFraction > 0.f)
    {
      auto const thickness = juce::jmax (
          1, juce::roundToInt (static_cast<float> (height)
                               * peakMarkOfTrackHeight));

      // Clamped into the track rather than trusted to land there: at full
      // scale the mark's top would sit exactly on the track's top edge and
      // its thickness would carry it out the other side.
      auto const top = juce::jlimit (
          bounds.getY (), bounds.getBottom () - thickness,
          bounds.getBottom ()
              - juce::roundToInt (static_cast<float> (height) * peakFraction));

      out.peak = juce::Rectangle<int> (bounds.getX (), top,
                                       bounds.getWidth (), thickness);
    }

  return out;
}

StripColumns
splitStripForMeter (juce::Rectangle<int> strip)
{
  if (strip.isEmpty ())
    return {};

  auto controls = strip;
  auto column = controls.removeFromLeft (juce::roundToInt (
      static_cast<float> (strip.getWidth ()) * meterWidthOfStrip));

  if (column.isEmpty () || controls.isEmpty ())
    return { {}, strip };

  column.removeFromRight (juce::jmax (
      1, juce::roundToInt (static_cast<float> (column.getWidth ())
                           * meterGapOfMeterWidth)));

  if (column.isEmpty ())
    return { {}, strip };

  // The meter keeps the strip's full height, every row of it. Each control
  // beside it draws its own caption inside its own cell, so there is no line
  // here for the meter to stop short of.
  return { column, controls };
}

OutputMeterBlock
outputMeterBlock (juce::Rectangle<int> block, ControlMetrics metrics)
{
  OutputMeterBlock out{};

  if (block.isEmpty ())
    return out;

  auto bars = block;
  out.caption = bars.removeFromBottom (juce::jmax (
      metrics.knobDiam / 2,
      juce::roundToInt (static_cast<float> (block.getHeight ())
                        * outputCaptionOfBlock)));

  auto const cellWidth = outputBarCellWidth (bars);
  if (bars.isEmpty () || cellWidth <= 0)
    {
      out.caption = {};
      return out;
    }

  auto const gap = juce::jmax (
      1, juce::roundToInt (static_cast<float> (cellWidth)
                           * outputBarGapOfCell));

  // Stepped from the left with an integer cell width, the way MixerLayout's
  // cellAcross and ClipSettingsLayout's colW are: taken from the right the
  // remainder lands between the bars rather than against the edge, and a
  // block whose bars are not evenly spaced reads as a fault.
  for (int i = 0; i < numOutputMeters; ++i)
    out.bars[static_cast<std::size_t> (i)]
        = juce::Rectangle<int> (bars.getX () + cellWidth * i, bars.getY (),
                                juce::jmax (1, cellWidth - gap),
                                bars.getHeight ());

  return out;
}

void
VuLevels::set (Meter &meter, VuLevel level, juce::int64 nowMs)
{
  meter.latest = level;

  // A louder peak takes over at once; a quieter one only once the hold is up.
  // Both branches restart the hold, so a signal sitting at one level keeps
  // its mark rather than having it expire underneath a steady sound.
  if (level.peak >= meter.heldPeak || nowMs - meter.heldAtMs > vuPeakHoldMs)
    {
      meter.heldPeak = level.peak;
      meter.heldAtMs = nowMs;
    }
}

VuLevel
VuLevels::read (Meter const &meter, juce::int64 nowMs)
{
  // The rms is always the latest frame's -- it is the level, and holding it
  // would draw a sound that has stopped. Only the mark lingers.
  return { nowMs - meter.heldAtMs <= vuPeakHoldMs ? meter.heldPeak
                                                  : meter.latest.peak,
           meter.latest.rms };
}

void
VuLevels::setChannel (int channel, VuLevel level, juce::int64 nowMs)
{
  if (channel < 0 || channel >= numChannelsInitial)
    return;
  set (_channel[static_cast<std::size_t> (channel)], level, nowMs);
}

void
VuLevels::setOutput (int meter, VuLevel level, juce::int64 nowMs)
{
  if (meter < 0 || meter >= numOutputMeters)
    return;
  set (_output[static_cast<std::size_t> (meter)], level, nowMs);
}

VuLevel
VuLevels::channel (int channel, juce::int64 nowMs) const
{
  if (channel < 0 || channel >= numChannelsInitial)
    return {};
  return read (_channel[static_cast<std::size_t> (channel)], nowMs);
}

VuLevel
VuLevels::output (int meter, juce::int64 nowMs) const
{
  if (meter < 0 || meter >= numOutputMeters)
    return {};
  return read (_output[static_cast<std::size_t> (meter)], nowMs);
}

void
paintVuMeter (juce::Graphics &g, juce::Rectangle<int> bounds, VuLevel level)
{
  auto const geometry = vuMeterGeometry (bounds, level);
  if (geometry.track.isEmpty ())
    return;

  auto const &t = theme ();

  // The strip's own raised surface, so a meter reads as part of the block of
  // controls beside it rather than as a picture laid over it. Left plain
  // above the fill: REAPER draws the bands into the signal and not into the
  // empty track, and a track pre-painted in three colours would read as a
  // meter permanently at full scale.
  g.setColour (toColour (t.surfaceRaised));
  g.fillRect (geometry.track);

  for (std::size_t i = 0; i < static_cast<std::size_t> (numVuMeterBands); ++i)
    if (!geometry.bands[i].isEmpty ())
      {
        g.setColour (bandColour (t, i));
        g.fillRect (geometry.bands[i]);
      }

  if (!geometry.peak.isEmpty ())
    {
      // White, and not one of the three band colours it may land on or beside
      // -- a mark drawn in the yellow of the band under it says "band" where
      // it means "peak", and a red one at full scale would be invisible on
      // exactly the reading a meter exists to shout. It stands over the bare
      // track whenever there is any headroom between the rms and the peak,
      // which with real programme material is always, so reading against the
      // dark is the case it has to answer first.
      g.setColour (toColour (t.textPrimary));
      g.fillRect (geometry.peak);
    }
}

}
