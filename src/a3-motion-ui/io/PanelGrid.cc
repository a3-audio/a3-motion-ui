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

#include "PanelGrid.hh"

namespace a3
{

bool
isPanelCell (PanelCell cell)
{
  if (cell.row < 0 || cell.row >= panelRows || cell.col < 0
      || cell.col >= panelColumns)
    return false;

  auto const endColumn = cell.col == 0 || cell.col == panelColumns - 1;
  return endColumn || cell.row >= firstPanelPadRow;
}

std::vector<PanelCell> const &
panelCells ()
{
  static std::vector<PanelCell> const cells = [] {
    std::vector<PanelCell> out;
    for (int row = 0; row < panelRows; ++row)
      for (int col = 0; col < panelColumns; ++col)
        if (isPanelCell ({ row, col }))
          out.push_back ({ row, col });
    return out;
  }();
  return cells;
}

}
