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

#include "PanelButtonCells.hh"

namespace a3
{

namespace
{
/** The "RC" label of each firmware button, in firmware order -- the labels
 *  InputOutputAdapterV3.hh's buttonMap is annotated with. */
constexpr int buttonLabels[numPanelButtons] = {
  40, 30, 20, 50,                  // col0, rows 4 3 2 5
  21, 51, 31, 41, 42, 32, 22, 52,  // ch0
  23, 53, 33, 43, 44, 34, 24, 54,  // ch1
  25, 55, 35, 45, 46, 36, 26, 56,  // ch2
  27, 57, 37, 47, 48, 38, 28, 58,  // ch3
  29, 59, 39, 49,                  // col9, rows 2 5 3 4
  0,  10, 9,  19,                  // rows 0-1, col0 then col9
};
}

PanelCell
panelCellOfPad (int channel, int pad)
{
  constexpr int padsPerColumn = 4;
  return { firstPanelPadRow + pad % padsPerColumn,
           1 + 2 * channel + pad / padsPerColumn };
}

PanelCell
panelCellOfButton (int button)
{
  if (button < 0 || button >= numPanelButtons)
    return { -1, -1 };

  auto const label = buttonLabels[button];
  return { label / 10, label % 10 };
}

}
