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

#include <a3-motion-engine/flight/FlightField.hh>
#include <a3-motion-engine/flight/FlightTuning.hh>
#include <a3-motion-ui/components/SphereProjection.hh>

#include <array>
#include <optional>

namespace a3
{

/** FPV's ships and groups as things standing in the room, for the shader to
 *  raytrace beside the ball, the floor and the towers.
 *
 *  Everything here is in sphere radii, and every place it hands over is in the
 *  *seen* frame: the room turned by asSeenFrom, x and y across the glass, z
 *  towards the eye. The shader's rays fall down that z, so a seen point lands
 *  on its screen at (-y, x) and its depth along the ray is its z. */

/** A plain 3-vector. Pos is a position with spherical helpers and no scalar
 *  product; the poses below need both a length and a cross product. */
struct Vec3
{
  float x = 0.f, y = 0.f, z = 0.f;
};

Vec3 toVec3 (Pos const &p);
Pos toPos (Vec3 v);
float dot (Vec3 a, Vec3 b);
Vec3 cross (Vec3 a, Vec3 b);
float length (Vec3 v);
/** `fallback` where `v` has no direction of its own. */
Vec3 normalised (Vec3 v, Vec3 fallback);

// ── groups: people on the dance floor ───────────────────────────────────

/** One person standing, in metres: the height every group blob is drawn. */
constexpr float personHeightM = 1.75f;
/** How wide a group stands, in metres, by weight: about 3, 6 and 10 people
 *  close together. */
constexpr float groupWidthM = 1.4f;
constexpr float crowdWidthM = 2.0f;
constexpr float hotspotWidthM = 2.6f;
/** The beat swell lifts a blob by half of what it widens it: a crowd that
 *  breathes, not one that jumps. */
constexpr float groupSwellOfHeight = 0.5f;

/** Metres into sphere radii (metrePerSphereRadius). */
float metresToSphereRadii (float metres);

struct GroupBlobSize
{
  float height = 0.f;   // sphere radii
  float diameter = 0.f; // sphere radii
};

/** The blob a body is drawn as: a group's width by its weight, through G, C
 *  and H's masses in FlightTuning and straight between them, one person
 *  tall. A dead zone has none: it stays a mark on the floor. */
std::optional<GroupBlobSize> groupBlobSize (float mass,
                                            FlightTuning const &tuning);

/** The blob at drawnPulse `pulse`: wider by bodyPulseScale, taller by
 *  groupSwellOfHeight of that. */
GroupBlobSize swollen (GroupBlobSize size, float pulse);

/** A group as the shader draws it: an upright spheroid with its feet on the
 *  floor. `centre` is its middle, seen. */
struct GroupInScene
{
  Vec3 centre;
  float radius = 0.f;
  float halfHeight = 0.f;
};

/** `feet` is where the group stands, in the room (floorPointInRoom on the
 *  dance floor). */
GroupInScene groupInScene (Pos const &feet, GroupBlobSize size,
                           SphereCamera const &camera);

/** The top of the blob, in the room: where its label goes. */
Pos groupTopInRoom (Pos const &feet, GroupBlobSize size);

/** How wide a footprint of `radius` round `feet` looks, in view units (the
 *  sphere's radius is 1): the mean of its two axes as the camera sees them.
 *  Leaned over, a circle on the floor is an ellipse, and the hit area is a
 *  circle; the mean is fair to both ends. */
float footprintRadiusOnView (Pos const &feet, float radius,
                             SphereCamera const &camera);

// ── ships: craft flying on the ball ─────────────────────────────────────

/** How high a ship hovers over the ball, in ship lengths: clear of the glass
 *  everywhere along its hull, so a ship on the near side never sinks into it
 *  and one on the far side is wholly behind it. */
constexpr float shipHoverOfLength = 0.15f;
/** On the far half a ship is drawn smaller and darker, eased over this much
 *  of the seen depth either side of the ball's middle plane so it does not
 *  flick as it crosses. */
constexpr float shipBackSideBand = 0.15f;
constexpr float shipBackScale = 0.8f;
constexpr float shipBackShade = 0.5f;

/** Which way a ship points, in the room: along its last movement long enough
 *  to read, so one standing still keeps its course. In the room rather than
 *  on the glass, so turning the camera turns the ship with everything else. */
class ShipCourse
{
public:
  /** `minimumStep` in sphere radii; below it the course is kept. */
  void update (Pos const &at, float minimumStep);
  /** Forget the last point (the ship vanished); the course is kept. */
  void lose ();
  /** Unset until the ship has moved once. */
  std::optional<Vec3> forward () const;

private:
  std::optional<Vec3> _last;
  std::optional<Vec3> _forward;
};

/** Whether a seen point lies on the half of the ball away from the eye. */
bool onTheBackSide (Vec3 seen);

struct ShipDepthCue
{
  float scale = 1.f;
  float shade = 1.f;
};

/** Smaller and darker on the far half, eased across shipBackSideBand. */
ShipDepthCue shipDepthCue (float seenZ);

/** A ship as the shader draws it. `length` already carries the depth cue. */
struct ShipInScene
{
  Vec3 centre; // seen
  Vec3 nose;   // seen, unit, along the ball
  Vec3 up;     // seen, unit, away from the ball's middle
  float length = 0.f;
  float shade = 1.f;
  float r = 1.f, g = 1.f, b = 1.f;
};

/** Where a ship at `direction` on the ball flies, pointing along `course`
 *  (the room's; anything not along the ball is taken out of it, and a course
 *  that leaves nothing points it along the ball's turn instead). */
ShipInScene shipInScene (Pos const &direction, std::optional<Vec3> course,
                         float length, SphereCamera const &camera);

// ── what hides what ─────────────────────────────────────────────────────

/** Whether the ball stands between the eye and a seen point: the point lies
 *  beyond the ball's far side along its ray. Inside the ball nothing is
 *  hidden by it -- the glass is the sound field the groups stand in. */
bool hiddenByTheBall (Vec3 seen);

/** A hidden thing's label is dimmed to this, like its ghost: still there,
 *  plainly not in front. */
constexpr float ghostLabelAlpha = 0.4f;

float labelAlpha (bool hidden);

/** Where a finger finds a group: its footprint as drawn, never less than a
 *  fingertip across. Both in pixels. */
float groupHitRadius (float footprintRadius, float fingertip);

// ── into the shader ─────────────────────────────────────────────────────

constexpr int maxSceneShips = 4;
constexpr int maxSceneGroups = maxFlightBodies;

/** The uniforms the shader reads, packed four floats to an entry, the way
 *  glUniform4fv takes them. An unused entry has zero size, which the shader
 *  takes as "nothing here". */
struct FlightSceneUniforms
{
  /** centre xyz, half length */
  std::array<float, 4 * maxSceneShips> shipAt{};
  /** nose xyz, shade */
  std::array<float, 4 * maxSceneShips> shipNose{};
  /** up xyz, how far from its centre on the screen it can reach */
  std::array<float, 4 * maxSceneShips> shipUp{};
  /** r, g, b, unused */
  std::array<float, 4 * maxSceneShips> shipColour{};
  /** centre xyz, half height */
  std::array<float, 4 * maxSceneGroups> groupAt{};
  /** radius, reach on the screen, unused, unused */
  std::array<float, 4 * maxSceneGroups> groupShape{};
  /** On the shader's screen (x right, y up, the ball's radius 1): the box
   *  every ship and group lies in, min x, min y, max x, max y. Empty (min
   *  over max) with nothing in it, so a pixel outside costs one compare. */
  std::array<float, 4> bounds{ 1.f, 1.f, -1.f, -1.f };
};

/** Where a seen point lands on the shader's screen: (-y, x). */
std::array<float, 2> onShaderScreen (Vec3 seen);

FlightSceneUniforms
packFlightScene (std::array<ShipInScene, maxSceneShips> const &ships,
                 int numShips,
                 std::array<GroupInScene, maxSceneGroups> const &groups,
                 int numGroups);

}
