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
index_t
closingLength (index_t fadeTicks, index_t numTicks, index_t seamAt)
{
  auto const stale = numTicks - 1 - seamAt;
  auto const room = stale > 0 ? std::min (stale, numTicks / 2) : numTicks / 8;
  return std::min (fadeTicks, room);
}

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
void
closeLoopPoint (Pattern &pattern, index_t fadeTicks, index_t seamAt)
{
  auto const numTicks = pattern.getNumTicks ();
  if (numTicks < 4)
    return;

  auto const positions = pattern.getTicks ().positions;
  // The edge to close: from the last tick of the freshest pass to whatever
  // follows it round the ring.
  auto const last = pattern.getTick (seamAt % numTicks);
  auto const first = pattern.getTick ((seamAt + 1) % numTicks);

  if (!last.isValid () || !first.isValid ())
    return;

  // Where the take stopped travels with the pattern whatever the fade is, so
  // the length can be turned later without recording again.
  pattern.setSeamJoin (seamAt % numTicks);

  if (fadeTicks == 0)
    return; // held and jumped: the take keeps its ending exactly as played

  // The join is only worth closing if it breaks: a take that already comes
  // round to where it started must keep its ending.
  auto const speed = typicalTrajectorySpeed (positions);
  auto const gap = std::sqrt (std::pow (last.x () - first.x (), 2.f)
                              + std::pow (last.y () - first.y (), 2.f)
                              + std::pow (last.z () - first.z (), 2.f));
  if (speed > 0.f && gap <= trajectoryJumpThreshold (speed))
    return;

  // The closing move may only spend the stale pass behind the join. The ticks
  // up to it are the freshest pass -- what was just played -- and a fade long
  // enough to wrap round into them would close the join by deleting the take.
  // Not a matter of taste like the length itself: a guard.
  auto const length = closingLength (fadeTicks, numTicks, seamAt % numTicks);
  UnwrittenSpan const span{ (seamAt + 1) % numTicks, length };

  // Written over the stale pass that follows the join, never over the fresh
  // one before it: the pass just played is the one worth keeping.
  auto const after = pattern.getTick ((span.begin + span.length) % numTicks);
  fillSpan (pattern, span, last, after, length);
}
}

void
applyFade (Pattern &pattern, index_t fadeTicks)
{
  auto const numTicks = pattern.getNumTicks ();
  if (numTicks == 0)
    return;

  // A take's join: the length is the fade, and both ends are ticks somebody
  // played, so this can be turned as often as you like without drifting.
  if (auto const join = pattern.getSeamJoin ())
    {
      auto const seamAt = *join % numTicks;
      auto const budget = closingLength (fadeTicks, numTicks, seamAt);

      if (budget > 0)
        {
          UnwrittenSpan const span{ (seamAt + 1) % numTicks, budget };
          auto const before = pattern.getTick (seamAt);
          auto const after
              = pattern.getTick ((span.begin + span.length) % numTicks);
          fillSpan (pattern, span, before, after, budget);
        }

      pattern.markComplete ();
      return;
    }

  // A hole across the loop point: its length is the hole's, and the fade says
  // how much of it is spent travelling rather than standing still.
  auto const span = pattern.getSeamSpan ();
  if (span.length == 0)
    return;

  // From the two played positions at either end, never from the last fill --
  // otherwise turning it repeatedly would walk the seam somewhere else.
  auto const before = pattern.getTick ((span.begin + numTicks - 1) % numTicks);
  auto const after = pattern.getTick ((span.begin + span.length) % numTicks);

  fillSpan (pattern, span, before, after, fadeTicks);
  pattern.markComplete ();
}

void
closeRecordingSeams (Pattern &pattern, index_t fadeTicks,
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

      // Only the stretch across the loop point is the take's own seam — the
      // place where it happens to have started and stopped. Everything else is
      // a finger that lifted on purpose, and smoothing that would erase a jump
      // somebody played.
      auto const isSeam = span.begin + span.length > numTicks;
      if (isSeam)
        pattern.setSeamSpan (span);

      // Only the stretch across the loop point spends the fade; a hole in
      // the middle is a jump somebody played and is always held.
      fillSpan (pattern, span, before, after, isSeam ? fadeTicks : index_t{ 0 });
    }

  // A take that wrote across the loop point leaves no hole there, so the loop
  // above never saw it. That edge is a seam too.
  if (pattern.getSeamSpan ().length == 0)
    closeLoopPoint (pattern, fadeTicks,
                    stopTick.value_or (numTicks - 1) % numTicks);

  // Every tick holds something now, and playback has to be told: it reads the
  // last written tick as the pattern's length, and the span filled last is the
  // one across the loop point, which ends near the beginning.
  pattern.markComplete ();

  // Filling is not recording: the mask says what the finger wrote, and these
  // ticks were reasoned about, not performed.
  pattern.clearWrittenTicks ();
}

}
