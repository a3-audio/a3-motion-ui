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

#pragma once

#include <a3-motion-engine/util/Geometry.hh>
#include <a3-motion-engine/util/Types.hh>

#include <vector>

namespace a3
{

/** The projection the sphere display actually uses: an orthographic view of
 *  the upper hemisphere from above. The centre of the disc is straight up, the
 *  rim is the horizon.
 *
 *  This is what the drawing does — a blob is drawn by dropping z — so it is
 *  also what a drag has to invert. The height map's 2D space is a different
 *  thing: it parametrises a pattern's own coordinate range through its `reach`,
 *  where radius 1 sits 45 degrees off the zenith rather than on the horizon.
 *  Reading a finger's radius as a pattern radius is what left the blob short of
 *  the finger.
 *
 *  Takes and returns HOA cartesian positions; the screen conversion stays with
 *  `cartesian2DHOA2JUCE` in Helpers.hh. */

/** Direction on the unit sphere a point of the disc stands for. Only the x/y
 *  of `flat` are read. Beyond the rim there is no direction left above the
 *  horizon, so it is held there rather than returning a NaN. */
Pos discToDirection (Pos const &flat);

/** Where a direction lands on the disc — dropping z, which is what
 *  orthographic projection comes to. */
Pos directionToDisc (Pos const &direction);

/** How many pieces a straight step across the *recorded* disc has to be cut
 *  into before each piece is projected onto the sphere and joined up.
 *
 *  Two measures, and the larger wins. The obvious one is how long the step is:
 *  a long step bends on a sphere and needs pieces to bend with. The one that
 *  is easy to miss is how far it swings the azimuth -- the disc's angle *is*
 *  the azimuth, so a step passing close to the disc's origin turns through up
 *  to half a revolution in almost no distance at all.
 *
 *  While the elevation base sits on the pole that costs nothing: the origin is
 *  drawn at the pole, in the middle of the sphere, where every azimuth is the
 *  same point. Move the base off the pole -- which is what sway does, every
 *  cycle -- and the origin is drawn out towards the rim, so the same swing
 *  becomes an arc most of the way round the sphere. Cut only by length, that
 *  arc was one straight line drawn clean across the picture.
 *
 *  `maxStep` is the longest piece wanted in the disc's own units; `maxSwing`
 *  the widest turn wanted in radians. Never fewer than one and never more than
 *  `maxPieces`, so a step that asks for the impossible is drawn coarsely
 *  rather than costing a frame. */
int discStepPieces (float x1, float y1, float x2, float y2, float maxStep,
                    float maxSwing, int maxPieces);

/** How finely a step of the disc is cut, and how far a drawn one may reach. */
struct DiscSampling
{
  /** The longest piece wanted in the disc's own units. */
  float maxStep = 0.03f;
  /** The widest turn wanted out of one piece, in radians. */
  float maxSwing = 0.02f;
  /** The most pieces one step may be cut into by those two measures. */
  int maxPieces = 256;
  /** How far apart two *drawn* points may be on the unit sphere before the
   *  piece between them is halved again. */
  float maxDrawn = 0.06f;
  /** How many times a piece may be halved. Eight is 256 more of them, and
   *  only where they are needed. */
  int maxDepth = 8;
};

/** Cut a straight step of the recorded disc into pieces and hand each one's
 *  far end to `emit`, in order. `project` maps a disc point to wherever it is
 *  drawn; `distance` says how far apart two of those are.
 *
 *  Cut where the projection actually moves rather than evenly along the step.
 *  The azimuth swing near the disc's origin is concentrated at the closest
 *  approach: spread evenly, most of the pieces are spent out where nothing is
 *  happening and the one place that needs them is starved -- measured, a path
 *  missing the origin by two thousandths still drew a third of the sphere in
 *  one straight line after 154 even pieces.
 *
 *  A template so the rule can be tested without a screen: the painter hands
 *  it its own projector, a test hands it the height map. */
template <typename Project, typename Distance, typename Emit>
void
sampleDiscStep (float x1, float y1, float x2, float y2,
                DiscSampling const &how, Project project, Distance distance,
                Emit emit)
{
  auto const at = [&] (float t) {
    return project (x1 + (x2 - x1) * t, y1 + (y2 - y1) * t);
  };

  auto const coarse = discStepPieces (x1, y1, x2, y2, how.maxStep,
                                      how.maxSwing, how.maxPieces);

  // Halve a piece while its two ends are drawn too far apart. Iterative with
  // an explicit stack, and pushed right before left so what comes out is in
  // order -- a path drawn out of order is a path drawn as a scribble.
  struct Piece
  {
    float t0, t1;
    decltype (at (0.f)) p0, p1;
    int depth;
  };

  auto previousT = 0.f;
  auto previous = at (0.f);

  for (int piece = 1; piece <= coarse; ++piece)
    {
      auto const t1
          = static_cast<float> (piece) / static_cast<float> (coarse);

      std::vector<Piece> stack;
      stack.push_back ({ previousT, t1, previous, at (t1), 0 });

      while (!stack.empty ())
        {
          auto const here = stack.back ();
          stack.pop_back ();

          if (here.depth < how.maxDepth
              && distance (here.p0, here.p1) > how.maxDrawn)
            {
              auto const tm = (here.t0 + here.t1) * 0.5f;
              auto const pm = at (tm);
              stack.push_back ({ tm, here.t1, pm, here.p1, here.depth + 1 });
              stack.push_back ({ here.t0, tm, here.p0, pm, here.depth + 1 });
              continue;
            }

          emit (here.p1);
        }

      previousT = t1;
      previous = at (t1);
    }
}

}
