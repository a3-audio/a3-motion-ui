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
#include <a3-motion-engine/elevation/HeightMap.hh>

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
  /** How far the figure is squeezed or stretched along each horizontal axis,
   *  bipolar with the middle at zero -- see PlaneShaping in
   *  TrajectoryShaping.hh, which is where the two are read and what they do
   *  to a position. X is front-back, Y left-right.
   *
   *  Beside the rotate rather than in the elevation block: all three are
   *  transforms of the recorded figure in its own plane, applied on the way
   *  out and never written into the take. */
  float squeezeX = 0.f;
  float squeezeY = 0.f;
  /** Each squeeze's own sweep -- what swell is to reach. A signed TempoLfo
   *  step, the sign saying which way it travels: positive stretches the axis
   *  out and back, negative presses it flat and back. */
  int squeezeXLfo = 0;
  int squeezeYLfo = 0;

  float reach = 0.5f;
  float clipTop = 0.f;
  float clipBottom = 0.f;
  /** Where the middle of the trajectory sits, 0 at the north pole and 1 at
   *  the south. The line in the elevation graphic is this value, and the
   *  reach cone grows from it towards whichever pole is further away.
   *
   *  It replaces what `mirrorSouth` decided, which could only ever be one
   *  pole or the other. That flag is still carried so files written before
   *  this parse, and a clip that had it set loads as a base of 1 -- see
   *  ClipFile::load(). Nothing reads it. */
  float elevationBase = 0.f;
  bool mirrorSouth = false;
  bool flat = false;
  float flatElevation = 0.5f;

  int spin = 0;
  int reachLfo = 0;
  /** The elevation base's own sweep. What swell is to reach: a signed
   *  TempoLfo step in bars per cycle, sweeping where the trajectory's middle
   *  sits out of where it was set and back, towards whichever pole the sign
   *  points at.
   *
   *  It lives beside swell rather than in the elevation block because what it
   *  is, is a slow movement of the clip rather than a shape of it -- the same
   *  reason swell's own control sits with spin rather than with reach. */
  int elevationLfo = 0;
  int envelopeAttack = 2;
  int envelopeDecay = 3;
  float envelopeMax = 1.f;

  /** Two more envelopes, on the filter: one for the cutoff, one for the
   *  resonance, each with its own times and its own ceiling.
   *
   *  They were one set at first, on the reasoning that a resonant sweep is a
   *  single gesture. Heard on the device it is not: the resonance wants to
   *  arrive somewhere other than where the cutoff does, and one pair of times
   *  gives every sweep the same shape. `*Max` is how far each one goes, and
   *  zero -- the default -- is off. */
  int freqAttack = 2;
  int freqDecay = 3;
  float freqMax = 0.f;
  int qAttack = 2;
  int qDecay = 3;
  float qMax = 0.f;
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
         && a.squeezeX == b.squeezeX               //
         && a.squeezeY == b.squeezeY               //
         && a.squeezeXLfo == b.squeezeXLfo         //
         && a.squeezeYLfo == b.squeezeYLfo         //
         && a.reach == b.reach                     //
         && a.clipTop == b.clipTop                 //
         && a.clipBottom == b.clipBottom           //
         && a.elevationBase == b.elevationBase     //
         && a.mirrorSouth == b.mirrorSouth         //
         && a.flat == b.flat                       //
         && a.flatElevation == b.flatElevation     //
         && a.spin == b.spin                       //
         && a.reachLfo == b.reachLfo               //
         && a.elevationLfo == b.elevationLfo       //
         && a.envelopeAttack == b.envelopeAttack   //
         && a.envelopeDecay == b.envelopeDecay     //
         && a.envelopeMax == b.envelopeMax         //
         && a.freqAttack == b.freqAttack           //
         && a.freqDecay == b.freqDecay             //
         && a.freqMax == b.freqMax                 //
         && a.qAttack == b.qAttack                 //
         && a.qDecay == b.qDecay                   //
         && a.qMax == b.qMax                       //
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

/** What a fired action puts on the clip it is fired at.
 *
 *  An action is a whole ClipSettings, saved off a clip that was dialled the
 *  way ACT should make it sound. But it says *where* the clip is thrown, not
 *  *how*: the two envelopes' times and ceilings, and whether ACT is a stab or
 *  a hold, stay the slot's and are taken from `current`.
 *
 *  Two reasons for the seam being there. The ACTION page shows exactly those
 *  seven numbers per slot, and a page showing an attack that the running
 *  action had quietly replaced would be a page that lies. And `actMode` in
 *  particular is read once, when ACT goes down, to decide whether the clip
 *  belongs to the finger -- a mode that changed mid-gesture would leave a held
 *  clip running with nothing holding it.
 */
ClipSettings actionOver (ClipSettings const &current,
                         ClipSettings const &action);

ClipSettings clipSettingsFrom (Pattern const &pattern);
void applyClipSettings (Pattern &pattern, ClipSettings const &settings);

/** The elevation parameters a clip is actually projecting through right now:
 *  what it was set to, with its two slow sweeps laid over it.
 *
 *  One function because three places have to agree about this and could not
 *  be made to by hand. The engine sweeps before it projects; the renderer
 *  sweeps before it draws, in each of its two paths -- and a line drawn at a
 *  coverage the blob is not running at is a line that is simply wrong. Three
 *  copies of two lines each was already one too many when there was only
 *  swell; with sway beside it there would have been six.
 */
ElevationParams sweptElevation (ElevationParams params,
                                Pattern const &pattern);

}
