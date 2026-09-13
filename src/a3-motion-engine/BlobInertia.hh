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

#include <cmath>

namespace a3
{

/** The blob hangs off its trajectory on a spring.
 *
 *  Everything else the engine lays over a recorded position -- the spin, the
 *  two squeezes, the elevation sweeps, the accent -- is *stateless*: phase and
 *  parameter in, answer out, which is exactly why the renderer can compute the
 *  same thing independently and the drawn line and the running blob can never
 *  disagree. A mass on a spring has a velocity, and a velocity cannot be
 *  computed, only carried forward. So this is the first thing on the sphere
 *  the renderer has to *read* rather than re-derive, and that is why
 *  Channel keeps the target and the blob under one seqlock.
 *
 *  It runs in the **shaped 2D disc** -- after the spin and the squeezes, before
 *  the elevation mapping. Two things fall out of that for free: the blob goes
 *  through mapTo3D like any other point, so it is always on the sphere and
 *  always inside the clip's own elevation bounds; and because the spin happens
 *  first, the spin throws the blob. That last one is the centrifugal force
 *  this was asked for.
 *
 *  **It counts in bars, not in seconds.** The natural frequency is cycles per
 *  bar, like TempoLfo and everything else that moves here, so a take feels the
 *  same at 90 and at 140 BPM. That is not what a real pendulum does -- a
 *  pendulum does not know how fast the music is -- and it is the right trade
 *  anyway: a take that does two different things at two tempi is one you
 *  cannot trust in a set. It also costs nothing, because performPlayback()
 *  already runs on ticks and ticks-per-bar is constant.
 */

/** Where the blob is and how fast it is going, in the shaped disc. */
struct InertialState
{
  Pos position;
  Pos velocity;
  /** Whether it has ever been given a position. A cold spring is laid down on
   *  the target rather than flying at it from the origin. */
  bool primed = false;
};

/** How far a target may move in one tick and still count as movement.
 *
 *  Past it, it is a cut: a clip looping back to its start, a Random end
 *  action, stop then play, a finger letting go. Under a spring a teleporting
 *  target would fling the blob across the room -- and that travel would go out
 *  on OSC as a movement nobody played.
 *
 *  Second place in this codebase with that rule; BlobTrail has the first. At
 *  the third it should become a module of its own, and not before. */
constexpr float inertiaCutDistance = 0.35f;

/** The softest and the stiffest the spring is allowed to be, in cycles per
 *  bar.
 *
 *  The stiff end is bounded by the integrator rather than by taste: a spring
 *  stepped once per tick comes apart once omega passes about a radian a tick,
 *  and at the coarsest tick rate the clock runs at, this leaves a wide margin.
 *
 *  The soft end is bounded by the room. At half this the blob trailed the
 *  figure by most of a sphere on a fast take -- physically what a spring that
 *  loose does, and musically a sound somewhere else entirely. Most of the
 *  knob's travel was then in the part nobody would use. */
constexpr float inertiaStiffestCyclesPerBar = 14.f;
constexpr float inertiaSoftestCyclesPerBar = 1.2f;

/** How much of the ringing is taken out. Under one, so it swings past and
 *  comes back -- that overshoot is the whole difference between a spring and
 *  a smoothing filter. */
constexpr float inertiaDamping = 0.32f;

/** One tick on.
 *
 *  `elasticity` is 0..1 and 0 is rigid -- and rigid means the target itself,
 *  to the bit. Not a very stiff spring: every take and every set in existence
 *  has no value for this and gets the zero, and they have to sound exactly as
 *  they did before this existed. It is the promise slewTowards() and
 *  envelopeOver() already make. */
inline InertialState
stepInertia (InertialState const &state, Pos target, float elasticity,
             float ticksPerBar)
{
  if (!(elasticity > 0.f) || !(ticksPerBar > 0.f))
    return { target, Pos::fromCartesian (0.f, 0.f, 0.f), true };

  auto const jumped
      = state.primed
        && std::hypot (target.x () - state.position.x (),
                       target.y () - state.position.y ())
               > inertiaCutDistance;

  if (!state.primed || jumped)
    return { target, Pos::fromCartesian (0.f, 0.f, 0.f), true };

  auto const cycles = inertiaStiffestCyclesPerBar
                      + (inertiaSoftestCyclesPerBar
                         - inertiaStiffestCyclesPerBar)
                            * elasticity;
  auto const omega = 2.f * 3.14159265358979f * cycles / ticksPerBar;

  // Semi-implicit Euler: the velocity is stepped first and the position with
  // the velocity it has just been given. It costs nothing over the plain form
  // and is what keeps a spring this stiff from walking away.
  auto const pullX = omega * omega * (target.x () - state.position.x ());
  auto const pullY = omega * omega * (target.y () - state.position.y ());

  auto const dragX = 2.f * inertiaDamping * omega * state.velocity.x ();
  auto const dragY = 2.f * inertiaDamping * omega * state.velocity.y ();

  auto const vx = state.velocity.x () + pullX - dragX;
  auto const vy = state.velocity.y () + pullY - dragY;

  return { Pos::fromCartesian (state.position.x () + vx,
                               state.position.y () + vy, 0.f),
           Pos::fromCartesian (vx, vy, 0.f), true };
}

}
