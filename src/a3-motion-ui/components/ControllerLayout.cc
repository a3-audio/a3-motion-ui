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

#include "ControllerLayout.hh"

namespace a3
{

namespace
{
/** The breathing room between two pads, and between the grid and the edges. */
constexpr int padGap = 4;
constexpr int minPadding = 4;

/** The panel as a grid of equal square cells (InputOutputAdapterV3.hh):
 *  six rows, the pads in rows 2-5 between the key columns. Across: the
 *  panel's ten -- the left column (TAP and clock over the scene column), two
 *  per channel, and the right-hand key column. The panel's rows 0-1 over the pads carry its pots, which the
 *  screen has elsewhere, so that band stays empty here as it is on the
 *  panel. */
constexpr int panelRows = 6;
constexpr int firstPadRow = 2;
constexpr int sceneColumns = 1;
constexpr int gridColumns
    = sceneColumns + 2 * static_cast<int> (numChannelColumns) + 1;
constexpr int rightKeyColumn = gridColumns - 1;
constexpr int leftKeyColumn = 0;

/** The one cell size, and where the grid starts: square cells, as large as
 *  the area allows in whichever direction runs out first, the grid centred
 *  in the rest. Stretching cells to fill both directions is what pulled the
 *  pads tall. */
struct PanelGrid
{
  juce::Point<int> origin;
  int cell = 0;

  juce::Rectangle<int>
  at (int column, int row) const
  {
    return { origin.x + column * (cell + padGap),
             origin.y + row * (cell + padGap), cell, cell };
  }
};

PanelGrid
fitPanelGrid (juce::Rectangle<int> area)
{
  auto const across = (area.getWidth () - (gridColumns - 1) * padGap) / gridColumns;
  auto const down = (area.getHeight () - (panelRows - 1) * padGap) / panelRows;
  auto const cell = juce::jmax (0, juce::jmin (across, down));

  auto const width = gridColumns * cell + (gridColumns - 1) * padGap;
  auto const height = panelRows * cell + (panelRows - 1) * padGap;
  return { area.withSizeKeepingCentre (width, height).getPosition (), cell };
}

/** Where a pad sits inside its clip's box, as a column and a row.
 *
 *  Read off the panel, not derived from the pad indices — those say which pad
 *  is which function, but only the hardware says where that function sits
 *  under a hand. The first arrangement here was taken from the index order
 *  and had action and stop the wrong way round; pressing the pad drawn as
 *  ACT reported STOP.
 *
 *  Play and stop on top, action and settings below. Keyed on the function so
 *  a pad is where it is because of what it does.
 */
juce::Point<int>
padCellInBox (index_t pad)
{
  // As the panel stands: two columns of four, pads 0..3 down the left and
  // 4..7 down the right (padFunctionByPadIndex says which is which).
  return { static_cast<int> (pad / 4), static_cast<int> (pad % 4) };
}
}

ControllerLayout
layOutController (juce::Rectangle<int> contentArea, float, int)
{
  ControllerLayout out;
  auto const grid = fitPanelGrid (contentArea.reduced (minPadding, minPadding));

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    {
      auto const firstColumn = sceneColumns + 2 * static_cast<int> (channel);
      for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
        {
          auto const cell = padCellInBox (pad);
          out.pads[channel][pad]
              = grid.at (firstColumn + cell.x, firstPadRow + cell.y);
        }

      out.clipBoxes[channel][0]
          = grid.at (firstColumn, firstPadRow)
                .getUnion (grid.at (firstColumn + 1, panelRows - 1));
    }

  // The scene column, in the panel's left column: its pad `p` level with
  // every channel's pad `p`.
  for (std::size_t pad = 0; pad < numSceneRows; ++pad)
    {
      auto const cell = padCellInBox (static_cast<index_t> (pad));
      out.scenes[0][pad] = grid.at (leftKeyColumn, firstPadRow + cell.y);
    }

  for (std::size_t i = 0; i < numPanelKeys; ++i)
    {
      auto const place = panelKeyPlaces[i];
      out.keys[i] = grid.at (place.side == PanelSide::Left ? leftKeyColumn
                                                           : rightKeyColumn,
                             place.row);
    }

  return out;
}

}
