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

/** What lies over the sphere, opened from the global strip's keys.
 *
 *  MAINMIX and FILES are not bar pages: each covers the sphere, and the bar
 *  stays on the page it was on underneath. FILES was a page of the clip area
 *  until 2026-09-27 -- a list of seventy names in a strip a few rows tall --
 *  and moved over the sphere, where the big mixer already stood, because the
 *  sphere is the one area on screen with room for a list.
 *
 *  One value rather than a flag per overlay, because the two share the same
 *  rectangle: both open at once would be one hidden under the other, still
 *  taking the keys and the Back press meant for the one in front. */
enum class SphereOverlay
{
  None,
  MainMix,
  Files,
};

}
