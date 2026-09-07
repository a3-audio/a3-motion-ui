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

/** Where the room is being looked at from.
 *
 *  The display has always been one view: straight down on the room, the
 *  listener's zenith in the middle and the horizon at the rim. That is the
 *  right *default* -- it is the view a plan of a room is drawn in, nothing is
 *  hidden behind anything, and every azimuth is equally readable. It is also
 *  the one view in which a north-south travel is invisible, which is why
 *  there is a second one.
 *
 *  `pitch` is how far the eye has come down from straight above: 0 is the
 *  view the device has always had, a right angle is looking in from the
 *  horizon. `turn` is which way round it has walked. Both are radians, and
 *  both being zero is the identity -- so a device nobody has tilted computes
 *  exactly what it computed before, to the bit.
 */
struct SphereCamera
{
  float pitch = 0.f;
  float turn = 0.f;

  bool isOverhead () const { return pitch == 0.f && turn == 0.f; }
};

/** A direction in the room, seen from where the camera stands.
 *
 *  Turn first, then pitch: walking round the room and then leaning over it is
 *  what a person does, and the other order tips the room sideways instead. */
Pos asSeenFrom (Pos const &direction, SphereCamera const &camera);

/** And back: a direction in the view, in the room's own terms. The exact
 *  inverse of asSeenFrom -- a finger lands on the view and has to be given to
 *  the room, or the blob does not come out under it. */
Pos asSeenFromInverse (Pos const &viewed, SphereCamera const &camera);

/** A point of the way from one direction to another, walked *along* the
 *  sphere rather than straight across it.
 *
 *  Every drawn step is a straight line between two projected points, which is
 *  fine while the steps are short. Near the pad's origin they are not: the
 *  projection moves there without the disc moving, so two neighbouring samples
 *  sit a fair way apart on the same latitude -- a Clover's junction is
 *  nineteen degrees wide. A straight line between those two is a chord through
 *  the *inside* of the sphere, the one place the sound never is, and it reads
 *  as a line ruled across the picture.
 *
 *  Both ends are assumed to be on the unit sphere, which is what a projected
 *  direction is. `t` runs 0 at `from` to 1 at `to`. */
Pos slerpDirection (Pos const &from, Pos const &to, float t);

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
  /** The hole in the middle of the disc: how close to its origin the drawing
   *  may go, in the disc's own units.
   *
   *  Inside it the picture is not merely difficult, it does not exist. The
   *  radius there stands for a whole latitude circle, so the bearing a sample
   *  comes back with is whatever the last few thousandths of the shape happen
   *  to point at -- measured on a Clover at a base of a fifth, the two arms of
   *  a junction come out nineteen degrees apart when the path is flattened at
   *  five thousandths and a hundred and thirteen degrees at two ten
   *  thousandths. A picture that changes that much with how finely it is
   *  sampled is not a picture of anything.
   *
   *  So the drawing stops at the edge of the hole, where the shape's own
   *  bearings are still its own, and the two arms are joined across it. Two
   *  hundredths of the disc is a couple of pixels of the shape and the last
   *  place the shape still means something. */
  float originHole = 0.03f;
};

/** Cut a straight step of the recorded disc into pieces and hand each one's
 *  far end to `emit`, in order, along with whether the caller may join it to
 *  the point before it.
 *
 *  It may not always. Halving a piece has a floor -- maxDepth -- and at the
 *  disc's origin the azimuth swing does not: however fine the cut, the two
 *  ends of the last piece are still most of a latitude circle apart. What the
 *  sampler hands over there is not a piece of the path, it is the two banks of
 *  a gap, and a line between them is a chord across the sphere that is in none
 *  of the data. Short enough to slip under the drawer's own pen lift, long
 *  enough to see: it was the little straight hook at the end of each of a
 *  Clover's petals.
 *
 *  So the sampler says what it knows. `joined` is false for a piece it could
 *  not resolve, and the pen goes up.
 *
 *  `project` maps a disc point to wherever it is drawn; `distance` says how
 *  far apart two of those are.
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
  auto const stepX = x2 - x1;
  auto const stepY = y2 - y1;
  auto const stepLength = std::sqrt (stepX * stepX + stepY * stepY);

  // Inside the hole there is nothing to draw -- see DiscSampling::originHole.
  auto const inHole = [&] (float t) {
    auto const x = x1 + stepX * t;
    auto const y = y1 + stepY * t;
    return x * x + y * y < how.originHole * how.originHole;
  };

  auto const at = [&] (float t) {
    auto x = x1 + stepX * t;
    auto y = y1 + stepY * t;

    // A point inside the hole is still measured -- the halving needs
    // somewhere to put it -- but it is measured at the hole's edge, not at
    // the middle, where the projection would answer with whatever atan2 makes
    // of two zeroes: a bearing of zero, one fixed corner of the room. Held on
    // its own bearing rather than moved to another one, so nothing is
    // invented; and it is never emitted, so nothing of it is drawn.
    auto const radius = std::sqrt (x * x + y * y);
    if (radius > 0.f && radius < how.originHole)
      {
        x *= how.originHole / radius;
        y *= how.originHole / radius;
      }

    return project (x, y);
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

  // What the caller's pen is on. The step starts where the last one ended, so
  // this is the right thing to measure the first piece against -- and it is
  // the only way to notice the hole: the pieces either side of it are each
  // small, and it is the distance *across* it that says whether the two banks
  // belong to one line.
  auto lastEmitted = previous;

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

          auto const reach = distance (here.p0, here.p1);

          if (here.depth < how.maxDepth && reach > how.maxDrawn)
            {
              auto const tm = (here.t0 + here.t1) * 0.5f;
              auto const pm = at (tm);
              stack.push_back ({ tm, here.t1, pm, here.p1, here.depth + 1 });
              stack.push_back ({ here.t0, tm, here.p0, pm, here.depth + 1 });
              continue;
            }

          juce::ignoreUnused (reach);

          if (!inHole (here.t1))
            {
              emit (here.p1, distance (lastEmitted, here.p1) <= how.maxDrawn);
              lastEmitted = here.p1;
            }
        }

      previousT = t1;
      previous = at (t1);
    }
}

}
