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

#include <JuceHeader.h>

#include <a3-motion-ui/components/LayoutHints.hh>

#include <array>
#include <atomic>

namespace a3
{

/**
 * LoopLengthDisplay visualizes the global playhead position and loop
 * length for each channel. The display width represents one bar of
 * time. A solid colour fill from left to the playhead shows the
 * current position. Thin vertical lines overlaid on the fill mark
 * loop boundaries — more lines when loops are short (fast pattern
 * repetition), fewer when loops are long.
 *
 * The playhead always runs with the clock, independent of whether
 * a pattern is playing or not.
 */
class LoopLengthDisplay : public juce::Component,
                         private juce::Timer
{
public:
  static constexpr int numChannels = 4;

  LoopLengthDisplay ();
  ~LoopLengthDisplay () override;

  void resized () override;
  void paint (juce::Graphics &g) override;

  /** Set the loop length in beats for a channel (can be fractional).
   *  This determines how many loop-boundary lines are drawn. */
  void setLoopLengthBeats (int channel, float lengthBeats);

  /** Set the playhead position for a channel (normalized 0.0 - 1.0
   *  within the display reference period). Always driven by the
   *  global clock. */
  void setPlayheadPosition (int channel, float position);

  /** Set the display colour for a channel. */
  void setChannelColour (int channel, juce::Colour colour);

  /** Set the reference period in beats (display width = this many beats).
   *  Typically beatsPerBar (4). */
  void setReferenceBeats (int beats);

  /** Call on each external /beat OSC.  Stores beat position and timestamp.
   *  The timer will interpolate smoothly from this beat to the next,
   *  using the time between the previous two beats as the expected period.
   *  @param beat 1-based beat number (1..beatsPerBar)
   *  @param beatsPerBar number of beats per bar (typically 4) */
  void setExternalBeat (int beat, int beatsPerBar);

  /** Enable/disable external clock mode.  In EXT mode, the playhead is
   *  interpolated from setExternalBeat().  In INT mode, use setPlayheadPosition(). */
  void setClockMode (int mode);

  /** Set whether this row is highlighted for a given channel
   *  (encoder is pointing at this row in row-select mode). */
  void setRowHighlighted (int channel, bool highlighted);

  static constexpr int
  getMinimumHeight ()
  {
    return LayoutHints::lineHeight;
  }

private:
  void timerCallback () override;
  void paintChannel (juce::Graphics &g, juce::Rectangle<int> bounds,
                     int channel);

  struct ChannelLoopState
  {
    std::atomic<float> loopLengthBeats{ 4.f };
    std::atomic<float> playheadPosition{ 0.f };
    juce::Colour colour{ juce::Colours::white };
    bool rowHighlighted{ false };
    bool cellSelected{ false };
  };

  std::array<ChannelLoopState, numChannels> _channels;
  std::atomic<int> _referenceBeats{ 4 };

  // External clock interpolation state
  std::atomic<int> _extClockMode{ 0 };
  std::atomic<int> _extBeat{ 1 };              // last received beat (1-based)
  std::atomic<int> _extBeatsPerBar{ 4 };
  std::atomic<juce::int64> _extBeatTime{ 0 };  // timestamp of last beat (ms)
  std::atomic<juce::int64> _extBeatPeriod{ 500 }; // time between beats (ms)
};

}
