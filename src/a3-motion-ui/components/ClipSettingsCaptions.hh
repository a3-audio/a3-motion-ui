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

namespace a3
{

/** The caption under each control in the clip settings bar.
 *
 *  Named here rather than written at the drawing site so that the size
 *  computation below and the drawing code cannot drift apart — a caption
 *  the computation does not know about is a caption that gets cut off.
 *
 *  They are kept short on purpose: every one of them is fitted into a
 *  third of a section at most, and the longest one decides how large all
 *  the others may be drawn. "end-action" cost every caption in the bar a
 *  third of its height. */
namespace caption
{
constexpr char const *reach = "reach";
constexpr char const *pole = "pole";
constexpr char const *clipTop = "clip-top";
constexpr char const *clipBottom = "clip-bot";
constexpr char const *flat = "flat";
constexpr char const *flatElevation = "flat-elv";
constexpr char const *speed = "speed";
constexpr char const *direction = "dir";
constexpr char const *endAction = "end";
/** The filter's cutoff. The engine still calls it sweep (see
 *  Pattern/setFilterSweep); on screen it is what it does. */
constexpr char const *frequency = "freq";
constexpr char const *q = "Q";
}

/** The value a control shows: the state of a toggle, or the note value
 *  the Speed control displays.
 *
 *  Here for the same reason as the captions — the size computation has to
 *  know every string that can appear, or the widest one is the one that
 *  gets cut off. */
namespace value
{
constexpr char const *north = "North";
constexpr char const *south = "South";
constexpr char const *on = "On";
constexpr char const *off = "Off";
constexpr char const *directionNames[] = { "Forward", "Reverse", "PingPong" };
constexpr char const *endActionNames[] = { "Loop", "Stop", "Bounce" };
/** The widest speed label the Motion section can produce — whole bars
 *  above 1, fractions below (see A3MotionUIComponent's speedLog2 range). */
constexpr char const *widestSpeed = "1/16";
}

/** A string drawn on a control, together with the number of columns its
 *  section splits its width into — Elevation and Filter place two controls
 *  side by side, Motion three, so the same string has less room in
 *  Motion. */
struct TextEntry
{
  char const *text;
  int columns;
};

constexpr TextEntry captionTable[] = {
  { caption::reach, 2 },  { caption::pole, 2 },
  { caption::clipTop, 2 },     { caption::clipBottom, 2 },
  { caption::flat, 2 },        { caption::flatElevation, 2 },
  { caption::speed, 3 },       { caption::direction, 3 },
  { caption::endAction, 3 },   { caption::frequency, 2 },
  { caption::q, 2 },
};

constexpr TextEntry valueTable[] = {
  { value::north, 2 },
  { value::south, 2 },
  { value::on, 2 },
  { value::off, 2 },
  { value::directionNames[0], 3 },
  { value::directionNames[1], 3 },
  { value::directionNames[2], 3 },
  { value::endActionNames[0], 3 },
  { value::endActionNames[1], 3 },
  { value::endActionNames[2], 3 },
  { value::widestSpeed, 3 },
};

/** The one size every caption in the bar is drawn at.
 *
 *  `baseSize` is what the theme asks for; the result never exceeds it. The
 *  bar's controls all sit in sections of the same width, so a section's
 *  content width plus the gap between its columns is enough to know how
 *  much room each caption has. `controlBoxHeight` is the shortest control
 *  box in the bar; the caption row is given a fixed share of it. */
float sharedCaptionSize (float baseSize, int sectionContentWidth, int columnGap,
                         int controlBoxHeight);

/** The one size every value in the bar is drawn at, the same way.
 *
 *  A control that shows a value has no knob under it, so the value row and
 *  the caption row share the box between them. */
float sharedValueSize (float baseSize, int sectionContentWidth, int columnGap,
                       int controlBoxHeight);

/** The share of a control box each text row may take. A knob's box holds a
 *  caption and the knob; a value's box holds a value and a caption, and
 *  the value gets the larger share — it is what the control is about,
 *  the caption only names it. */
constexpr float captionRowShare = 0.40f;
constexpr float valueRowShare = 0.45f;
/** A text row is drawn this much taller than its font. */
constexpr float rowHeightFactor = 1.25f;

}
