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
/** How tall the fader's handle is: a share of the track it travels, or of the
 *  width it spans, whichever asks for more.
 *
 *  The width has a say because a cap is about twice as wide as it is tall, and
 *  the bar's tab has a short wide meter where a twelfth of the track came out
 *  flat -- "im clipmixer ist der faderknob zu gestaucht". */
constexpr float faderHandleOfTrack = 1.f / 12.f;
constexpr float faderHandleOfWidth = 1.f / 2.f;

/** The air between two bars of the output block.
 *
 *  An eighth of a bar's own cell. REAPER separates the bars of a
 *  multi-channel meter by a hairline; a share rather than a count of pixels
 *  so five bars still read as five however wide the master's column comes
 *  out. */
/** How much of the master's column its meters take, at the foot. */
constexpr float outputBarsOfBlock = 1.f / 4.f;

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

/** The fraction of the block that is one bar plus its gap. */
int
outputBarCellWidth (juce::Rectangle<int> bars)
{
  return bars.getWidth () / numOutputMeters;
}
}

juce::Colour
vuBandColour (Theme const &theme, std::size_t band)
{
  if (band == vuGreenBand)
    return toColour (theme.accent);
  if (band == vuYellowBand)
    return toColour (theme.highlight);
  return toColour (theme.danger);
}

VuDot
vuDot (VuLevel level)
{
  // The rms, not the peak. A dot says whether the channel is making sound,
  // and the peak is a transient -- read through a mark with no length it
  // would flicker at every drum hit and read as noise rather than as level.
  // The peak still has somewhere to be: the meters on the MIX page hold it.
  auto const fraction = vuMeterFraction (level.rms);

  VuDot dot;
  if (fraction <= 0.f)
    return dot;   // below the floor: nothing, which is what silence looks like

  dot.visible = true;

  // The band the head of the fill would be in -- the same ceilings the meter
  // cuts its bands on, read through the same table, so the dot turns yellow
  // exactly where the meter's fill first reaches yellow.
  dot.band = static_cast<std::size_t> (numVuMeterBands) - 1;
  for (std::size_t i = 0; i < static_cast<std::size_t> (numVuMeterBands); ++i)
    if (fraction <= bandCeilings[i])
      {
        dot.band = i;
        break;
      }

  dot.alpha
      = vuDotMinAlpha + (1.f - vuDotMinAlpha) * juce::jmin (1.f, fraction);
  return dot;
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

namespace
{
/** The stretch of a meter between two fractions of its length, measured from
 *  where it starts -- the foot of a column, the left edge of a bar.
 *
 *  One function for both directions rather than two arrangements of the same
 *  arithmetic: a sideways meter that rounded differently from an upright one
 *  would put its bands a pixel off its fill. */
juce::Rectangle<int>
meterSlice (juce::Rectangle<int> bounds, VuDirection direction, float from,
            float to)
{
  auto const along = direction == VuDirection::Up ? bounds.getHeight ()
                                                  : bounds.getWidth ();
  auto const start = juce::roundToInt (static_cast<float> (along)
                                       * juce::jlimit (0.f, 1.f, from));
  auto const end = juce::roundToInt (static_cast<float> (along)
                                     * juce::jlimit (0.f, 1.f, to));
  if (end <= start)
    return {};

  return direction == VuDirection::Up
             ? juce::Rectangle<int> (bounds.getX (), bounds.getBottom () - end,
                                     bounds.getWidth (), end - start)
             : juce::Rectangle<int> (bounds.getX () + start, bounds.getY (),
                                     end - start, bounds.getHeight ());
}
}

VuMeterGeometry
vuMeterGeometry (juce::Rectangle<int> bounds, VuLevel level,
                 VuDirection direction)
{
  VuMeterGeometry out{};

  if (bounds.isEmpty ())
    return out;

  out.track = bounds;
  out.rms = meterSlice (bounds, direction, 0.f, vuMeterFraction (level.rms));

  auto foot = 0.f;
  for (std::size_t i = 0; i < static_cast<std::size_t> (numVuMeterBands); ++i)
    {
      // The band is the *fill* cut by the zone, never the zone itself: what
      // makes this a meter rather than three lamps is that a band is only
      // drawn as far as the signal has actually reached into it.
      auto const zone = meterSlice (bounds, direction, foot, bandCeilings[i]);
      out.bands[i] = zone.getIntersection (out.rms);
      foot = bandCeilings[i];
    }

  auto const peakFraction = vuMeterFraction (level.peak);
  if (peakFraction > 0.f)
    {
      auto const along = direction == VuDirection::Up ? bounds.getHeight ()
                                                      : bounds.getWidth ();
      auto const thickness = juce::jmax (
          1, juce::roundToInt (static_cast<float> (along)
                               * peakMarkOfTrackHeight));
      auto const at = juce::roundToInt (static_cast<float> (along)
                                        * peakFraction);

      // Clamped into the track rather than trusted to land there: at full
      // scale the mark would sit exactly on the track's far edge and its
      // thickness would carry it out the other side.
      if (direction == VuDirection::Up)
        out.peak = juce::Rectangle<int> (
            bounds.getX (),
            juce::jlimit (bounds.getY (), bounds.getBottom () - thickness,
                          bounds.getBottom () - at),
            bounds.getWidth (), thickness);
      else
        out.peak = juce::Rectangle<int> (
            juce::jlimit (bounds.getX (), bounds.getRight () - thickness,
                          bounds.getX () + at - thickness),
            bounds.getY (), thickness, bounds.getHeight ());
    }

  return out;
}

namespace
{
/** How thick the handle is on this track -- see faderHandleOfTrack. */
int
faderHandleThickness (juce::Rectangle<int> bounds)
{
  auto const wanted = juce::jmax (
      juce::roundToInt (static_cast<float> (bounds.getHeight ())
                        * faderHandleOfTrack),
      juce::roundToInt (static_cast<float> (bounds.getWidth ())
                        * faderHandleOfWidth));

  // A fingertip is the floor of the ceiling, not the ceiling: on a wide meter
  // half the width is what keeps the cap from reading as a line, and the
  // track's own height is the only real limit above that.
  return juce::jlimit (minimumFaderHandleThickness,
                       juce::jmax (fingertipSize,
                                   juce::jmin (bounds.getWidth () / 2,
                                               bounds.getHeight ())),
                       wanted);
}
}

juce::Rectangle<int>
vuFaderHandleAt (juce::Rectangle<int> bounds, int centreY)
{
  if (bounds.isEmpty ())
    return {};

  auto const thickness = faderHandleThickness (bounds);
  auto const top = juce::jlimit (bounds.getY (),
                                 bounds.getBottom () - thickness,
                                 centreY - thickness / 2);

  return { bounds.getX (), top, bounds.getWidth (), thickness };
}

juce::Rectangle<int>
vuFaderHandle (juce::Rectangle<int> bounds, float value)
{
  if (bounds.isEmpty ())
    return {};

  // NaN fails every comparison, so it is caught here rather than by the clamp.
  auto const travel = value >= 0.f ? juce::jmin (value, 1.f) : 0.f;
  auto const at = bounds.getBottom ()
                  - juce::roundToInt (static_cast<float> (bounds.getHeight ())
                                      * travel);

  return vuFaderHandleAt (bounds, at);
}

float
vuMeterDragVolume (float atPress, int pixelsUp, int meterHeight)
{
  if (meterHeight <= 0)
    return atPress;

  return juce::jlimit (0.f, 1.f,
                       atPress
                           + static_cast<float> (pixelsUp)
                                 / static_cast<float> (meterHeight));
}

void
paintVuFaderCap (juce::Graphics &g, juce::Rectangle<int> track,
                 juce::Rectangle<int> handle, juce::Colour colour)
{
  if (handle.isEmpty ())
    return;

  auto const face = handle.toFloat ();

  // A cap, not a line: opaque, so the bands do not shine through it -- a
  // yellow channel's handle over the yellow band was a line that disappeared
  // exactly where it mattered -- and washed in the channel's colour, so it
  // still says whose fader this is.
  g.setColour (toColour (theme ().surfaceRaised));
  g.fillRoundedRectangle (face, theme ().radiusControl);
  g.setColour (colour.withAlpha (theme ().alphaFillEmphasis));
  g.fillRoundedRectangle (face, theme ().radiusControl);
  g.setColour (colour);
  g.drawRoundedRectangle (face, theme ().radiusControl, theme ().strokeThick);

  // The groove across its middle, the mark a hand reads a fader's position
  // off on any desk.
  auto const groove = juce::Rectangle<float> (
      face.getX (), face.getCentreY () - theme ().strokeThick * 0.5f,
      face.getWidth (), theme ().strokeThick);
  g.setColour (colour);
  g.fillRect (groove.getIntersection (track.toFloat ()));
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

  // A share of the block, but never more than a knob is tall: the block was
  // two rows when this was written and is the master's whole column now,
  // where a fifth of it is a caption taller than the word in it.
  auto bars = block;
  out.caption = bars.removeFromBottom (juce::jlimit (
      metrics.knobDiam / 2, metrics.knobDiam,
      juce::roundToInt (static_cast<float> (block.getHeight ())
                        * outputCaptionOfBlock)));

  // Only the foot of the column: the rest is the master's fader track, and
  // five bars filling the whole of it read as a wall rather than as meters.
  bars = bars.removeFromBottom (juce::roundToInt (
      static_cast<float> (bars.getHeight ()) * outputBarsOfBlock));

  auto const cellHeight = bars.getHeight () / numOutputMeters;
  if (bars.isEmpty () || cellHeight <= 0)
    {
      out.caption = {};
      return out;
    }

  auto const gap = juce::jmax (
      1, juce::roundToInt (static_cast<float> (cellHeight)
                           * outputBarGapOfCell));

  // Stacked rather than side by side, and counted up from the foot: the bars
  // are turned a quarter (VuDirection::Right), and the subwoofer -- meter 0 --
  // stands at the bottom, where a subwoofer stands in the room.
  stepMeterBarsUp (bars, cellHeight, gap, out.bars);

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
paintVuMeter (juce::Graphics &g, juce::Rectangle<int> bounds, VuLevel level,
              VuDirection direction)
{
  // The strip's own raised surface, so a meter reads as part of the block of
  // controls beside it rather than as a picture laid over it — and, on the
  // page this was drawn for, as a channel cut into the black the mixer fills
  // itself with.
  paintVuMeter (g, bounds, level, toColour (theme ().surfaceRaised), direction);
}

void
paintVuMeter (juce::Graphics &g, juce::Rectangle<int> bounds, VuLevel level,
              juce::Colour track, VuDirection direction)
{
  auto const geometry = vuMeterGeometry (bounds, level, direction);
  if (geometry.track.isEmpty ())
    return;

  auto const &t = theme ();

  // Left plain above the fill: REAPER draws the bands into the signal and not
  // into the empty track, and a track pre-painted in three colours would read
  // as a meter permanently at full scale.
  g.setColour (track);
  g.fillRect (geometry.track);

  for (std::size_t i = 0; i < static_cast<std::size_t> (numVuMeterBands); ++i)
    if (!geometry.bands[i].isEmpty ())
      {
        g.setColour (vuBandColour (t, i));
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
