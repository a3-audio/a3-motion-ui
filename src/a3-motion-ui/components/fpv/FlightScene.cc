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

#include "FlightScene.hh"

#include <a3-motion-ui/components/SpeakerLightScaling.hh>
#include <a3-motion-ui/components/fpv/BodyLook.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
constexpr float noDirection = 1e-6f;

Vec3
operator- (Vec3 a, Vec3 b)
{
  return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vec3
operator* (Vec3 v, float f)
{
  return { v.x * f, v.y * f, v.z * f };
}

Vec3
seen (Vec3 room, SphereCamera const &camera)
{
  return toVec3 (asSeenFrom (toPos (room), camera));
}

/** `v` with whatever runs along `normal` (a unit vector) taken out. */
Vec3
alongTheSurface (Vec3 v, Vec3 normal)
{
  return v - normal * dot (v, normal);
}

/** Which way a ship with no course of its own points: round the room's
 *  vertical, the way an azimuth turns, or along the room's x at the poles
 *  where that turn has no direction. */
Vec3
defaultNose (Vec3 normal)
{
  auto const turning = cross ({ 0.f, 0.f, 1.f }, normal);
  if (length (turning) > noDirection)
    return normalised (turning, { 1.f, 0.f, 0.f });
  return normalised (alongTheSurface ({ 1.f, 0.f, 0.f }, normal),
                     { 0.f, 1.f, 0.f });
}

float
smoothstep (float edge0, float edge1, float x)
{
  auto const t = std::clamp ((x - edge0) / (edge1 - edge0), 0.f, 1.f);
  return t * t * (3.f - 2.f * t);
}

void
put (std::array<float, 4 * maxSceneShips> &into, int i, Vec3 v, float w)
{
  auto const at = static_cast<size_t> (4 * i);
  into[at] = v.x;
  into[at + 1] = v.y;
  into[at + 2] = v.z;
  into[at + 3] = w;
}

void
put (std::array<float, 4 * maxSceneGroups> &into, int i, float a, float b,
     float c, float d)
{
  auto const at = static_cast<size_t> (4 * i);
  into[at] = a;
  into[at + 1] = b;
  into[at + 2] = c;
  into[at + 3] = d;
}

void
widen (std::array<float, 4> &bounds, Vec3 centre, float reach)
{
  auto const at = onShaderScreen (centre);
  bounds[0] = std::min (bounds[0], at[0] - reach);
  bounds[1] = std::min (bounds[1], at[1] - reach);
  bounds[2] = std::max (bounds[2], at[0] + reach);
  bounds[3] = std::max (bounds[3], at[1] + reach);
}

/** How far a ship can reach from its centre on the screen. Its hull is the
 *  longest thing on it; a little over, so the edge's soft pixels stay in. */
float
shipReach (ShipInScene const &ship)
{
  return ship.length * 0.5f * 1.05f;
}

/** A group's furthest point from its middle is the larger of its two
 *  semi-axes, whichever way it is seen. */
float
groupReach (GroupInScene const &group)
{
  return std::max (group.radius, group.halfHeight) * 1.05f;
}
}

Vec3
toVec3 (Pos const &p)
{
  return { p.x (), p.y (), p.z () };
}

Pos
toPos (Vec3 v)
{
  return Pos::fromCartesian (v.x, v.y, v.z);
}

float
dot (Vec3 a, Vec3 b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3
cross (Vec3 a, Vec3 b)
{
  return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
           a.x * b.y - a.y * b.x };
}

float
length (Vec3 v)
{
  return std::sqrt (dot (v, v));
}

Vec3
normalised (Vec3 v, Vec3 fallback)
{
  auto const l = length (v);
  if (!(l > noDirection) || !std::isfinite (l))
    return fallback;
  return v * (1.f / l);
}

// ── groups ──────────────────────────────────────────────────────────────

float
metresToSphereRadii (float metres)
{
  return metres * metrePerSphereRadius;
}

std::optional<GroupBlobSize>
groupBlobSize (float mass, FlightTuning const &tuning)
{
  if (bodyRole (mass) == BodyRole::Repel)
    return std::nullopt;

  // Straight between the three weights the floor knows, so a tuned mass
  // still lands between the sizes it lies between.
  auto const between = [mass] (float m0, float w0, float m1, float w1) {
    auto const t = std::clamp ((mass - m0) / (m1 - m0), 0.f, 1.f);
    return w0 + t * (w1 - w0);
  };
  auto const width = mass < tuning.crowdMass
                         ? between (tuning.groupMass, groupWidthM,
                                    tuning.crowdMass, crowdWidthM)
                         : between (tuning.crowdMass, crowdWidthM,
                                    tuning.hotspotMass, hotspotWidthM);
  return GroupBlobSize{ metresToSphereRadii (personHeightM),
                        metresToSphereRadii (width) };
}

GroupBlobSize
swollen (GroupBlobSize size, float pulse)
{
  auto const wider = bodyPulseScale (pulse);
  return { size.height * (1.f + (wider - 1.f) * groupSwellOfHeight),
           size.diameter * wider };
}

GroupInScene
groupInScene (Pos const &feet, GroupBlobSize size, SphereCamera const &camera)
{
  auto const halfHeight = size.height * 0.5f;
  auto const middle = Pos::fromCartesian (feet.x (), feet.y (),
                                          feet.z () + halfHeight);
  return { toVec3 (asSeenFrom (middle, camera)), size.diameter * 0.5f,
           halfHeight };
}

Pos
groupTopInRoom (Pos const &feet, GroupBlobSize size)
{
  return Pos::fromCartesian (feet.x (), feet.y (), feet.z () + size.height);
}

float
footprintRadiusOnView (Pos const &, float radius, SphereCamera const &camera)
{
  // A circle on the floor seen orthographically: one axis keeps its length,
  // the other is cut by how far the floor is turned from the eye.
  auto const up = seen ({ 0.f, 0.f, 1.f }, camera);
  return radius * (1.f + std::abs (up.z)) * 0.5f;
}

// ── ships ───────────────────────────────────────────────────────────────

void
ShipCourse::update (Pos const &at, float minimumStep)
{
  auto const here = toVec3 (at);
  if (_last && length (here - *_last) < minimumStep)
    return;
  if (_last)
    _forward = normalised (here - *_last, _forward.value_or (Vec3{}));
  _last = here;
}

void
ShipCourse::lose ()
{
  _last.reset ();
}

std::optional<Vec3>
ShipCourse::forward () const
{
  return _forward;
}

bool
onTheBackSide (Vec3 seen)
{
  return seen.z < 0.f;
}

ShipDepthCue
shipDepthCue (float seenZ)
{
  auto const back = 1.f - smoothstep (-shipBackSideBand, shipBackSideBand, seenZ);
  return { 1.f + (shipBackScale - 1.f) * back,
           1.f + (shipBackShade - 1.f) * back };
}

ShipInScene
shipInScene (Pos const &direction, std::optional<Vec3> course, float length,
             SphereCamera const &camera)
{
  auto const normal = normalised (toVec3 (direction), { 0.f, 0.f, 1.f });
  auto const nose = course ? normalised (alongTheSurface (*course, normal),
                                         defaultNose (normal))
                           : defaultNose (normal);

  ShipInScene ship;
  ship.centre
      = seen (normal * (1.f + shipHoverOfLength * length), camera);
  ship.nose = seen (nose, camera);
  ship.up = seen (normal, camera);

  // The cue goes by where the ship is on the ball, not by its hover.
  auto const cue = shipDepthCue (seen (normal, camera).z);
  ship.length = length * cue.scale;
  ship.shade = cue.shade;
  return ship;
}

// ── what hides what ─────────────────────────────────────────────────────

bool
hiddenByTheBall (Vec3 seen)
{
  auto const across = seen.x * seen.x + seen.y * seen.y;
  if (across >= 1.f)
    return false;
  // The ray leaves the ball at the far side's depth; beyond that the ball
  // lies between.
  return seen.z < -std::sqrt (1.f - across);
}

float
labelAlpha (bool hidden)
{
  return hidden ? ghostLabelAlpha : 1.f;
}

float
groupHitRadius (float footprintRadius, float fingertip)
{
  return std::max (footprintRadius, fingertip / 2.f);
}

// ── into the shader ─────────────────────────────────────────────────────

std::array<float, 2>
onShaderScreen (Vec3 seen)
{
  return { -seen.y, seen.x };
}

FlightSceneUniforms
packFlightScene (std::array<ShipInScene, maxSceneShips> const &ships,
                 int numShips,
                 std::array<GroupInScene, maxSceneGroups> const &groups,
                 int numGroups)
{
  FlightSceneUniforms packed;

  auto const shipCount = std::clamp (numShips, 0, maxSceneShips);
  for (auto i = 0; i < shipCount; ++i)
    {
      auto const &ship = ships[static_cast<size_t> (i)];
      if (!(ship.length > 0.f))
        continue;
      auto const reach = shipReach (ship);
      put (packed.shipAt, i, ship.centre, ship.length * 0.5f);
      put (packed.shipNose, i, ship.nose, ship.shade);
      put (packed.shipUp, i, ship.up, reach);
      put (packed.shipColour, i, { ship.r, ship.g, ship.b }, 0.f);
      widen (packed.bounds, ship.centre, reach);
    }

  auto const groupCount = std::clamp (numGroups, 0, maxSceneGroups);
  for (auto i = 0; i < groupCount; ++i)
    {
      auto const &group = groups[static_cast<size_t> (i)];
      if (!(group.radius > 0.f) || !(group.halfHeight > 0.f))
        continue;
      auto const reach = groupReach (group);
      put (packed.groupAt, i, group.centre.x, group.centre.y, group.centre.z,
           group.halfHeight);
      put (packed.groupShape, i, group.radius, reach, 0.f, 0.f);
      widen (packed.bounds, group.centre, reach);
    }

  return packed;
}

}
