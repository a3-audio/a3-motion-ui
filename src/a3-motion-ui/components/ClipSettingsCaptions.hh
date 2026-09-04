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
/** What the Action key does to this clip. Three letters, like the "dir" and
 *  "end" it stands in a row with -- and "actmode" was the longest caption in
 *  the bar's tightest columns, which pulled the shared caption size down for
 *  every section at once. Naming it after the key is safe now that the key
 *  itself has left this section for the header. */
constexpr char const *actMode = "act";
/** The filter's cutoff. The engine still calls it sweep (see
 *  Pattern/setFilterSweep); on screen it is what it does. */
constexpr char const *frequency = "freq";
constexpr char const *recordLength = "len";
/** The time taken out of the take's end to close its join. */
constexpr char const *fade = "fade";
/** How fast the whole trajectory turns under the blob, in bars per
 *  revolution either way round. */
constexpr char const *spin = "spin";
/** The sweep that opens and closes the coverage: reach travels out of where
 *  it was set, to one pole or the other, and back. Five letters and not six,
 *  because every caption in the bar is drawn at one shared size and the
 *  widest of them sets it — "breath" would have shrunk the lot. */
constexpr char const *swell = "swell";
/** The accent's rise and fall, played with ACT rather than set: how long it
 *  takes to come up while the pad is down, and to fall once it is let go. */
constexpr char const *attack = "atk";
constexpr char const *decay = "dec";
/** How far the accent throws: the 3d it rises to. The channel's own 3d is the
 *  floor, this the ceiling. */
constexpr char const *envelopeMax = "max";
/** Which way the clip's shape faces — a standing angle, where spin is the
 *  movement over it. */
constexpr char const *rotate = "rot";
/** What a recording pass writes where the finger is not. The one control in
 *  the bar that is not the shown clip's — it is the same for every channel. */
constexpr char const *recMode = "recmode";
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
/** Short on purpose, like the captions: Motion gives each of these a third
 *  of a section, and the longest one decided how large every value in the
 *  bar could be drawn. "PingPong" held them all below their own captions. */
/** Where a clip sets off. "Ping" used to sit here and meant the same thing as
 *  the Bounce end action -- two controls for one behaviour. */
constexpr char const *directionNames[] = { "Fwd", "Rev" };
/** Two words for what the Action key does. Short enough for a button a third
 *  of a narrow section wide -- "1shot" rather than "one-shot", which fitted
 *  nowhere. */
constexpr char const *actModeNames[] = { "1shot", "Hold" };
constexpr int numActModes
    = static_cast<int> (sizeof (actModeNames) / sizeof (*actModeNames));

constexpr char const *endActionNames[] = { "Loop", "Stop", "Paus", "Bnce",
                                           "Rnd" };
constexpr int numEndActions
    = static_cast<int> (sizeof (endActionNames) / sizeof (*endActionNames));
// What happens to what a take never wrote — glide across it, or hold and jump.
/** How long the take's closing move lasts, in sixteenths of a beat. "off" is
 *  a hard join: the take holds and jumps rather than travelling back. */
inline juce::String fadeName (int sixteenths)
{
  if (sixteenths <= 0)
    return "off";
  return juce::String (sixteenths) + "/16";
}
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
  { caption::reach, 2 },       { caption::pole, 2 },
  { caption::clipTop, 2 },     { caption::clipBottom, 2 },
  { caption::flat, 2 },        { caption::flatElevation, 2 },
  // Motion carries four since the fade joined it — the tightest columns in
  // the bar, and what pins the shared size for everything above.
  { caption::speed, 4 },       { caption::direction, 4 },
  { caption::endAction, 4 },   { caption::fade, 4 },
  { caption::spin, 4 },        { caption::swell, 4 },
  { caption::attack, 4 },      { caption::decay, 4 },
  { caption::envelopeMax, 4 },   { caption::rotate, 4 },
  { caption::actMode, 4 },
  { caption::frequency, 2 },   { caption::q, 2 },
  { caption::recordLength, 1 },
};

constexpr TextEntry valueTable[] = {
  { value::north, 2 },
  { value::south, 2 },
  { value::on, 2 },
  { value::off, 2 },
  { value::directionNames[0], 4 },
  { value::directionNames[1], 4 },
  { value::actModeNames[0], 4 },
  { value::actModeNames[1], 4 },
  { value::endActionNames[0], 4 },
  { value::endActionNames[1], 4 },
  { value::endActionNames[2], 4 },
  { value::endActionNames[3], 4 },
  { value::endActionNames[4], 4 },
  { "16/16", 4 },
  { value::widestSpeed, 4 },
};

/** The one size every caption in the bar is drawn at.
 *
 *  `baseSize` is what the theme asks for; the result never exceeds it. The
 *  bar's controls all sit in sections of the same width, so a section's
 *  content width plus the gap between its columns is enough to know how
 *  much room each caption has. `controlBoxHeight` is the shortest control
 *  box in the bar; the caption row is given a fixed share of it. */
/** How wide a knob is: a multiple of the body font, scaled by Pot Size.
 *
 *  Tied to the font rather than to the section width, so the whole bar has one
 *  scale. The factor is what the old height-derived knob came out at with the
 *  shipped settings, so the default look is unchanged. */
int knobDiameterForFont (float bodySize, float potSizeScale);

/** How tall a control box must be to hold its knob and both text rows.
 *
 *  The dependency used to run the other way: the bar was nailed to a quarter
 *  of the screen, the box fell out of that, and the box capped the font — so
 *  Body Font Size changed nothing in this bar at any setting. Now the box is
 *  as tall as its contents need, and the bar follows. */
int controlBoxHeightForFont (float bodySize, int knobDiameter);

/** How tall the whole clip settings bar wants to be: a title row, the
 *  elevation section's graphic, and three rows of control boxes. */
int clipSettingsPreferredHeight (float headerSize, float bodySize,
                                 int knobDiameter);

/** The largest share of the screen the bar may take. Past this the font is
 *  capped again — the old behaviour, but only at the extreme rather than at
 *  every setting. */
constexpr float maxClipSettingsScreenShare = 0.50f;

/** `wanted`, clamped to what the screen can spare. */
int clipSettingsHeightWithin (int wanted, int screenHeight);

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
