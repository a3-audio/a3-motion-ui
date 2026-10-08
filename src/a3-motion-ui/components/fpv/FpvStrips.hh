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

#include <a3-motion-engine/flight/FlightTuning.hh>
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
  float escortMass = FlightTuning{}.groupMass; // that body's mass, for the size of its disc
};

/** The header's channel plate: "CH n" on the channel's colour, StemDeck's
 *  deck plate. Left of the mode key. */
juce::Rectangle<float> fpvHeaderPlate (juce::Rectangle<int> header);

/** Where the header's mode key sits, at its right: CLIP on the idle key face,
 *  ORBIT on the lifted one. The mode is a ground before it is a word, so it
 *  reads at a glance. */
juce::Rectangle<float> fpvModeKey (juce::Rectangle<int> header);

/** The clip row's play-state key, a square at the row's right end: lit in
 *  play's colour while the clip runs, the idle face while it does not. */
juce::Rectangle<float> fpvPlayKey (juce::Rectangle<int> clip);

/** What a channel's plate is filled with: its colour, dimmed (hue kept) to
 *  no lighter than the skin's caption ink. A plate names a column; it is not
 *  a light, and four full-strength plates would be the brightest thing under
 *  the sphere at rest. */
juce::Colour fpvPlateColour (juce::Colour channel);

/** The ink for a word on a coloured ground: the skin's `textOnAccent` or its
 *  `textPrimary`, whichever stands out more. */
juce::Colour fpvInkOn (juce::Colour ground);

/** The bottom third in FPV: one strip per channel. Its bounds are the strips'
 *  row only; strip rectangles come from fpvStripRow. Painted from plain data,
 *  so a test can set it up without a running app. */
class FpvStrips : public juce::Component, public ThemedComponent
{
public:
  FpvStrips ();

  void setChannels (std::array<FpvChannel, 4> const &channels);
  /** The engine's flight tuning: the escort disc is sized by its masses. */
  void setFlightTuning (FlightTuning const &tuning) { _tuning = tuning; }

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
  FlightTuning _tuning;
  std::array<FpvStrip, 4> _strips;
};

}
