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

#include <a3-motion-engine/Pattern.hh>

namespace a3
{

/** Whether a scene's Play pad starts this clip.
 *
 *  Only a clip that stands still. A single Play pad toggles, but a scene that
 *  toggled would start half a row and stop the other half -- mid-set, a
 *  surprise nobody asked for. Running and waiting clips are left alone. */
constexpr bool
sceneStartsClip (Pattern::Status status)
{
  return status == Pattern::Status::Idle;
}

}
