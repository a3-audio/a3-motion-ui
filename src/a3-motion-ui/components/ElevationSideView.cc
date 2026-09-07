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

#include "ElevationSideView.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

SphereCamera
elevationSideCamera (SphereCamera sphere)
{
  // A quarter turn from the sphere above, and counted the other way round, so
  // that overhead up there is a side view down here with the ceiling at the
  // top of it -- and a side view up there is an overhead down here rather than
  // a view from under the floor.
  sphere.pitch = pi<float> () / 2.f - sphere.pitch;
  return sphere;
}

ElevationSidePoint
elevationSideView (Pos const &direction, SphereCamera sphere)
{
  auto const seen = asSeenFrom (direction, elevationSideCamera (sphere));

  // The same screen convention the sphere above uses: the room's +x up the
  // picture and its +y to the left. Turn your head from one to the other and
  // the room has not moved.
  ElevationSidePoint point;
  point.across = -seen.y ();
  point.down = -seen.x ();
  point.behind = seen.z () < 0.f;
  point.startsStroke = false;

  return point;
}

Pos
elevationSideDirection (float across, float down, SphereCamera sphere)
{
  // Back through the same convention, on the near side -- the half a finger
  // can see.
  auto x = -down;
  auto y = -across;

  // A finger past the rim is held at the rim rather than dropped: sliding off
  // the edge should keep setting a value, not stop dead.
  auto const r = std::sqrt (x * x + y * y);
  if (r > 1.f)
    {
      x /= r;
      y /= r;
    }

  auto const z = std::sqrt (std::max (0.f, 1.f - x * x - y * y));

  return asSeenFromInverse (Pos::fromCartesian (x, y, z),
                            elevationSideCamera (sphere));
}

std::vector<ElevationSidePoint>
elevationSideView (std::vector<Pos> const &directions, std::size_t maxPoints,
                   SphereCamera sphere)
{
  std::vector<ElevationSidePoint> drawn;

  if (directions.empty () || maxPoints == 0)
    return drawn;

  // Every nth, chosen so the last tick is still reached: taking the first
  // maxPoints of a long take would draw its first twentieth and call it the
  // figure.
  auto const stride
      = std::max<std::size_t> (1, (directions.size () + maxPoints - 1)
                                      / maxPoints);

  drawn.reserve (directions.size () / stride + 1);

  // Kept alongside the drawn points: how far the sound moved in the room to
  // get to each of them. The picture cannot answer that -- see startsStroke.
  std::vector<float> steps;
  steps.reserve (drawn.capacity ());

  Pos previous;
  bool havePrevious = false;

  for (std::size_t i = 0; i < directions.size (); i += stride)
    {
      if (!directions[i].isValid ())
        {
          // A hole in the take. What follows it is a new stroke, whatever the
          // distance says.
          havePrevious = false;
          continue;
        }

      drawn.push_back (elevationSideView (directions[i], sphere));
      auto const step = directions[i] - previous;
      steps.push_back (havePrevious
                           ? std::sqrt (step.x () * step.x ()
                                        + step.y () * step.y ()
                                        + step.z () * step.z ())
                           : -1.f);

      previous = directions[i];
      havePrevious = true;
    }

  // The typical step of this figure, taken as the middle one so that the seam
  // itself does not drag the measure it is judged against. A seam is many
  // times the typical step; a fast stretch of an ordinary figure is not.
  auto ordered = steps;
  ordered.erase (std::remove_if (ordered.begin (), ordered.end (),
                                 [] (float step) { return step < 0.f; }),
                 ordered.end ());

  auto typical = 0.f;
  if (!ordered.empty ())
    {
      auto const middle = ordered.begin () + ordered.size () / 2;
      std::nth_element (ordered.begin (), middle, ordered.end ());
      typical = *middle;
    }

  // The floor keeps a figure that barely moves -- a near-still clip, or one
  // sampled so finely its steps are noise -- from having every wobble read as
  // a seam. A tear is always a fair fraction of the room across.
  auto const seam = std::max (6.f * typical, 0.25f);

  for (std::size_t i = 0; i < drawn.size (); ++i)
    drawn[i].startsStroke = steps[i] < 0.f || steps[i] > seam;

  return drawn;
}

}
