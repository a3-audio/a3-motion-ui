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

#include <a3-motion-ui/theme/ThemeColours.hh>

#include <a3-motion-ui/components/LayoutHints.hh>

#include <array>

namespace a3
{

/**
 * ElevationDisplay shows the current elevation coverage setting.
 * The elevation coverage controls how far around the sphere the
 * pattern trajectories wrap: from top-only to full sphere.
 *
 * Displays one cell per channel (like other option bars), each
 * showing the same global coverage value. Per-channel highlight
 * and selection states mirror the encoder navigation pattern.
 */
class ElevationDisplay : public juce::Component
{
public:
  static constexpr int numChannels = 4;

  ElevationDisplay ();

  void resized () override;
  void paint (juce::Graphics &g) override;

  /** Set the display colour for a channel. */
  void setChannelColour (int channel, juce::Colour colour);

  static constexpr int
  getMinimumHeight ()
  {
    return LayoutHints::lineHeight;
  }

private:
  void paintCell (juce::Graphics &g, juce::Rectangle<int> bounds,
                  int channel);

  struct CellState
  {
    bool rowHighlighted{ false };
    bool cellSelected{ false };
    /** Read at construction, so a cell built after a skin change
     *  carries the new skin's text colour. */
    juce::Colour colour = toColour (theme ().textPrimary);
    float coverage{ 0.5f };  // per-channel coverage (0.5 = hemisphere)
  };

  std::array<CellState, numChannels> _cells;
};

}
