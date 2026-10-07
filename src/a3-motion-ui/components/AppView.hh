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

/** Which screen the device shows: FULL is the sphere with the clip settings
 *  and the menu, FPV the pilots' view (spec: .claude/notes/fpv-motion-ui.md
 *  in the workspace). */
enum class AppView
{
  Full,
  Fpv,
};

constexpr char const *
appViewName (AppView view)
{
  switch (view)
    {
    case AppView::Full:
      return "FULL";
    case AppView::Fpv:
      return "FPV";
    }
  return "FULL";
}

constexpr AppView
toggled (AppView view)
{
  return view == AppView::Fpv ? AppView::Full : AppView::Fpv;
}

} // namespace a3
