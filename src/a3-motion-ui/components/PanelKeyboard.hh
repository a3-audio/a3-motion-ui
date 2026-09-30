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

#include <a3-motion-ui/components/BarKeyboardLayout.hh>
#include <a3-motion-ui/io/PanelGrid.hh>

namespace a3
{
/** The keyboard is the panel (2026-09-30): each of its 44 keys is a key of
 *  the keyboard, QWERTY in the four rows that are ten wide, the corners and
 *  the bottom row for what is not a letter.
 *
 *  ```
 *          col0   1    2    3    4    5    6    7    8   col9
 *  row 0   ESC                                            BKSP
 *  row 1   123                                            ENTER
 *  row 2    q     w    e    r    t    y    u    i    o    p
 *  row 3    a     s    d    f    g    h    j    k    l    -
 *  row 4    z     x    c    v    b    n    m    ,    .    /
 *  row 5   SHIFT  <    >   SPACE SPACE SPACE SPACE '    "   HIDE
 *  ```
 *
 *  The symbols page changes rows 2-4 only. */
KeyDef panelKeyAt (KeyboardPage page, PanelCell cell);
}
