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

#include <a3-motion-ui/io/PanelGrid.hh>

namespace a3
{
/** How many buttons the panel's firmware reports (InputOutputAdapterV3). */
constexpr int numPanelButtons = 44;

/** Where firmware button `button` stands on the panel: its "RC" label,
 *  row and column (InputOutputAdapterV3.hh's buttonMap). */
PanelCell panelCellOfButton (int button);

/** Where channel \`channel\`'s pad \`pad\` stands: two columns of four per
 *  channel from column 1, pads 0-3 down the left, 4-7 down the right. */
PanelCell panelCellOfPad (int channel, int pad);
}
