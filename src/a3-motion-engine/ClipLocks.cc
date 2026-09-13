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

#include "ClipLocks.hh"

namespace a3
{

ClipSettings
heldOver (ClipSettings const &current, ClipSettings incoming,
          ClipLocks const &locks)
{
  // Grouped the way the bar is, not the way the struct is: a lock is a thing
  // pressed above a column of knobs, so what it holds has to be that column.
  // reach sits in Motion beside the swell that sweeps it, even though what it
  // does is elevation -- and it is held with Motion, because that is where
  // the hand that pressed the lock was looking.
  if (locks.shape)
    {
      incoming.speedLog2 = current.speedLog2;
    }

  if (locks.elevation)
    {
      incoming.clipTop = current.clipTop;
      incoming.clipBottom = current.clipBottom;
      incoming.elevationBase = current.elevationBase;
      incoming.elevationLfo = current.elevationLfo;
      incoming.flat = current.flat;
      incoming.flatElevation = current.flatElevation;
      incoming.mirrorSouth = current.mirrorSouth;
    }

  if (locks.motion)
    {
      incoming.rotate = current.rotate;
      incoming.squeezeX = current.squeezeX;
      incoming.squeezeY = current.squeezeY;
      incoming.squeezeXLfo = current.squeezeXLfo;
      incoming.squeezeYLfo = current.squeezeYLfo;
      incoming.reach = current.reach;
      incoming.reachLfo = current.reachLfo;
      incoming.spin = current.spin;
      incoming.fadeReach = current.fadeReach;
      incoming.bridgeBias = current.bridgeBias;
      incoming.direction = current.direction;
      incoming.endAction = current.endAction;
    }

  return incoming;
}

}
