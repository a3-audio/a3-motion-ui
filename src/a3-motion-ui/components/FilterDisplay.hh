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
 * FilterDisplay visualizes an Airwindows Isolator 3 style filter
 * at the bottom of the screen. Divided into 4 sections (one per channel).
 *
 * Pot 1 (Sweep): 0.0 = Lowpass, 0.5 = Fullrange, 1.0 = Highpass
 *   Controls which end of the spectrum is filtered out.
 *
 * Pot 2 (Q / Band-Narrower): Moves both cutoffs toward each other.
 *   Low Q = wide passband (classic isolator sweep)
 *   High Q = narrow bandpass centered on the sweep position
 *
 * The visualization shows a filled frequency passband with steep
 * Butterworth-style rolloff on the filter edges.
 */
class FilterDisplay : public juce::Component
{
public:
  static constexpr int numChannels = 4;

  FilterDisplay ();

  void resized () override;
  void paint (juce::Graphics &g) override;

  /** Set the sweep position for a channel (normalized 0-1 from pot1).
   *  0.0 = full lowpass, 0.5 = fullrange, 1.0 = full highpass */
  void setSweep (int channel, float normSweep);

  /** Set the Q / band-narrower for a channel (normalized 0-1 from pot2).
   *  0.0 = wide/no narrowing, 1.0 = maximum narrowing */
  void setQ (int channel, float normQ);

  /** Set the display colour for a channel. */
  void setChannelColour (int channel, juce::Colour colour);

  static constexpr int
  getMinimumHeight ()
  {
    return LayoutHints::lineHeight;
  }

private:
  void paintChannel (juce::Graphics &g, juce::Rectangle<int> bounds,
                     int channel);

  /** Compute the LP and HP cutoff positions (0-1) for the given sweep and Q. */
  static std::pair<float, float> computeCutoffs (float sweep, float q);

  /** Butterworth-style rolloff response at normalized position x
   *  given a cutoff position and rolloff direction. */
  static float butterworthResponse (float x, float cutoff, bool isHighpass,
                                    float steepness);

  struct ChannelFilterState
  {
    std::atomic<float> sweep{ 0.5f };  // 0=LP, 0.5=full, 1.0=HP
    std::atomic<float> q{ 0.0f };      // 0=wide, 1=narrow band
    juce::Colour colour{ juce::Colours::white };
  };

  std::array<ChannelFilterState, numChannels> _channels;
};

}
