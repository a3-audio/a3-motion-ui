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

#include <a3-motion-engine/Playhead.hh>

namespace a3
{

class Pattern;

/** Everything the clip settings menu holds, as plain values.
 *
 *  Separate from both the Pattern that owns them at runtime and the file they
 *  are written to: ClipFile serialises this, PatternLibrary applies it, and the
 *  migration builds one out of an old combined file. Three users, one list of
 *  fields — and the list of fields is exactly what drifts apart when each user
 *  keeps its own copy of it.
 *
 *  The defaults are a fresh Pattern's defaults, so a clip file that leaves a
 *  field out loads as "not set" rather than as something else. That is what
 *  lets a new setting be added without invalidating every file already
 *  written, and a test holds the two lists of defaults together.
 *
 *  What is deliberately *not* here: the running phases (spinPhase,
 *  reachLfoPhase) and the play position. Where a movement happens to be at the
 *  moment of saving is not a setting, it is where the clip got to — restoring
 *  it would make a saved clip start mid-turn.
 */
struct ClipSettings
{
  int speedLog2 = 0;
  float rotate = 0.f;

  float reach = 0.5f;
  float clipTop = 0.f;
  float clipBottom = 0.f;
  bool mirrorSouth = false;
  bool flat = false;
  float flatElevation = 0.5f;

  int spin = 0;
  int reachLfo = 0;
  int envelopeAttack = 2;
  int envelopeDecay = 3;
  float envelopeMax = 1.f;
  ActMode actMode = ActMode::OneShot;

  PlayDirection direction = PlayDirection::Forward;
  EndAction endAction = EndAction::Loop;

  /** How far a gap in the movement may be for the fade to draw through it,
   *  0..1 of the sphere's diameter. Zero plays every jump as a jump; one
   *  closes every gap.
   *
   *  A distance, not a time: the fade is no longer something done to a
   *  recording's seam but a reading of the movement's holes. */
  float fadeReach = 0.25f;

  /** Where a bridged gap leads. Zero is the next tick in time; negative goes
   *  to the nearest other run, positive to a random one. Bipolar like spin and
   *  swell, and the magnitude mixes rather than switches. */
  int bridgeBias = 0;
};

/** Two settings are the same when every field is.
 *
 *  This is what "unsaved" is worked out with: a pattern is compared against
 *  its clip file rather than a flag being set when something is touched. The
 *  difference shows when a control is turned and turned back -- a flag would
 *  still say unsaved, and comparing says what is true.
 *
 *  The floats are compared exactly on purpose. They are carried from one place
 *  to another and never computed, so a value that has been through a file and
 *  back is bit-for-bit what went in; a tolerance here would hide a field that
 *  is quietly rounding.
 */
inline bool
operator== (ClipSettings const &a, ClipSettings const &b)
{
  return a.speedLog2 == b.speedLog2                //
         && a.rotate == b.rotate                   //
         && a.reach == b.reach                     //
         && a.clipTop == b.clipTop                 //
         && a.clipBottom == b.clipBottom           //
         && a.mirrorSouth == b.mirrorSouth         //
         && a.flat == b.flat                       //
         && a.flatElevation == b.flatElevation     //
         && a.spin == b.spin                       //
         && a.reachLfo == b.reachLfo               //
         && a.envelopeAttack == b.envelopeAttack   //
         && a.envelopeDecay == b.envelopeDecay     //
         && a.envelopeMax == b.envelopeMax         //
         && a.actMode == b.actMode                 //
         && a.direction == b.direction             //
         && a.endAction == b.endAction             //
         && a.fadeReach == b.fadeReach             //
         && a.bridgeBias == b.bridgeBias;
}

inline bool
operator!= (ClipSettings const &a, ClipSettings const &b)
{
  return !(a == b);
}

ClipSettings clipSettingsFrom (Pattern const &pattern);
void applyClipSettings (Pattern &pattern, ClipSettings const &settings);

}
