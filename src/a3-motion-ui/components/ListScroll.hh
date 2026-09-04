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

namespace a3
{

/** Where a long list's window sits, as a first visible row.
 *
 *  Kept apart from the selection, which is the whole point. The window used to
 *  be placed *around* the selected row — `selected - rows / 2` — so touching a
 *  row you could plainly see slid it into the middle and took it out from
 *  under your finger. A list you can point at should stay where it is when
 *  you point at it.
 */

/** The window after a drag of `steps` rows, clamped to the list. Positive is
 *  further down the list; a drag *downwards* gives a negative step, because
 *  the page follows the finger the way it does on a phone. */
int scrollBy (int top, int steps, int visibleRows, int totalRows);

/** The window that shows `selected`, moved as little as possible: unchanged
 *  when the row is already in view, and otherwise just far enough that the row
 *  appears at the edge it was reached from. */
int scrollToShow (int top, int selected, int visibleRows, int totalRows);

}
