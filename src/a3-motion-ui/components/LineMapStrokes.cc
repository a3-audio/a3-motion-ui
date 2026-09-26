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
 *  and the software path always skipped it. */
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
