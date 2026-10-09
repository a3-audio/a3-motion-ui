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

#include <a3-motion-engine/elevation/HeightMap.hh>
#include <a3-motion-engine/flight/FlightVec.hh>
#include <a3-motion-ui/components/SphereProjection.hh>

#include <juce_graphics/juce_graphics.h>

#include <optional>

namespace a3
{

/** Where FPV's floor points are drawn and touched.
 *
 *  A floor point (x, y, the ships' disc ending at radius 1) is one value with
 *  two places on the screen. The ships fly on the sphere, so what marks their
 *  path stays there. The guests stand on the dance floor the shader draws
 *  under the ball: straight below the sphere point their floor point maps
 *  to, so a ship circling a group on the ball is seen right above it. Past
 *  the disc, out to floorReach, the guests stand between the speakers, where
 *  no ship flies. */
enum class FloorSurface
{
  Sphere,
  DanceFloor,
};

/** What the floor needs from the view. `floorZ` is the dance floor's height
 *  in the room, in sphere radii (speakerFloorZ). */
struct FloorView
{
  HeightMap const &heightMap;
  SphereCamera camera;
  float floorZ = 0.f;
  ElevationParams elevation{};
};

/** How far up the eye's own axis the shader starts its floor rays, in sphere
 *  radii. The view is orthographic, so this is not a distance anyone sees;
 *  it is where the floor stops being drawn, and a floor that is not drawn
 *  must not take a finger either. Has to match `ro` in SphereShader's
 *  danceFloor(). */
constexpr float danceFloorEyeHeight = 4.f;

/** Where a floor point stands in the room on `surface`. Invalid where the
 *  height map has no point for it. On the dance floor a point past the rim
 *  walks straight out from where the rim stands, and at floorReach it stands
 *  at its own (x, y): the floor's edge is where the shader draws it. */
Pos floorPointInRoom (Vec2 at, FloorSurface surface, FloorView const &view);

/** A floor length at `at`, in the room on the dance floor: the mean of how
 *  far a step of `length` along the floor's x and y carries the point there.
 *  The walk from floor to room is not a scale, so a ring of a floor length
 *  round a group is measured where the group stands. */
float floorLengthInRoom (Vec2 at, float length, FloorView const &view);

/** Where a floor point lands on the view, in the sphere's normalised screen
 *  units (the sphere's radius is 1, oriented as cartesian2DHOA2JUCE puts
 *  it). Empty where the height map has no point for it. */
std::optional<juce::Point<float> >
floorPointOnView (Vec2 at, FloorSurface surface, FloorView const &view);

/** Where the ray through a point of the view meets the dance floor, in the
 *  room. `onView` is in floorPointOnView's units. Empty where the ray never
 *  meets it: looking in from the horizon the rays run parallel to the floor,
 *  and steeply leaned the floor below the picture lies behind the eye. */
std::optional<Pos> danceFloorUnder (juce::Point<float> onView,
                                    FloorView const &view);

/** The floor point whose group stands at `room` on the dance floor: the
 *  exact inverse of floorPointInRoom (..., DanceFloor, ...). Only `room`'s
 *  horizontal part is read. Inside the rim it is lifted back onto the upper
 *  half of the sphere, where the floor's points live; past it the walk out
 *  is undone, and keeps going beyond floorReach, so the caller can tell a
 *  finger past the floor's edge from one on it. */
Vec2 floorPointOfGroupAt (Pos const &room, FloorView const &view);

/** The floor point under a point of the view: danceFloorUnder, then
 *  floorPointOfGroupAt. Empty where the ray misses the floor. */
std::optional<Vec2> floorPointUnder (juce::Point<float> onView,
                                     FloorView const &view);

}
