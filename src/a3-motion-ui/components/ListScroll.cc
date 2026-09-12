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

#include "ListScroll.hh"

#include <algorithm>

namespace a3
{

namespace
{
int
clampTop (int top, int visibleRows, int totalRows)
{
  // A list shorter than its window does not scroll: there is nothing above
  // the first row to bring into view.
  auto const last = std::max (0, totalRows - visibleRows);

  return std::clamp (top, 0, last);
}
}

int
scrollBy (int top, int steps, int visibleRows, int totalRows)
{
  return clampTop (top + steps, visibleRows, totalRows);
}

int
scrollToShow (int top, int selected, int visibleRows, int totalRows)
{
  auto const clamped = clampTop (top, visibleRows, totalRows);

  if (selected < clamped)
    return clampTop (selected, visibleRows, totalRows);

  if (selected >= clamped + visibleRows)
    return clampTop (selected - visibleRows + 1, visibleRows, totalRows);

  // Already in view: left exactly where it was. Moving it here is what took
  // a row out from under the finger that had just touched it.
  return clamped;
}

}
