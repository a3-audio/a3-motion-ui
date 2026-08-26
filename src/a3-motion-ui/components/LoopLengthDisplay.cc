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

#include "LoopLengthDisplay.hh"

#include <a3-motion-ui/components/LookAndFeel.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
// Structural opacities: the marks that divide a bar into loop repetitions,
// the single mark where a bar ends inside a longer loop, and the frame around
// the row the encoder is on.
constexpr float repetitionMarkOpacity = 0.35f;
constexpr float barMarkOpacity = 0.2f;
constexpr float highlightOpacity = 0.8f;
}

LoopLengthDisplay::LoopLengthDisplay ()
{
  startTimerHz (60);
}

LoopLengthDisplay::~LoopLengthDisplay ()
{
  stopTimer ();
}

void
LoopLengthDisplay::timerCallback ()
{
  // In EXT mode: interpolate playhead position based on time since last beat
  if (_extClockMode.load () != 0)
    {
      auto now = juce::Time::currentTimeMillis ();
      auto beatTime = _extBeatTime.load ();
      auto period = _extBeatPeriod.load ();
      auto beat = _extBeat.load ();           // 1-based
      auto beatsPerBar = _extBeatsPerBar.load ();

      if (beatTime > 0 && period > 0 && beatsPerBar > 0)
        {
          // Elapsed time since last beat, as fraction of beat period
          auto elapsed = now - beatTime;
          auto beatFraction = static_cast<float> (elapsed)
                              / static_cast<float> (period);
          // Clamp to [0, 1] — don't extrapolate beyond one beat
          beatFraction = juce::jlimit (0.f, 1.f, beatFraction);

          // beat is 1-based; "/beat N" means "beat N just started".
          // At moment of /beat 1, position = 0.0 (bar start).
          // We interpolate from (beat-1)/beatsPerBar to beat/beatsPerBar.
          auto beatStart = static_cast<float> (beat - 1)
                           / static_cast<float> (beatsPerBar);
          auto beatEnd = static_cast<float> (beat)
                         / static_cast<float> (beatsPerBar);

          auto position = beatStart + beatFraction * (beatEnd - beatStart);
          // Wrap to [0, 1)
          position = std::fmod (position, 1.f);
          if (position < 0.f)
            position += 1.f;

          // Set all channels to the same global playhead position
          for (int ch = 0; ch < numChannels; ++ch)
            {
              _channels[ch].playheadPosition = position;
            }
        }
    }

  repaint ();
}

void
LoopLengthDisplay::resized ()
{
}

void
LoopLengthDisplay::paint (juce::Graphics &g)
{
  auto bounds = getLocalBounds ();

  // Grey background matching the StatusBar
  g.setColour (Colours::statusBar ());
  g.fillRect (bounds);

  // Divide into 4 equal channel sections
  auto const sectionWidth = bounds.getWidth () / numChannels;

  for (int ch = 0; ch < numChannels; ++ch)
    {
      auto sectionBounds
          = bounds.removeFromLeft (ch < numChannels - 1 ? sectionWidth
                                                        : bounds.getWidth ());
      paintChannel (g, sectionBounds, ch);

      // Draw thin separator line between sections
      if (ch < numChannels - 1)
        {
          g.setColour (Colours::background ());
          g.drawVerticalLine (sectionBounds.getRight (),
                              static_cast<float> (sectionBounds.getY ()),
                              static_cast<float> (sectionBounds.getBottom ()));
        }
    }
}

void
LoopLengthDisplay::paintChannel (juce::Graphics &g,
                                 juce::Rectangle<int> bounds, int channel)
{
  auto const loopBeats = _channels[channel].loopLengthBeats.load ();
  auto const playhead = _channels[channel].playheadPosition.load ();
  auto const colour = _channels[channel].colour;
  auto const isHighlighted = _channels[channel].rowHighlighted;
  auto const isSelected = _channels[channel].cellSelected;
  auto const beatsPerBar = static_cast<float> (_referenceBeats.load ());

  // Highlight/selection background (same style as PadRowDisplay)
  if (isSelected)
    {
      g.setColour (colour.withAlpha (0.4f));
      g.fillRect (bounds);
    }
  else if (isHighlighted)
    {
      g.setColour (colour.withAlpha (0.15f));
      g.fillRect (bounds);
    }

  auto const leftX = static_cast<float> (bounds.getX ());
  auto const rightX = static_cast<float> (bounds.getRight ());
  auto const top = static_cast<float> (bounds.getY ());
  auto const bottom = static_cast<float> (bounds.getBottom ());
  auto const padding = (bottom - top) * 0.1f;
  auto const drawTop = top + padding;
  auto const drawBottom = bottom - padding;
  auto const drawHeight = drawBottom - drawTop;
  auto const drawWidth = rightX - leftX;

  // Display width = 1 bar (beatsPerBar beats).
  // Playhead = position within the bar (0-1).
  auto const playheadX = leftX + playhead * drawWidth;

  // Solid colour fill from left to playhead
  g.setColour (colour.withAlpha (0.6f));
  g.fillRect (leftX, drawTop, playheadX - leftX, drawHeight);

  // Faint fill for the remaining area
  g.setColour (colour.withAlpha (0.08f));
  g.fillRect (playheadX, drawTop, rightX - playheadX, drawHeight);

  // Loop boundary lines: how many loop repetitions fit in one bar?
  // loopFraction = fraction of the bar that one loop occupies.
  auto const loopFraction = loopBeats / beatsPerBar;

  if (loopFraction > 0.f && loopFraction < 1.f)
    {
      // Short loops: multiple repetitions per bar → many striche
      auto const numReps = static_cast<int> (1.f / loopFraction + 0.5f);
      for (int i = 1; i < numReps; ++i)
        {
          auto const lineX = leftX + i * loopFraction * drawWidth;
          g.setColour (toColour (theme ().textPrimary,
                                 repetitionMarkOpacity));
          g.drawVerticalLine (static_cast<int> (lineX), drawTop, drawBottom);
        }
    }
  else if (loopFraction > 1.f)
    {
      // Long loops: loop longer than 1 bar → show bar boundary within loop
      // One marker showing where the bar ends within the loop
      auto const barEndX = leftX + (1.f / loopFraction) * drawWidth;
      g.setColour (toColour (theme ().textPrimary, barMarkOpacity));
      g.drawVerticalLine (static_cast<int> (barEndX), drawTop, drawBottom);
    }

  // Playhead line (drawn last, on top of everything)
  g.setColour (colour);
  g.fillRect (playheadX - 1.f, drawTop, 2.f, drawHeight);

  // White border when row is highlighted (hovered by encoder)
  if (isHighlighted)
    {
      g.setColour (toColour (theme ().textPrimary, highlightOpacity));
      g.drawRect (bounds, 2);
    }

  // Thin baseline
  g.setColour (colour.withAlpha (0.1f));
  g.drawHorizontalLine (static_cast<int> (drawBottom), leftX, rightX);
}

void
LoopLengthDisplay::setLoopLengthBeats (int channel, float lengthBeats)
{
  jassert (channel >= 0 && channel < numChannels);
  _channels[channel].loopLengthBeats = std::max (0.001f, lengthBeats);
  repaint ();
}

void
LoopLengthDisplay::setPlayheadPosition (int channel, float position)
{
  jassert (channel >= 0 && channel < numChannels);
  _channels[channel].playheadPosition = juce::jlimit (0.f, 1.f, position);
  // No repaint() here — timer repaints at constant framerate
}

void
LoopLengthDisplay::setChannelColour (int channel, juce::Colour colour)
{
  jassert (channel >= 0 && channel < numChannels);
  _channels[channel].colour = colour;
}

void
LoopLengthDisplay::setReferenceBeats (int beats)
{
  _referenceBeats = std::max (1, beats);
  repaint ();
}

void
LoopLengthDisplay::setExternalBeat (int beat, int beatsPerBar)
{
  auto now = juce::Time::currentTimeMillis ();
  auto prevTime = _extBeatTime.load ();

  // Calculate period from time since last beat (only if reasonable)
  if (prevTime > 0)
    {
      auto period = now - prevTime;
      // Sanity check: BPM 30-300 → period 50-2000ms per beat
      if (period >= 50 && period <= 2000)
        {
          _extBeatPeriod = period;
        }
    }

  _extBeat = beat;
  _extBeatsPerBar = beatsPerBar;
  _extBeatTime = now;
}

void
LoopLengthDisplay::setClockMode (int mode)
{
  _extClockMode = mode;
}

void
LoopLengthDisplay::setRowHighlighted (int channel, bool highlighted)
{
  jassert (channel >= 0 && channel < numChannels);
  _channels[channel].rowHighlighted = highlighted;
  repaint ();
}

}
