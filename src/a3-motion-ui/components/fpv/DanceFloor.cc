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

#include "DanceFloor.hh"

#include <a3-motion-ui/Helpers.hh>

#include <cmath>

namespace a3
{

namespace
{
/** Below the shader's own cut-off the floor is seen edge on: a ray along it
 *  meets it nowhere, or everywhere. */
constexpr float edgeOn = 0.001f;

bool
isFinite (juce::Point<float> p)
{
  return std::isfinite (p.x) && std::isfinite (p.y);
}
}

Pos
floorPointInRoom (Vec2 at, FloorSurface surface, FloorView const &view)
{
  auto const onSphere = view.heightMap.mapTo3D (
      Pos::fromCartesian (at.x, at.y, 0.f), view.elevation);
  if (!onSphere.isValid () || surface == FloorSurface::Sphere)
    return onSphere;
  return Pos::fromCartesian (onSphere.x (), onSphere.y (), view.floorZ);
}

std::optional<juce::Point<float> >
floorPointOnView (Vec2 at, FloorSurface surface, FloorView const &view)
{
  auto const room = floorPointInRoom (at, surface, view);
  if (!room.isValid ())
    return std::nullopt;
  auto const onView = cartesian2DHOA2JUCE (asSeenFrom (room, view.camera));
  if (!isFinite (onView))
    return std::nullopt;
  return onView;
}

std::optional<Pos>
danceFloorUnder (juce::Point<float> onView, FloorView const &view)
{
  // Orthographic: every ray runs down the eye's own axis, so the ray is the
  // view point lifted to the eye and that axis, both taken into the room.
  auto const seen = cartesian2DJUCE2HOA (onView);
  auto const from = asSeenFromInverse (
      Pos::fromCartesian (seen.x (), seen.y (), danceFloorEyeHeight),
      view.camera);
  auto const along
      = asSeenFromInverse (Pos::fromCartesian (0.f, 0.f, -1.f), view.camera);

  if (std::abs (along.z ()) < edgeOn)
    return std::nullopt;
  auto const t = (view.floorZ - from.z ()) / along.z ();
  if (!std::isfinite (t) || t < 0.f)
    return std::nullopt;

  return Pos::fromCartesian (from.x () + t * along.x (),
                             from.y () + t * along.y (), view.floorZ);
}

Vec2
floorPointOfGroupAt (Pos const &room, FloorView const &view)
{
  // A group stands below its sphere point, and the floor's points are on the
  // upper half of the sphere: lifting the horizontal part straight up is the
  // one sphere point it can have come from.
  auto const lifted
      = discToDirection (Pos::fromCartesian (room.x (), room.y (), 0.f));
  auto const flat = view.heightMap.mapTo2D (lifted, view.elevation);
  return { flat.x (), flat.y () };
}

std::optional<Vec2>
floorPointUnder (juce::Point<float> onView, FloorView const &view)
{
  auto const room = danceFloorUnder (onView, view);
  if (!room)
    return std::nullopt;
  return floorPointOfGroupAt (*room, view);
}

}
