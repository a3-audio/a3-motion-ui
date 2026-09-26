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

/** One piece of a line map as it is painted: a polyline in map texels
 *  (image orientation, y down), stroked at one width in one opaque colour
 *  over whatever is already there -- curved joins, rounded ends.
 *
 *  The software path strokes these with juce::Graphics, the GPU pass as
 *  capsules (a3-motion-ui#34); both paint the list in its order, which is
 *  what makes the stepped cone a distance field.
 */
struct MapStroke
{
  std::vector<juce::Point<float> > points;
  /** True where a new sub-path begins at this point; the first always does. */
  std::vector<bool> lifts;
  float width = 0.f;
  juce::Colour colour;
};

/** Where a point the camera sees lands in the line map. */
juce::Point<float> toLineMap (juce::Point<float> const &seen);

/** Whether config.json asks for the line map to be painted on the GPU
 *  (`"ui": { "gpuLineMaps": true }`) instead of by the software strokes.
 *
 *  Off unless it is a real JSON true: the two paths paint the same list, and
 *  the switch exists so they can be compared on the device (a3-motion-ui#34).
 *  Read with the rest of the visual config, so it can be flipped while the
 *  app runs. */
bool gpuLineMapsWanted (juce::var const &config);

/** The cone's ten steps, widest first, then the core -- in painting order. */
std::vector<MapStroke> lineMapStrokes (ProjectedLine const &line);

}
