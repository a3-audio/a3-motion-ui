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

#include "RecordingSeam.hh"

#include <a3-motion-engine/RecordingSpans.hh>
#include <a3-motion-engine/TrajectoryShape.hh>

#include <algorithm>
#include <cmath>
#include <vector>

namespace a3
{

namespace
{
/** A glide from one recorded position to another.
 *
 *  Straight, and deliberately so. A pattern stores 2D pattern-space positions
 *  — mapTo2D, with the radius carrying the colatitude — and playback lerps
 *  between neighbouring ticks in that same space. A fill that took any other
 *  route would not match the motion either side of it.
 *
 *  Turning this into an arc on the sphere was a mistake: the pattern-space
 *  origin is the zenith, so arcing about it swings the blob over the pole. */
Pos
between (Pos const &from, Pos const &to, float t)
{
  return Pos::fromCartesian (from.x () + (to.x () - from.x ()) * t,
                             from.y () + (to.y () - from.y ()) * t,
                             from.z () + (to.z () - from.z ()) * t);
}

/** Fill `span`, holding at `before` and then travelling to `after` over the
 *  last `fadeTicks` of it.
 *
 *  Holding first and travelling last is what "time taken at the end" means:
 *  the motion carries on standing where it was left until the closing move
 *  begins. A fade of zero holds the whole span and jumps; a fade at least as
 *  long as the span travels the whole way. Either end is a tick somebody
 *  played, so this can be redone at any length without the join drifting. */
void
fillSpan (Pattern &pattern, UnwrittenSpan span, Pos const &before,
          Pos const &after, index_t fadeTicks)
{
  auto const numTicks = pattern.getNumTicks ();

  for (index_t step = 0; step < span.length; ++step)
    {
      auto const tick = (span.begin + step) % numTicks;
      auto const remaining = span.length - step;

      if (remaining > fadeTicks)
        {
          pattern.setTick (tick, before);
          continue;
        }

      // +1 so the far end lands on `after` rather than one step short of it:
      // the span holds `length` ticks between two played ones.
      auto const t = static_cast<float> (fadeTicks - remaining + 1)
                     / static_cast<float> (fadeTicks + 1);
      pattern.setTick (tick, between (before, after, t));
    }
}
}

namespace
{
/** How many ticks the closing move may spend.
 *
 *  It is written over what follows the join, so it may only spend the stale
 *  pass sitting there -- the ticks up to the join are the freshest pass, and a
 *  move long enough to wrap round into them would close the join by deleting
 *  the take. A guard, unlike the length itself, which is the listener's.
 *
 *  A take that stopped on its very last tick has no stale pass at all: its
 *  join is the loop point, and closing it means trimming the beginning
 *  instead. That is allowed, but sparingly -- an eighth of the loop -- because
 *  what sits there was played too. */

/** Close the take's own join: the tick where it stopped.
 *
 *  Recording in Loop runs several passes, so the last tick carries an early
 *  one and the first a late one. Nothing is missing there -- it is an edge
 *  between two written ticks -- and filling holes, which is all the seam ever
 *  did, has nothing to do with it. The blob snapped from the end back to the
 *  start.
 *
 *  The closing move lasts as long as the fade says and is written over the
 *  stale pass behind the join, never back into the pass just played. Where the
 *  join is travels with the pattern, so the length can be turned afterwards
 *  without recording again. */
/** The closing move from `from` to `to`, leaving and arriving along the motion
 *  either side of it.
 *
 *  A straight line was visibly a straight line: two chords laid across a take
 *  that has none. A cubic that starts along `outgoing` and ends along
 *  `incoming` continues what was being played instead of cutting across it. */
Pos
closingCurve (Pos const &from, Pos const &to, Pos const &outgoing,
              Pos const &incoming, float t)
{
  auto const h00 = 2.f * t * t * t - 3.f * t * t + 1.f;
  auto const h10 = t * t * t - 2.f * t * t + t;
  auto const h01 = -2.f * t * t * t + 3.f * t * t;
  auto const h11 = t * t * t - t * t;

  return Pos::fromCartesian (
      h00 * from.x () + h10 * outgoing.x () + h01 * to.x () + h11 * incoming.x (),
      h00 * from.y () + h10 * outgoing.y () + h01 * to.y () + h11 * incoming.y (),
      h00 * from.z () + h10 * outgoing.z () + h01 * to.z () + h11 * incoming.z ());
}

/** Lay a closing move over `span`, from the tick before it to the tick after.
 *
 *  Both ends and both directions are read from `baseline` -- the take as it was
 *  played -- never from the pattern, which may already carry an earlier
 *  closing move. That is what makes the length free to change in either
 *  direction. */
/** The closing move sampled finely, with the distance travelled to each
 *  sample. Shared by writing it and by asking how long it wants to be. */
struct SampledCurve
{
  std::vector<Pos> point;
  std::vector<float> arcLength;
};

SampledCurve
sampleClosingMove (std::vector<Pos> const &baseline, UnwrittenSpan span)
{
  SampledCurve curve;

  auto const n = static_cast<index_t> (baseline.size ());
  if (n < 4 || span.length == 0)
    return curve;

  auto const at = [&baseline, n] (index_t tick) { return baseline[tick % n]; };

  auto const from = at (span.begin + n - 1);
  auto const to = at (span.begin + span.length);

  // How the motion was travelling as it arrived at the join, and how it is
  // travelling where the take picks up again -- measured over a window, never
  // over one tick. Ticks run far faster than a finger reports, so the tick
  // before the join usually repeats the one before it and a one-tick
  // difference is zero. Two zero tangents turn the curve into exactly what it
  // was meant to replace: a straight line between the two ends.
  auto const window = std::max (index_t{ 8 }, span.length / 8);

  auto const direction = [&at, window] (index_t head, index_t tail) {
    auto const a = at (head);
    auto const b = at (tail);
    return Pos::fromCartesian (a.x () - b.x (), a.y () - b.y (),
                               a.z () - b.z ());
  };

  // Only the direction comes from the motion; how far the curve reaches along
  // it comes from the distance it has to cover.
  //
  // Scaling the tangent by the take's speed across the span looked right on
  // paper and was a spike on screen: at 128 ticks and a speed of 0.06 that is
  // a tangent 7.7 long against a gap of one or two, so the curve shot far out
  // and turned back in a hairpin. A tangent about as long as the chord leaves
  // along the motion and still heads for where it is going.
  auto const chord = std::sqrt (std::pow (to.x () - from.x (), 2.f)
                                + std::pow (to.y () - from.y (), 2.f)
                                + std::pow (to.z () - from.z (), 2.f));

  auto const along = [chord] (Pos const &v) {
    auto const length
        = std::sqrt (v.x () * v.x () + v.y () * v.y () + v.z () * v.z ());
    if (length < 1e-6f)
      return Pos::fromCartesian (0.f, 0.f, 0.f);
    auto const scale = chord / length;
    return Pos::fromCartesian (v.x () * scale, v.y () * scale,
                               v.z () * scale);
  };

  auto const outgoing
      = along (direction (span.begin + n - 1, span.begin + n - 1 - window));
  auto const incoming = along (
      direction (span.begin + span.length + window, span.begin + span.length));

  // Ticks are laid along the curve by distance, not by parameter.
  //
  // A cubic does not run at a constant rate over t: equal steps in t are
  // unequal steps in space, much longer in the middle than at the ends. Placed
  // by t the blob crawled out of the join, shot through the middle faster than
  // it had ever moved, and crawled in again. Walking the curve by arc length
  // gives every tick the same distance, which is what "at the take's own
  // speed" has to mean.
  auto constexpr samplesPerTick = 8;
  auto const numSamples
      = static_cast<int> (span.length) * samplesPerTick;

  auto &arcLength = curve.arcLength;
  auto &sample = curve.point;
  arcLength.assign (static_cast<size_t> (numSamples) + 1, 0.f);
  sample.assign (static_cast<size_t> (numSamples) + 1, Pos::invalid);

  for (int i = 0; i <= numSamples; ++i)
    {
      auto const t = static_cast<float> (i) / static_cast<float> (numSamples);
      sample[static_cast<size_t> (i)]
          = closingCurve (from, to, outgoing, incoming, t);
      if (i > 0)
        {
          auto const &a = sample[static_cast<size_t> (i - 1)];
          auto const &b = sample[static_cast<size_t> (i)];
          arcLength[static_cast<size_t> (i)]
              = arcLength[static_cast<size_t> (i - 1)]
                + std::sqrt (std::pow (b.x () - a.x (), 2.f)
                             + std::pow (b.y () - a.y (), 2.f)
                             + std::pow (b.z () - a.z (), 2.f));
        }
    }

  return curve;
}


}



void
closeRecordingSeams (Pattern &pattern,
                     std::optional<index_t> stopTick)
{
  auto const numTicks = pattern.getNumTicks ();
  if (numTicks == 0)
    return;

  auto const spans = unwrittenSpans (pattern.writtenTicks ());

  // Nothing written at all: one span covering everything, and no position to
  // interpolate between. A take like that is discarded by the caller rather
  // than filled with a guess.
  if (spans.size () == 1 && spans.front ().length == numTicks)
    return;

  for (auto const &span : spans)
    {
      // Ring arithmetic: the tick before the span and the one after it, both
      // written by definition, are what the span is filled from.
      auto const before = pattern.getTick ((span.begin + numTicks - 1)
                                           % numTicks);
      auto const after
          = pattern.getTick ((span.begin + span.length) % numTicks);

      // Every span is held, the take's own seam included. It used to travel
      // back to the start over a computed length; the seam is a gap like any
      // other now, and whether it is drawn through is the fade's business at
      // playback -- a reading of the movement rather than a change to it.
      fillSpan (pattern, span, before, after, index_t{ 0 });
    }



  // Every tick holds something now, and playback has to be told: it reads the
  // last written tick as the pattern's length, and the span filled last is the
  // one across the loop point, which ends near the beginning.
  pattern.markComplete ();

  // Filling is not recording: the mask says what the finger wrote, and these
  // ticks were reasoned about, not performed.
  pattern.clearWrittenTicks ();
}

}
