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

#include <JuceHeader.h>

#include <a3-motion-ui/components/LineMapGeometry.hh>
#include <a3-motion-ui/components/PlasmaSheath.hh>

#include <array>
#include <vector>

namespace a3
{

// The line map: how many texels across, and how far out it reaches in the
// units the shader thinks in -- sphere radii, with the ball's edge at one.
//
// A little past the ball, because a glow that stopped at the rim would cut
// where a trajectory runs off the edge. Two hundred and fifty-six across is
// about two and a half screen pixels per texel at the sizes this ships at:
// coarse for a *line*, which is why the crisp line is still drawn as a vector
// on top, and plenty for a *field*, which is all the shader asks of it.
// 512, not 256.
//
// At 256 over 1.3 sphere radii a texel is two and a half screen pixels, and a
// hairline two pixels wide cannot be held in that at all — which is why the
// sharp core of the trajectory was drawn by JUCE into an overlay instead, and
// why it looked raw and floated over everything: that overlay is blitted after
// the whole scene and knows nothing about depth, towers or glow.
//
// Doubled, a texel is 1.4 screen pixels. The core can live in the map, and the
// trajectory becomes one thing the shader draws rather than two layers that
// happen to line up.
// It stops here, and what is not here is why: the hairlines have a map of
// their own now, and this one holds the cone alone. See strandMapSize.
//
// Every width below is written in texels of a 512 map and scaled by
// lineMapTexels, so the cone and the core keep the size they had in the room
// rather than being halved by a finer grid.
auto constexpr lineMapSize = 512;
constexpr float lineMapTexels = lineMapSize / 512.f;
auto constexpr lineMapExtent = 1.3f;

// The nested strokes that make the stepped cone of nearness: half-width in
// texels, and how near that says you are. Widest and dimmest first -- each is
// drawn over the last, and a narrower stroke is wholly inside a wider one, so
// overwriting *is* the maximum a distance field needs.
struct LineMapStep
{
  float width;
  float nearness;
};
// Ten of them, not five. The shader builds everything it draws out of this
// ramp, so the ramp's own terraces are what "sehr pixelig" was looking at: at
// five steps the field jumps by a fifth of its range between neighbouring
// bands and no amount of bilinear filtering hides a step that size. Ten small
// strokes into a 256-square image cost almost nothing.
// Widths are in texels, so they double with the map to keep the same size in
// the room.
constexpr LineMapStep lineMapSteps[] = {
  { 68.f, 0.04f }, { 56.f, 0.10f }, { 46.f, 0.17f }, { 38.f, 0.25f },
  { 30.f, 0.34f }, { 24.f, 0.44f }, { 18.f, 0.55f }, { 14.f, 0.66f },
  { 10.f, 0.77f }, { 7.f, 0.86f },
};

// The innermost step is drawn on its own, and in pieces.
//
// It carries two things at once: full nearness in the red, and *where along
// the figure* this piece is in the green. The second is what lets the shader
// twist the cord: a weave is a pattern that travels along a line, and a
// fragment shader has no idea where along anything it is unless it is told.
//
// In pieces because a stroke has one colour and the arc length has to change
// along the line. Only at this width, not at all of them, or it would be a
// hundred and twenty strokes five times over.
// Not doubled, but not halved either, and one texel is too few: a stroke that
// narrow is averaged away by the bilinear filter before it ever reaches full
// nearness, so the shader finds no core to sharpen and draws glow alone.
// Measured on the device at 1.0 — the cord had no bright centre at all.
//
// At 2.4 texels it is 3.4 screen pixels of *map*, which the shader's own
// `tight` term (nearness to the ninth power) then pulls back down to a
// hairline. The map has to hold more than the line is wide.
constexpr float lineMapCoreWidth = 1.8f;
constexpr int lineMapPieces = 120;

// The cone is drawn in pieces too, for the depth it carries in the blue — but
// far fewer of them. Arc length changes with every step along the line and
// depth does not, and the cone is ten strokes where the core is one.
constexpr int lineMapConePieces = 24;

// What the core is worth, and it is deliberately short of one.
//
// A field that saturates cannot be modulated: with the core at full nearness
// the shader's weave scaled a value that was already clamped, so the cord's
// waist never moved and only its brightness did. Left with headroom, the same
// weave narrows and widens it along its length, which is the scalloped
// silhouette of a laid rope.
constexpr float lineMapCoreNearness = 0.88f;

// The braid's strands get a finer grid than the cone does, and this is the one
// number that made the trajectory look pixelated.
//
// The two maps hold two different kinds of thing. The cone is a wide, soft
// distance field: ten nested strokes up to sixty-eight texels across, so its
// cost is *area* and doubling the grid quadruples it — measured on the device,
// 12 ms a frame at 512 and 20 ms at 1024, for a field that has nothing fine in
// it to show. The strands are five hairlines 1.7 texels wide: their cost is
// length, not area, and at 512 they are thin enough that the bilinear filter
// and the shader's threshold beat against the grid and bead the line. That
// beading is what "ich will keine Pixel sehen" was looking at.
//
// Both maps cover the same `lineMapExtent`, so the shader samples them with
// the same uv and needs to know nothing about this.
auto constexpr strandMapSize = 1024;
constexpr float strandMapTexels = strandMapSize / 512.f;

/** One piece of a line map as it is painted: a polyline in map texels
 *  (image orientation, y down), stroked at one width in one opaque colour
 *  over whatever is already there -- curved joins, rounded ends.
 *
 *  LineMapRenderer paints them as capsules (a3-motion-ui#34), in the list's
 *  order, which is what makes the stepped cone a distance field.
 */
struct MapStroke
{
  std::vector<juce::Point<float> > points;
  /** True where a new sub-path begins at this point; the first always does. */
  std::vector<bool> lifts;
  float width = 0.f;
  juce::Colour colour;
};

/** The pen's path through `points`: a new sub-path wherever `lifts` says,
 *  a line to every other point. How the visible braid of a line that has
 *  no map is handed to juce::Graphics. */
juce::Path pathOf (std::vector<juce::Point<float> > const &points,
                   std::vector<bool> const &lifts);

/** Where a point the camera sees lands in the line map. */
juce::Point<float> toLineMap (juce::Point<float> const &seen);

/** The cone's ten steps, widest first, then the core -- in painting order. */
std::vector<MapStroke> lineMapStrokes (ProjectedLine const &line);

/** Where a point the camera sees lands in the strand map: the same extent as
 *  the line map, on its finer grid. */
juce::Point<float> toStrandMap (juce::Point<float> const &seen);

/** A run of points with the pen lifted where a new sub-path begins -- what a
 *  juce::Path built from startNewSubPath/lineTo holds. */
struct Polyline
{
  std::vector<juce::Point<float> > points;
  std::vector<bool> lifts;
};

/** The braid around a line, cut into pieces by how far behind the ball
 *  (band, from the line's depth) and how far round the cord (tier, from the
 *  strand's own depth) each stretch is. Tiers are painted back to front, so
 *  a strand passes behind the cord and comes out the other side.
 *
 *  In the sphere's screen units, like ProjectedLine. `seconds` turns the
 *  strands; `braid` with fewer than two strands or no radius is one plain
 *  strand on the line itself. */
struct BraidCord
{
  static constexpr int tiers = 5;
  static constexpr int bands = 4;
  std::array<std::array<Polyline, tiers>, bands> pieces;
};

BraidCord braidCord (ProjectedLine const &line, SheathRing const &braid,
                     float seconds);

/** The braid as strokes into the strand map: tiers back to front, each band
 *  within a tier, R 1 where a strand is, G how far in front of the cord. */
std::vector<MapStroke> strandMapStrokes (BraidCord const &cord);

}
