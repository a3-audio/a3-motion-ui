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

#include <array>
#include <functional>

#include <JuceHeader.h>

#include <a3-motion-ui/components/VuMeter.hh>
#include <a3-motion-ui/components/fpv/FpvLayout.hh>
#include <a3-motion-ui/theme/ThemedComponent.hh>

namespace a3
{

/** What one strip shows of its channel. */
struct FpvChannel
{
  juce::Colour colour;
  juce::String clipName;       // empty = no clip
  bool playing = false;
  std::array<float, 3> pots{}; // effective 3D, FREQ, Q (channelPotOrder), 0..1
  bool orbit = false;          // flies the gravity field rather than its clip
  int escort = -1;             // the body id it escorts; -1 = patrol
  float escortMass = 1.f;      // that body's mass, for the size of its disc
};

/** Where the header's mode pill sits: CLIP hollow, ORBIT filled. The mode is
 *  a shape before it is a word, so it reads at a glance. */
juce::Rectangle<float> fpvModePill (juce::Rectangle<int> header);

/** The bottom third in FPV: one strip per channel. Its bounds are the strips'
 *  row only; strip rectangles come from fpvStripRow. Painted from plain data,
 *  so a test can set it up without a running app. */
class FpvStrips : public juce::Component, public ThemedComponent
{
public:
  void setChannels (std::array<FpvChannel, 4> const &channels);

  /** The meter reading of a channel; unset, the meters read silence. */
  std::function<VuReading (int channel)> channelLevel;

  /** Colours come with setChannels, the rest is read while painting. */
  void applyTheme () override;
  void paint (juce::Graphics &) override;
  void resized () override;

  /** The strip rectangles in local coordinates (for tests and taps later). */
  std::array<FpvStrip, 4> const &strips () const;

private:
  std::array<FpvChannel, 4> _channels;
  std::array<FpvStrip, 4> _strips;
};

}
