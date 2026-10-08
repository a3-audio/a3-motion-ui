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

namespace a3
{

/** Every constant of the flight model, named, with its start value. The rig
 *  tunes these; the tests assert behaviours and do not depend on them.
 *
 *  Time is in beats throughout, so every speed follows the tempo without code
 *  of its own. Lengths are floor units: the room's edge is at radius 1. */
struct FlightTuning
{
  // gravity
  float gravity = 0.02f;   // G
  float softening = 0.08f; // epsilon, floor units
  float gravityMax = 1.5f; // |a| cap, floor units per beat^2
  // the big path
  float orbitRadius = 0.7f;
  float orbitEccentricity = 0.2f;
  float orbitLapBars = 4.f;
  float orbitPrecessionBars = 32.f;
  // steering
  // kSteer and cSteer: a PD loop without feed-forward of the rabbit's turn
  // lags it by about (centripetal acceleration / kSteer). The path's tightest
  // bend needs ~0.13 floor units/beat^2, so 0.6 left the ship 0.24 behind;
  // 2 keeps it within ~0.07. cSteer = 2 * 0.78 * sqrt (kSteer): just under
  // critically damped, so it settles onto the path without ringing.
  float steerStiffness = 2.f; // kSteer, per beat^2
  float steerDamping = 2.2f;  // cSteer, per beat
  float steerMax = 0.3f;
  float damping = 0.05f;  // per beat
  float speedMin = 0.08f; // floor units per beat
  float speedMax = 0.9f;
  float rimSoft = 0.9f;
  float rimStiffness = 4.f;
  // escort
  float captureRadius = 0.15f;
  float captureLapBars = 2.f;
  // ships among themselves, wander
  float separation = 0.004f;
  float wanderRadius = 0.12f;
  // the beat
  float pulseDepth = 0.3f;
  float pulseDownbeatDepth = 0.6f;
};

}
