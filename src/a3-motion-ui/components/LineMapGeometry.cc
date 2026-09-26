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

#include <a3-motion-ui/components/LineMapGeometry.hh>

#include <a3-motion-ui/Helpers.hh>

#include <cmath>

namespace a3
{

ProjectedLine
projectLine (juce::Path const &displayPath,
             ElevationParams const &elevationParams, HeightMap const &heightMap,
             PlaneShaping const &shaping, SphereCamera const &camera)
{
  ProjectedLine line;
  if (displayPath.isEmpty ())
    return line;

  // Project a 2D HOA point onto the sphere and return screen pos + z.
  // Every point of the line comes through here, which is why the shaping is
  // applied here and not by transforming the path: transforming the path would
  // mean copying it every frame, and the sub-sampling below would then be
  // measuring distances on the transformed copy.
  auto projectPoint = [&] (float x, float y) -> Pos {
    auto pos3D = heightMap.mapTo3D (
        shapedPosition (Pos::fromCartesian (x, y, 0.f), shaping),
        elevationParams);
    // The direction that comes back is the *seen* one: what is drawn nearer
    // the eye has to fade less, and which of two points that is depends on
    // where the eye is standing. Kept as a direction rather than flattened to
    // screen here, because a step too long to be a straight line has to be
    // walked along the sphere, and that walk is a walk between directions.
    return asSeenFrom (pos3D, camera);
  };

  // Maximum 2D step size before we insert intermediate samples.
  // Smaller = more sub-samples = smoother on the sphere. Flat mode has no
  // sphere curvature at all, so coarse sampling is fine there.
  float const maxStep = elevationParams.flat ? 0.06f : 0.03f;

  // ... and how wide a turn is wanted out of one piece. Flat mode has no
  // azimuth swing worth splitting for either -- there is no pole to be near.
  // See discStepPieces(), which is where both measures are weighed and why
  // the second one exists at all.
  DiscSampling sampling;
  sampling.maxStep = maxStep;
  // Flat mode has no pole to be near, so nothing to swing around.
  sampling.maxSwing = elevationParams.flat
                          ? juce::MathConstants<float>::pi
                          : 0.02f;

  // ... and what no amount of cutting can fix. At the disc's exact origin the
  // azimuth is not merely fast, it is undefined: the path arrives at one
  // bearing and leaves at the opposite one, so every sample on one side is
  // half a revolution from every sample on the other. With the base on the
  // pole that costs nothing -- both bearings are the same point up there --
  // and off the pole it is a real jump, which several of the shipped shapes
  // make (Clover, Infinity and Rose 4-Petal all pass exactly through the
  // origin). Drawn, it is a chord straight across the sphere that is in none
  // of the data. The pen goes up instead, the way it does at a take's gaps.
  auto constexpr maxJump = 0.3f;

  // Collect all projected points (with sub-sampling for long segments), and
  // remember where one subpath ends and the next begins.
  //
  // A path with several subpaths is several strokes: a take cut at its jumps,
  // a shape drawn in pieces. Only the very first point used to start a run, so
  // every later subpath was joined to the one before it by a line from where
  // that ended to where this begins -- a chord straight across the sphere that
  // is in none of the data. The Shape section never showed it because
  // strokePath knows about subpaths; this walks them by hand.
  line.points.reserve (512);
  line.depth.reserve (512);
  line.startsRun.reserve (512);

  Pos previousSeen;
  bool haveSeen = false;

  auto const keep = [&line] (Pos const &seen, bool starts) {
    line.points.push_back (cartesian2DHOA2JUCE (seen));
    line.depth.push_back (seen.z ());
    line.startsRun.push_back (starts);
  };

  auto const apart = [] (Pos const &a, Pos const &b) {
    auto const dx = a.x () - b.x ();
    auto const dy = a.y () - b.y ();
    auto const dz = a.z () - b.z ();
    return std::sqrt (dx * dx + dy * dy + dz * dz);
  };

  auto const addPoint = [&] (Pos const &seen, bool starts) {
    if (!starts && haveSeen)
      {
        auto const chord = apart (previousSeen, seen);

        if (chord > maxJump)
          {
            starts = true;
          }
        else if (chord > sampling.maxDrawn)
          {
            // Too long to be a straight line, so it is walked along the
            // sphere instead of ruled across it. This is the pad's origin:
            // the projection moves there without the disc moving, so no
            // amount of cutting the *step* brings these two any closer -- a
            // Clover's junction is nineteen degrees of one latitude, and the
            // chord between them passes through the inside of the sphere,
            // where the sound never is.
            auto const pieces = static_cast<int> (
                std::ceil (chord / sampling.maxDrawn));

            for (int i = 1; i < pieces; ++i)
              keep (slerpDirection (previousSeen, seen,
                                    static_cast<float> (i)
                                        / static_cast<float> (pieces)),
                    false);
          }
      }

    keep (seen, starts);
    previousSeen = seen;
    haveSeen = true;
  };

  juce::PathFlatteningIterator iter (displayPath, {}, 0.005f);

  bool firstPoint = true;
  float prevX = 0.f, prevY = 0.f;

  while (iter.next ())
    {
      // A stroke breaks where the segments stop meeting -- this one does not
      // start where the last one ended.
      //
      // Not iter.subPathIndex, whatever its name says: JUCE increments it on
      // every line marker (juce_PathIterator.cpp), so on a path built out of
      // lineTo -- which is every trajectory built from ticks -- every single
      // segment claimed to be a new sub-path. The line was drawn as a couple
      // of thousand two-point strokes, and wherever two ticks land far apart
      // in the picture, which is the pad's origin, it simply stopped and
      // started again. That is the gap at each of a Clover's junctions.
      auto const beginsSubPath
          = firstPoint || std::abs (iter.x1 - prevX) > 1e-6f
            || std::abs (iter.y1 - prevY) > 1e-6f;

      if (beginsSubPath)
        {
          // The first point of this subpath. Nothing joins it to what came
          // before -- that is what makes it a subpath.
          addPoint (projectPoint (iter.x1, iter.y1), true);
          prevX = iter.x1;
          prevY = iter.y1;
          firstPoint = false;
        }


      // Cut where the projection moves, not evenly along the step -- see
      // sampleDiscStep(), which halves a piece while its two ends land too
      // far apart. Spread evenly, the pieces are spent out where nothing is
      // happening and the closest approach to the disc's origin is starved.
      sampleDiscStep (prevX, prevY, iter.x2, iter.y2, sampling, projectPoint,
                      apart, [&addPoint] (Pos const &point, bool joined) {
                        addPoint (point, !joined);
                      });
      prevX = iter.x2;
      prevY = iter.y2;
    }


  return line;
}

}
