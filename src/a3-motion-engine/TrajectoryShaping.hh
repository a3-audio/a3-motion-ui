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

#include <a3-motion-engine/util/Types.hh>

namespace a3
{

class Pattern;

/** Everything done to a recorded 2D position before it is projected onto the
 *  sphere: the turn, and the two squeezes.
 *
 *  One bundle and one function rather than a call per transform, because the
 *  engine does this once and the renderer does it four times over -- once per
 *  point of every drawn line -- and the drawn line has to land exactly where
 *  the blob runs. Five places each composing their own transforms is five
 *  chances for the picture and the sound to disagree; this is the same
 *  reasoning that put the two elevation sweeps into sweptElevation().
 */
struct PlaneShaping
{
  /** The standing rotate and the running spin phase, summed, in revolutions.
   *  Two ways of asking for one rotation -- see Pattern::getRotate(). */
  float turns = 0.f;
  /** Bipolar, middle at zero: how far the figure is squeezed or stretched
   *  along the front-back axis. The screen mirrors the coordinates, so this
   *  is what a viewer reads as the vertical. */
  float squeezeX = 0.f;
  /** The same along the left-right axis -- the screen's horizontal. */
  float squeezeY = 0.f;
};

/** What one axis is multiplied by, given its pot.
 *
 *  A power of two, so the middle of the travel is unity and a squeeze undoes
 *  the stretch the same distance the other way. Clamped to the ends of the
 *  travel: past them the take would pile up on the far pole, and nothing --
 *  a script, a file, an encoder -- should be able to send it there. */
float squeezeFactor (float amount);

/** How far `pattern` is turned right now, in revolutions [0, 1): the standing
 *  angle it was left at, plus however far its spin has carried it.
 *
 *  The phase counts only while the spin is running. The engine advances it
 *  and nothing rewinds it, so a spin turned off stands still at whatever
 *  angle it stopped at -- and letting that keep turning the shape left `rot`
 *  saying one thing and the trajectory doing another, with no way back but a
 *  double tap. lfoSweep() has always had this rule for the other two sweeps;
 *  the spin was the one that added its phase whatever its step said.
 *
 *  One function because three readers have to agree: the engine before it
 *  projects, the renderer before it draws, and the arc on the rot knob that
 *  says where the spin is holding the shape. An arc drawn from a different
 *  sum than the one the blob runs on is an arc that lies. */
float turnsOf (Pattern const &pattern);

/** How `pattern` is shaping its take right now. */
PlaneShaping shapingOf (Pattern const &pattern);

/** Squeeze, then turn.
 *
 *  In that order because the ellipse belongs to the figure: squeezing a
 *  circular take and spinning it gives a turning ellipse, which is what
 *  "I flattened my orbit" means. Turning first would leave the ellipse
 *  standing still in the room, and then the spin would appear to do nothing
 *  at all to a circle -- the commonest take there is. */
Pos shapedPosition (Pos const &recorded, PlaneShaping const &shaping);

}
