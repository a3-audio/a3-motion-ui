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

#include "DoubleTap.hh"

namespace a3
{

bool
countsAsTap (TapTouch const &touch, DoubleTapMovement movement)
{
  if (!touch.moved)
    return true;

  return movement == DoubleTapMovement::MayMove
         && touch.upMs - touch.downMs < shortTouchMs;
}

bool
isDoubleTap (std::optional<TapTouch> const &previous, TapTouch const &current,
             DoubleTapMovement movement)
{
  if (!previous.has_value ())
    return false;
  if (!countsAsTap (*previous, movement) || !countsAsTap (current, movement))
    return false;

  auto const quick = current.upMs - previous->upMs < doubleTapMs;
  auto const near = current.at.getDistanceFrom (previous->at) < doubleTapSlopPx;
  return quick && near;
}

}
