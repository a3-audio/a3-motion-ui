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
constexpr char const *sweep = "sweep";
constexpr char const *q = "Q";
}

/** A caption together with the number of columns its section splits its
 *  width into — Elevation and Filter place two controls side by side,
 *  Motion three, so the same string has less room in Motion. */
struct CaptionEntry
{
  char const *text;
  int columns;
};

constexpr CaptionEntry captionTable[] = {
  { caption::reach, 2 },  { caption::pole, 2 },
  { caption::clipTop, 2 },     { caption::clipBottom, 2 },
  { caption::flat, 2 },        { caption::flatElevation, 2 },
  { caption::speed, 3 },       { caption::direction, 3 },
  { caption::endAction, 3 },   { caption::sweep, 2 },
  { caption::q, 2 },
};

/** The one size every caption in the bar is drawn at.
 *
 *  `baseSize` is what the theme asks for; the result never exceeds it. The
 *  bar's controls all sit in sections of the same width, so a section's
 *  content width plus the gap between its columns is enough to know how
 *  much room each caption has. `controlBoxHeight` is the shortest control
 *  box in the bar — the caption row takes at most half of it. */
float sharedCaptionSize (float baseSize, int sectionContentWidth, int columnGap,
                         int controlBoxHeight);

}
