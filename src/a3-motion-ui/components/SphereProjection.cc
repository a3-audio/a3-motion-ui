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

#include "SphereProjection.hh"

#include <a3-motion-ui/components/ControllerLayout.hh>

#include <JuceHeader.h>

#include <algorithm>
#include <cmath>

namespace a3
{

Pos
discToDirection (Pos const &flat)
{
  auto const radius = std::hypot (flat.x (), flat.y ());

  if (radius > 1.f)
    {
      // Held at the horizon: past the rim there is nothing above it to point
      // at, and the square root below would go imaginary.
      return Pos::fromCartesian (flat.x () / radius, flat.y () / radius, 0.f);
    }

  return Pos::fromCartesian (
      flat.x (), flat.y (),
      std::sqrt (std::max (0.f, 1.f - radius * radius)));
}

Pos
directionToDisc (Pos const &direction)
{
  return Pos::fromCartesian (direction.x (), direction.y (), 0.f);
}


int
discStepPieces (float x1, float y1, float x2, float y2, float maxStep,
                float maxSwing, int maxPieces)
{
  auto const dx = x2 - x1;
  auto const dy = y2 - y1;
  auto const length = std::sqrt (dx * dx + dy * dy);

  auto swing = std::abs (std::atan2 (y2, x2) - std::atan2 (y1, x1));
  if (swing > juce::MathConstants<float>::pi)
    swing = juce::MathConstants<float>::twoPi - swing;

  auto const byLength
      = maxStep > 0.f ? static_cast<int> (std::ceil (length / maxStep)) : 1;
  auto const bySwing
      = maxSwing > 0.f ? static_cast<int> (std::ceil (swing / maxSwing)) : 1;

  return juce::jlimit (1, juce::jmax (1, maxPieces),
                       juce::jmax (byLength, bySwing));
}


namespace
{
/** The room turned about its vertical axis, then tipped about the screen's
 *  horizontal one.
 *
 *  The screen's horizontal is the room's y and its vertical is the room's x
 *  -- cartesian2DHOA2JUCE puts a position at { -y, -x } -- so leaning the eye
 *  over the room is a rotation about y, which brings what was overhead down
 *  towards the top of the picture.
 */
Pos
rotated (Pos const &p, float turn, float pitch)
{
  auto const ct = std::cos (turn);
  auto const st = std::sin (turn);

  auto const x1 = p.x () * ct - p.y () * st;
  auto const y1 = p.x () * st + p.y () * ct;
  auto const z1 = p.z ();

  auto const cp = std::cos (pitch);
  auto const sp = std::sin (pitch);

  return Pos::fromCartesian (x1 * cp + z1 * sp, y1, -x1 * sp + z1 * cp);
}
}

Pos
asSeenFrom (Pos const &direction, SphereCamera const &camera)
{
  if (camera.isOverhead ())
    return direction;

  return rotated (direction, -camera.turn, camera.pitch);
}

Pos
asSeenFromInverse (Pos const &viewed, SphereCamera const &camera)
{
  if (camera.isOverhead ())
    return viewed;

  // Undone in the opposite order, or the room comes back tipped the wrong
  // way: the last thing done is the first thing undone.
  auto const unpitched = rotated (viewed, 0.f, -camera.pitch);

  return rotated (unpitched, camera.turn, 0.f);
}

Pos
slerpDirection (Pos const &from, Pos const &to, float t)
{
  auto const dot = std::clamp (from.x () * to.x () + from.y () * to.y ()
                                   + from.z () * to.z (),
                               -1.f, 1.f);
  auto const angle = std::acos (dot);
  auto const sine = std::sin (angle);

  // Nothing between them, or nothing that can be divided by: two directions
  // this close have no arc worth walking, and one is as good as the other.
  if (sine < 1e-5f)
    return to;

  auto const a = std::sin ((1.f - t) * angle) / sine;
  auto const b = std::sin (t * angle) / sine;

  return Pos::fromCartesian (a * from.x () + b * to.x (),
                             a * from.y () + b * to.y (),
                             a * from.z () + b * to.z ());
}

juce::Rectangle<int>
cameraBallBounds (juce::Rectangle<int> view)
{
  if (view.isEmpty ())
    return {};

  auto const side = juce::jmax (
      fingertipSize,
      juce::jmin (view.getWidth (), view.getHeight ()) / 6);

  if (side > view.getWidth () || side > view.getHeight ())
    return {};

  auto const margin = juce::jmax (4, side / 4);

  return juce::Rectangle<int> (view.getRight () - side - margin,
                               view.getY () + margin, side, side);
}

SphereCamera
cameraFromBallDrag (SphereCamera atGrab, juce::Point<float> moved,
                    juce::Rectangle<int> ball)
{
  auto const across = static_cast<float> (juce::jmax (1, ball.getWidth ()));
  auto const down = static_cast<float> (juce::jmax (1, ball.getHeight ()));

  SphereCamera moving;

  // Down leans the eye over the room and stops at the horizon: past a right
  // angle it would be looking up at the floor from underneath, which is a view
  // of the room nobody is standing in.
  moving.pitch = juce::jlimit (
      0.f, juce::MathConstants<float>::halfPi,
      atGrab.pitch + moved.y / down * juce::MathConstants<float>::halfPi);

  // And across walks it round, which has no end to stop at.
  moving.turn
      = atGrab.turn + moved.x / across * juce::MathConstants<float>::twoPi;

  return moving;
}

}
