/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

class Pattern;

/** What a take starts from on a slot that already holds a clip: that clip's
 *  path, at the take's length, and its lanes. The take writes over them by its
 *  rec mode, so TOUCH changes only what is touched -- which is what makes
 *  turning pots over an old figure an overdub rather than a new, empty take.
 *
 *  The path counts as written, or closing the take's seams would fill it
 *  over. Settings are not carried here: the take takes them when play is
 *  pressed, so what is dialled before the downbeat is kept. */
void seedTake (Pattern &take, Pattern const &from);

/** A take's band: the whole sphere, top at the north pole, nothing clipped
 *  and no sweep moving it (maintainer, 2026-10-08).
 *
 *  A clip's band grows south from its base, so a clip based at ear height can
 *  only play the lower half -- and with the camera looking from above, every
 *  finger on a take over it was held on the equator. A take records over the
 *  whole sphere instead and keeps that band, so what is heard is still what
 *  plays back (#66). Only these settings leave the clip's; the elevation
 *  knobs' lanes go with them, since the path is moved into the new band. */
void openToTheWholeSphere (Pattern &take);

}
