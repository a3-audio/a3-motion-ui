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
#include <functional>

namespace a3
{

/**
 * PadRowDisplay shows one row of the pad matrix (4 cells, one per channel).
 * Each cell displays a label for the pad option.
 *
 * Selection states:
 * - Row can be highlighted (encoder is on this row in row-select mode)
 * - Individual cell can be selected (encoder pressed, now editing that option)
 */
class PadRowDisplay : public juce::Component
{
public:
  static constexpr int numChannels = 4;

  /** Trajectory type determines which figure icon is drawn in a cell. */
  enum class TrajectoryType
  {
    Empty,
    Circle,
    FigureOfEight,
    CornerStep,
    Spiral,
    Lissajous,
    Rose,
    Zigzag,
    Ellipse,
    Pendulum,
    Triangle,
    Square,
    Star,
    Bounce,
    Helix,
    Orbit,
    Cross,
    Wave,
    Hypo
  };

  PadRowDisplay (int rowIndex);

  void resized () override;
  void paint (juce::Graphics &g) override;

  /** Set the trajectory type for a specific channel's cell. */
  void setTrajectoryType (int channel, TrajectoryType type);

  /** Set the label text (fallback, shown only for Empty type). */
  void setLabel (int channel, juce::String label);

  /** Set whether this row is highlighted for a given channel
   *  (encoder is pointing at this row in row-select mode). */
  void setRowHighlighted (int channel, bool highlighted);

  /** Set whether a specific channel's cell is actively selected
   *  (encoder pressed, in edit mode). */
  void setCellSelected (int channel, bool selected);

  /** Set the channel colour. */
  void setChannelColour (int channel, juce::Colour colour);

  static constexpr int
  getMinimumHeight ()
  {
    return LayoutHints::lineHeight;
  }

  int getRowIndex () const { return _rowIndex; }

private:
  void paintCell (juce::Graphics &g, juce::Rectangle<int> bounds,
                  int channel);
  void drawTrajectoryIcon (juce::Graphics &g, juce::Rectangle<float> area,
                           TrajectoryType type, juce::Colour colour);

  int _rowIndex;  // 0-3 for pad rows 1-4

  struct CellState
  {
    juce::String label{ "---" };
    TrajectoryType trajectoryType{ TrajectoryType::Empty };
    bool rowHighlighted{ false };
    bool cellSelected{ false };
    juce::Colour colour{ juce::Colours::white };
  };

  std::array<CellState, numChannels> _cells;
};

}
