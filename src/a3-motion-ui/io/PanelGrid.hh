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

#include <vector>

namespace a3
{
/** The panel as a grid (InputOutputAdapterV3.hh): six rows, ten columns.
 *  The two end columns (0 and 9) hold a key in every row; the eight between
 *  them hold the pads, in rows 2-5 -- rows 0-1 over them carry the pots.
 *  44 keys. The PADS page and the keyboard both stand on it, so a key on the
 *  screen is where its key is on the panel. */
constexpr int panelRows = 6;
constexpr int panelColumns = 10;
constexpr int firstPanelPadRow = 2;

/** A key's place on the panel, row 0 at the top, column 0 at the left. */
struct PanelCell
{
  int row = 0;
  int col = 0;

  bool operator== (PanelCell const &other) const
  {
    return row == other.row && col == other.col;
  }
};

/** Whether the panel has a key there. */
bool isPanelCell (PanelCell cell);

/** All 44, row by row, left to right. */
std::vector<PanelCell> const &panelCells ();
}
