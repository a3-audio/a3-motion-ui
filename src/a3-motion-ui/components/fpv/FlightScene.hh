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
#include <a3-motion-ui/components/fpv/BodyLook.hh>

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

// ── groups: marks on the dance floor ───────────────────────────────────

/** The radius of a body's mark, in sphere radii: the 2D disc's, bodyRadius
 *  against a blob `blobDiameter` (sphere radii) across, so a mark is the
 *  disc that was, laid on the floor. (Marked by the people in it instead --
 *  1.4 / 2.0 / 2.6 m across -- it came out about 1.4 times the disc and read
 *  as something new.) */
float discMarkRadius (float mass, float blobDiameter,
                      FlightTuning const &tuning);

/** The mark at drawnPulse `pulse`: it swells on the one as the disc did. */
float swollenMarkRadius (float radius, float pulse);

/** A body as the shader draws it: a disc lying on the dance floor, seen. */
struct FloorMark
{
  Vec3 centre;
  float radius = 0.f;
  bool deadZone = false;
};

/** `feet` is where the body stands, in the room (floorPointInRoom on the
 *  dance floor). */
FloorMark floorMarkInScene (Pos const &feet, float radius, BodyRole role,
                            SphereCamera const &camera);

/** How wide a footprint of `radius` round `feet` looks, in view units (the
 *  sphere's radius is 1): the mean of its two axes as the camera sees them.
 *  Leaned over, a circle on the floor is an ellipse, and the hit area is a
 *  circle; the mean is fair to both ends. */
float footprintRadiusOnView (Pos const &feet, float radius,
                             SphereCamera const &camera);

// ── ships: craft flying on the ball ─────────────────────────────────────

/** A ship's length, in blob diameters: as long as FULL's blob is wide and a
 *  little more, so a craft reads as a craft beside a group's mark. */
constexpr float shipLengthOfBlob = 1.6f;
/** The shortest step a ship's course is taken from, in ship lengths: below
 *  it a ship standing still would turn on its own jitter. */
constexpr float shipStepOfLength = 0.1f;

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
 *  hidden by it -- the glass is the sound field the guests stand in, and
 *  the floor under it is where most of them are. */
bool hiddenByTheBall (Vec3 seen);

/** A hidden thing's label is dimmed to this, like its ghost: still there,
 *  plainly not in front. */
constexpr float ghostLabelAlpha = 0.4f;

float labelAlpha (bool hidden);

/** Where a finger finds a body: its mark's footprint as seen, never less
 *  than a fingertip across. Both in pixels. */
float groupHitRadius (float footprintRadius, float fingertip);

// ── into the shader ─────────────────────────────────────────────────────

constexpr int maxSceneShips = 4;
constexpr int maxSceneMarks = maxFlightBodies;

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
  /** centre xyz, radius */
  std::array<float, 4 * maxSceneMarks> markAt{};
  /** 1 for a dead zone, reach on the screen, unused, unused */
  std::array<float, 4 * maxSceneMarks> markShape{};
  /** On the shader's screen (x right, y up, the ball's radius 1): the box
   *  every ship and mark lies in, min x, min y, max x, max y. Empty (min
   *  over max) with nothing in it, so a pixel outside costs one compare. */
  std::array<float, 4> bounds{ 1.f, 1.f, -1.f, -1.f };
  /** How wide a mark's outline and hatch are drawn, on the shader's screen:
   *  the theme's thin stroke, so the floor's lines are the 2D pass's. */
  float markStroke = 0.f;
};

/** Where a seen point lands on the shader's screen: (-y, x). */
std::array<float, 2> onShaderScreen (Vec3 seen);

FlightSceneUniforms
packFlightScene (std::array<ShipInScene, maxSceneShips> const &ships,
                 int numShips,
                 std::array<FloorMark, maxSceneMarks> const &marks,
                 int numMarks);

}
