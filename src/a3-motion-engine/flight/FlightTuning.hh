/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <algorithm>
#include <cmath>

namespace a3
{

/** Every constant of the flight model, named, with its start value. The rig
 *  tunes these; the tests assert behaviours and do not depend on them.
 *
 *  Time is in beats throughout, so every speed follows the tempo without code
 *  of its own. Lengths are floor units: the room's edge is at radius 1. */
struct FlightTuning
{
  // Where each value comes from, as a tag: [lab] the MJ Lab's simulation
  // (.claude/notes/mj-lab/mj-defaults-from-lab.md, 2026-10-08), [research]
  // auditory-motion-research.md part B, [sweep] chosen by a sweep of this
  // C++ model against the audible tests (FlightGravity.cc, FlightWorld.cc),
  // [guess] neither: try it on the rig. The pro-DJ owns these defaults.

  // gravity
  float gravity = 0.02f;   // G [lab]
  float softening = 0.08f; // epsilon, floor units; 0 reads as flightSofteningFloor [lab]
  // |pull| cap, floor units per beat^2. Kept equal to steerMax [lab]: above
  // it a group holds a patrolling ship for good, below it bends and lets go.
  // The lab's 0.6 bends audibly in this model but tops out at ~1.4x speed in
  // a flyby; 1.1 is the nearest that slings >= 1.5x on most passes [sweep].
  float gravityMax = 1.1f;
  // the big path
  float orbitRadius = 0.7f;
  float orbitEccentricity = 0.2f;
  float orbitLapBars = 4.f;         // <= 0: the rabbit stands at its slot [lab + research B1]
  float orbitPrecessionBars = 32.f; // <= 0: the path does not turn [research B1: "the room drifts"]
  // steering: a PD loop on the rabbit
  float steerStiffness = 2.5f; // kSteer, per beat^2; lag ~0.06 behind the rabbit [lab]
  float steerDamping = 1.6f;   // cSteer, per beat [lab]
  float steerMax = 1.1f;       // == gravityMax, see there [lab ratio, sweep value]
  float damping = 0.05f;  // per beat [guess, plan]
  float speedMin = 0.08f; // floor units per beat [guess, plan]
  // ~1 lap per bar round the path, where the ear stops following [lab + research B1]
  float speedMax = 0.9f;
  float rimSoft = 0.9f;      // [guess, plan]
  float rimStiffness = 4.f;  // [guess, plan]
  // escort: the circle round a group is captureRadius * sqrt(mass), see
  // escortRadius below. At 0.15
  // it was a small circle near the group, heard as parked [lab + research B2].
  float captureRadius = 0.3f;
  float captureLapBars = 2.f; // <= 0: the escort goal stands still [lab, weak]
  // dead zones: a soft wall round each, like the room's rim, which the
  // gravity cap does not touch (a capped push never beat the steering). 0.25
  // is ~20 deg seen from the centre at the path [research B2]; the stiffness
  // keeps a ship at >= ~0.8 of it at full speed [guess, sweep].
  float deadZoneClearance = 0.25f;
  float deadZoneStiffness = 40.f; // per beat^2
  // ships among themselves, wander
  float separation = 0.004f; // [guess, plan]
  // the core of the push between two ships, floor units; its own, so a
  // sharper or softer well does not change how close ships come. 0 reads as
  // flightSofteningFloor [lab: the lab used the gravity's 0.08 for it]
  float separationSoftening = 0.08f;
  float wanderRadius = 0.12f; // ~10 deg a bar: "life", below a heard bend [research B2]
  // the beat. The gate: gravity pulls only during beat 1 of each bar, so the
  // bends land on the one [lab: pulseMode = gate]. Off, it breathes on every
  // beat by the two depths below, under 1 deg of wobble at any depth: nothing
  // may rely on that being heard [lab]. Under the gate pulseDepth is unused;
  // pulseDownbeatDepth is how much the floor swells the discs on the one.
  bool gravityOnlyOnTheOne = true;
  float pulseDepth = 0.3f;
  float pulseDownbeatDepth = 0.6f;
  // the planets' masses: light, half the plan's 1/2/3. With the breath on
  // every body cost attention in the model; light ones cost least [lab]. A
  // dead zone's push is the plan's; its wall above does most of the work.
  float groupMass = 0.5f;
  float crowdMass = 1.f;
  float hotspotMass = 1.5f;
  float deadZoneMass = -2.f;
  // An action on a flying ship (FPV).
  // Every rate it asks for is held under what a room can follow: a turn round
  // the listener at most 180 deg a beat, one lap per two beats (about 360 deg/s
  // at 120 BPM; direction stops being heard as a path near 900 deg/s, Feron
  // 2010), height at most half that (vertical localisation is coarser than
  // horizontal; the factor is a design choice).
  float angularCapDegreesPerBeat = 180.f;
  float verticalCapDegreesPerBeat = 90.f;
  // ~sway swings the heard height this far, the crowd's design minimum for a
  // bend that is heard (the minimum audible movement angle is about 5 deg
  // in front and grows to the sides, Grantham).
  float swayTravelDegrees = 30.f;
  // ~swell breathes the base path: in to this share of its size [guess], out
  // until its long axis stands here, inside the soft wall's reach [guess].
  float swellInnerScale = 0.4f;
  float swellOuterReach = 0.95f;
  // How long an action takes to take hold of a ship, and to let go [guess:
  // the handover's one beat].
  float motionRampBeats = 1.f;
};

/** The radius of an escort's circle round a group of `mass`: wider for a
 *  heavier one. The engine flies it and the floor draws it, from here. */
inline float
escortRadius (float mass, FlightTuning const &tuning)
{
  return tuning.captureRadius * std::sqrt (std::max (mass, 0.f));
}

/** The smallest softening the field uses. A softening of 0 would divide
 *  0 by 0 for a ship exactly on a body (or on another ship); this floor keeps
 *  that finite, and gravityMax caps what it gives. */
constexpr float flightSofteningFloor = 1e-4f;

/** `softening`^2, never below flightSofteningFloor^2. */
inline float
softeningSquared (float softening)
{
  auto const floored = std::max (softening, flightSofteningFloor);
  return floored * floored;
}

/** The gravity's softening^2, never below flightSofteningFloor^2. */
inline float
flightSofteningSquared (FlightTuning const &tuning)
{
  return softeningSquared (tuning.softening);
}

}
