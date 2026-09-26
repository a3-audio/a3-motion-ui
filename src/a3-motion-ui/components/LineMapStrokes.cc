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

#include <a3-motion-ui/components/LineMapStrokes.hh>

#include <a3-motion-ui/components/SphereProjection.hh>

#include <algorithm>

namespace a3
{

juce::Path
pathOf (std::vector<juce::Point<float> > const &points,
        std::vector<bool> const &lifts)
{
  juce::Path path;
  for (std::size_t i = 0; i < points.size (); ++i)
    {
      if (lifts[i])
        path.startNewSubPath (points[i]);
      else
        path.lineTo (points[i]);
    }
  return path;
}

juce::Point<float>
toLineMap (juce::Point<float> const &seen)
{
  return juce::Point<float> (
      (seen.x / lineMapExtent * 0.5f + 0.5f) * static_cast<float> (lineMapSize),
      (seen.y / lineMapExtent * 0.5f + 0.5f) * static_cast<float> (lineMapSize));
}

namespace
{

/** A stroke has a line in it only if some point is joined to the one before;
 *  a piece that is all pen-lifts is what juce::Path::isEmpty() called empty,
 *  and stroking the maps in software always skipped it. */
bool
hasALine (MapStroke const &stroke)
{
  for (std::size_t i = 1; i < stroke.points.size (); ++i)
    if (!stroke.lifts[i])
      return true;
  return false;
}

}

std::vector<MapStroke>
lineMapStrokes (ProjectedLine const &line)
{
  std::vector<MapStroke> strokes;
  auto const count = line.points.size ();
  if (count < 2)
    return strokes;

  // Walk a run of points into a path, lifting the pen where the line does.
  auto const pieceOf = [&] (std::size_t start, std::size_t stop) {
    MapStroke piece;
    for (auto i = start; i <= stop; ++i)
      {
        piece.points.push_back (toLineMap (line.points[i]));
        piece.lifts.push_back (i == start || line.startsRun[i]);
      }
    return piece;
  };

  // The same run, thinned to the points a wide stroke can actually show --
  // see thinByArcLength(), which is where the rule lives and is tested.
  auto const spacedPieceOf
      = [&] (std::size_t start, std::size_t stop, float spacing) {
    std::vector<juce::Point<float> > run;
    std::vector<bool> runLifts;
    run.reserve (stop - start + 1);
    runLifts.reserve (stop - start + 1);
    for (auto i = start; i <= stop; ++i)
      {
        run.push_back (toLineMap (line.points[i]));
        runLifts.push_back (line.startsRun[i]);
      }

    MapStroke piece;
    for (auto const index : thinByArcLength (run, runLifts, spacing))
      {
        piece.lifts.push_back (piece.points.empty () || runLifts[index]);
        piece.points.push_back (run[index]);
      }
    return piece;
  };

  // How far behind the ball a run of the line sits, as the light it keeps.
  auto const depthOf = [&] (std::size_t start, std::size_t stop) {
    auto worst = 1.f;
    for (auto i = start; i <= stop; ++i)
      worst = std::min (worst, lineDepthFade (line.depth[i]));
    return worst;
  };

  auto const keep = [&strokes] (MapStroke piece, float width,
                                juce::Colour colour) {
    piece.width = width;
    piece.colour = colour;
    if (hasALine (piece))
      strokes.push_back (std::move (piece));
  };

  // The falling cone of nearness, widest and dimmest first: a narrower stroke
  // lies wholly inside a wider one, so overwriting is the maximum a distance
  // field needs.
  //
  // In pieces, like the core, because each piece carries its own depth in the
  // blue. The cone is what the glow, the filaments and the bolts are all built
  // from, so a cone with no depth in it meant everything the shader draws came
  // out equally bright on both sides of the ball — and drowned the one layer
  // that did fade. Far fewer pieces than the core needs: depth changes slowly
  // along a line where the arc length changes with every step.
  auto const conePer = std::max<std::size_t> (2, count / lineMapConePieces);
  for (auto const &step : lineMapSteps)
    {
      auto const width = step.width * lineMapTexels;
      auto const coneSpacing = width * 0.25f;
      for (std::size_t start = 0; start + 1 < count; start += conePer)
        {
          auto const stop = std::min (start + conePer, count - 1);
          keep (spacedPieceOf (start, stop, coneSpacing), width,
                juce::Colour::fromFloatRGBA (step.nearness, 0.f,
                                             depthOf (start, stop), 1.f));
        }
    }

  // And the core, in pieces, each carrying where along the figure it is.
  auto const per = std::max<std::size_t> (2, count / lineMapPieces);
  for (std::size_t start = 0; start + 1 < count; start += per)
    {
      auto const stop = std::min (start + per, count - 1);
      auto const u = (static_cast<float> (start + stop) * 0.5f)
                     / static_cast<float> (count - 1);
      keep (pieceOf (start, stop), lineMapCoreWidth * lineMapTexels,
            juce::Colour::fromFloatRGBA (lineMapCoreNearness, u,
                                         depthOf (start, stop), 1.f));
    }

  return strokes;
}

}

namespace a3
{

juce::Point<float>
toStrandMap (juce::Point<float> const &seen)
{
  return juce::Point<float> (
      (seen.x / lineMapExtent * 0.5f + 0.5f) * static_cast<float> (strandMapSize),
      (seen.y / lineMapExtent * 0.5f + 0.5f) * static_cast<float> (strandMapSize));
}

namespace
{

// Depth-band helpers
int
depthBand (float z)
{
  if (z < -0.5f)
    return 0;
  if (z < 0.f)
    return 1;
  if (z < 0.5f)
    return 2;
  return 3;
}

bool
hasALine (Polyline const &piece)
{
  for (std::size_t i = 1; i < piece.points.size (); ++i)
    if (!piece.lifts[i])
      return true;
  return false;
}

}

BraidCord
braidCord (ProjectedLine const &line, SheathRing const &braid, float seconds)
{
  BraidCord cord;
  auto const count = line.points.size ();
  if (count < 2)
    return cord;

  auto const strands = juce::jlimit (1, 5, braid.strands);
  auto const plain = strands < 2 || !(braid.radius > 0.0001f);

  // Which way is across the line at each point, and how much offset it
  // carries there. Both from sheathFrames(), which looks past the repeated
  // points every recording holds -- a still hand used to read as an
  // infinitely tight bend and snap the strands onto the axis.
  std::vector<juce::Point<float> > across (count);
  std::vector<float> guard (count, 1.f);
  if (!plain)
    {
      std::vector<SheathPoint> sheath (count);
      for (std::size_t i = 0; i < count; ++i)
        sheath[i] = { line.points[i].x, line.points[i].y,
                      static_cast<bool> (line.startsRun[i]) };

      auto const frames = sheathFrames (sheath, braid.radius);
      for (std::size_t i = 0; i < count; ++i)
        {
          across[i] = { frames[i].acrossX, frames[i].acrossY };
          guard[i] = frames[i].guard;
        }
    }

  // Five depth tiers: a strand crosses every boundary twice a turn, and at
  // three with plainly different brightnesses you read the boundary rather
  // than the strand.
  auto constexpr numTiers = BraidCord::tiers;

  auto const total = static_cast<float> (count - 1);

  for (auto strand = 0; strand < strands; ++strand)
    {
      auto lastBand = -1;
      auto lastTier = -1;
      juce::Point<float> lastPoint;

      for (std::size_t i = 0; i < count; ++i)
        {
          auto const u = total > 0.f ? static_cast<float> (i) / total : 0.f;
          auto const sample
              = plain ? SheathSample{} : sheathAt (u, strand, braid, seconds);

          auto const point
              = line.points[i] + across[i] * (sample.offset * guard[i]);

          auto const band = depthBand (line.depth[i]);
          auto const tier = juce::jlimit (
              0, numTiers - 1,
              static_cast<int> ((sample.depth + 1.f) * 0.5f
                                * static_cast<float> (numTiers)));

          auto const broken = line.startsRun[i] || lastBand < 0
                              || band != lastBand || tier != lastTier;
          auto &piece = cord.pieces[static_cast<std::size_t> (band)]
                                   [static_cast<std::size_t> (tier)];
          if (broken)
            {
              piece.points.push_back (
                  line.startsRun[i] || lastBand < 0 ? point : lastPoint);
              piece.lifts.push_back (true);
            }
          piece.points.push_back (point);
          piece.lifts.push_back (false);

          lastBand = band;
          lastTier = tier;
          lastPoint = point;
        }
    }

  return cord;
}

std::vector<MapStroke>
strandMapStrokes (BraidCord const &cord)
{
  std::vector<MapStroke> strokes;

  // Back to front, so a strand passes behind the cord and comes out the other
  // side. The step between one tier and the next is small on purpose: a coil
  // brightens as it comes round, it does not switch.
  for (auto tier = 0; tier < BraidCord::tiers; ++tier)
    {
      auto const front = BraidCord::tiers > 1
                             ? static_cast<float> (tier)
                                   / static_cast<float> (BraidCord::tiers - 1)
                             : 1.f;
      for (auto band = 0; band < BraidCord::bands; ++band)
        {
          auto const &piece = cord.pieces[static_cast<std::size_t> (band)]
                                         [static_cast<std::size_t> (tier)];
          if (!hasALine (piece))
            continue;

          // Into the strand map: red says a strand is here, green how far
          // in front of the cord it is at this point. Opaque, so the
          // premultiplied image keeps both values as written; back tiers
          // first, so where two cross the nearer one is what is left.
          MapStroke stroke;
          // Into map space first, then stroked in texels. strokePath's own
          // transform moves the points and leaves the thickness alone, so
          // passing it there drew every strand at a hundredth of a texel —
          // a tenth-opaque smear the shader's threshold never saw.
          for (auto const &p : piece.points)
            stroke.points.push_back (toStrandMap (p));
          stroke.lifts = piece.lifts;
          // A strand wide enough to survive the bilinear filter, and no
          // wider: the shader sharpens it back down, as it does the cord.
          stroke.width = 1.7f * strandMapTexels;
          stroke.colour = juce::Colour::fromFloatRGBA (1.f, front, 0.f, 1.f);
          strokes.push_back (std::move (stroke));
        }
    }

  return strokes;
}

}
