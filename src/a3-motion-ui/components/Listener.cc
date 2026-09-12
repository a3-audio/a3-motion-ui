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

#include "Listener.hh"

#include <a3-motion-ui/Helpers.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
/** One lump of the body: an ellipsoid, in the room's own axes -- x towards the
 *  front of the room, y to the left, z up -- with the feet at nought. */
struct Lump
{
  float front, side, up; ///< half-widths
  float x, y, z;         ///< where its middle sits
};

/** Head, shoulders, chest, and a nose so that a head seen from straight above
 *  still says which way it is facing. Proportions of a standing person, taken
 *  as fractions of their height. */
/** Four groups, each hulled on its own: a head, a torso, and an arm either
 *  side.
 *
 *  One hull over the whole body bridges the neck and the arms and comes out as
 *  a bullet; one hull per lump shows every lump's own outline and comes out as
 *  a stack of eggs. Grouping by limb is what a person looks like: the arms
 *  give the shoulders somewhere to go and stop the torso reading as a cone,
 *  and from straight above they are most of what says this is a person at all.
 *
 *  Proportions are the ordinary ones -- a head about a seventh of the height,
 *  shoulders about a quarter of it across -- taken as fractions of the
 *  standing height, with the feet at nought. */
constexpr Lump head[]{
  { 0.044f, 0.046f, 0.055f, 0.f, 0.f, 0.930f },    // skull
  { 0.048f, 0.030f, 0.036f, 0.020f, 0.f, 0.918f }, // face
  { 0.020f, 0.018f, 0.018f, 0.070f, 0.f, 0.922f }  // nose
};

constexpr Lump torso[]{
  { 0.034f, 0.034f, 0.038f, 0.f, 0.f, 0.868f },  // neck
  { 0.050f, 0.098f, 0.055f, 0.f, 0.f, 0.815f },  // shoulders
  { 0.052f, 0.086f, 0.105f, 0.f, 0.f, 0.700f },  // chest
  { 0.044f, 0.068f, 0.085f, 0.f, 0.f, 0.545f },  // waist
  { 0.046f, 0.074f, 0.060f, 0.f, 0.f, 0.470f },  // hips
  { 0.040f, 0.070f, 0.150f, 0.f, 0.f, 0.330f },  // thighs
  { 0.032f, 0.062f, 0.170f, 0.f, 0.f, 0.100f }   // shins
};

constexpr Lump leftArm[]{
  { 0.032f, 0.032f, 0.036f, 0.f, 0.108f, 0.815f },  // shoulder
  { 0.028f, 0.028f, 0.090f, 0.f, 0.112f, 0.700f },  // upper
  { 0.024f, 0.024f, 0.090f, 0.010f, 0.108f, 0.545f } // fore
};

constexpr Lump rightArm[]{
  { 0.032f, 0.032f, 0.036f, 0.f, -0.108f, 0.815f },
  { 0.028f, 0.028f, 0.090f, 0.f, -0.112f, 0.700f },
  { 0.024f, 0.024f, 0.090f, 0.010f, -0.108f, 0.545f }
};

float
cross (juce::Point<float> const &o, juce::Point<float> const &a,
       juce::Point<float> const &b)
{
  return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}
}

std::vector<juce::Point<float> >
outlineOf (std::vector<juce::Point<float> > points)
{
  if (points.size () < 3)
    return points;

  std::sort (points.begin (), points.end (),
             [] (auto const &a, auto const &b) {
               return a.x != b.x ? a.x < b.x : a.y < b.y;
             });

  // Monotone chain: the lower hull, then the upper, each dropping any point
  // the two either side of it can be joined straight past.
  std::vector<juce::Point<float> > hull;
  hull.reserve (points.size () * 2);

  for (auto const &p : points)
    {
      while (hull.size () >= 2
             && cross (hull[hull.size () - 2], hull.back (), p) <= 0.f)
        hull.pop_back ();
      hull.push_back (p);
    }

  auto const lower = hull.size () + 1;
  for (auto it = points.rbegin () + 1; it != points.rend (); ++it)
    {
      while (hull.size () >= lower
             && cross (hull[hull.size () - 2], hull.back (), *it) <= 0.f)
        hull.pop_back ();
      hull.push_back (*it);
    }

  hull.pop_back (); // the first point, come round again
  return hull;
}

juce::Path
listenerSilhouette (SphereCamera camera, float height)
{
  juce::Path path;
  if (height <= 0.f)
    return path;

  // Standing on the floor of the room and centred on it: the middle of the
  // picture is the middle of the room, which is where they are.
  constexpr float standing = 0.5f;

  auto const hull = [&] (Lump const *lumps, size_t count) {
    std::vector<juce::Point<float> > drawn;
    drawn.reserve (count * 13 * 16);

    // The surface, coarsely. The outline of a coarse ellipsoid and the outline
    // of a fine one differ by less than the line it is drawn with.
    for (size_t i = 0; i < count; ++i)
      {
        auto const &lump = lumps[i];

        for (int ring = 0; ring <= 12; ++ring)
          {
            auto const theta = pi<float> () * static_cast<float> (ring) / 12.f;

            for (int step = 0; step < 16; ++step)
              {
                auto const phi
                    = 2.f * pi<float> () * static_cast<float> (step) / 16.f;

                auto const at = Pos::fromCartesian (
                    lump.x + lump.front * std::sin (theta) * std::cos (phi),
                    lump.y + lump.side * std::sin (theta) * std::sin (phi),
                    lump.z - standing + lump.up * std::cos (theta));

                auto const on = cartesian2DHOA2JUCE (asSeenFrom (at, camera));
                drawn.push_back ({ on.x * height, on.y * height });
              }
          }
      }

    auto const outline = outlineOf (std::move (drawn));
    if (outline.size () < 3)
      return;

    path.startNewSubPath (outline.front ());
    for (size_t i = 1; i < outline.size (); ++i)
      path.lineTo (outline[i]);
    path.closeSubPath ();
  };

  hull (torso, std::size (torso));
  hull (leftArm, std::size (leftArm));
  hull (rightArm, std::size (rightArm));
  hull (head, std::size (head));

  // Non-zero, so the lumps read as one body rather than as a pile of outlines
  // with the overlaps punched out of it.
  path.setUsingNonZeroWinding (true);

  return path;
}

}
