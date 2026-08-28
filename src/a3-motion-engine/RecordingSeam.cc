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

void
fillSpan (Pattern &pattern, UnwrittenSpan span, Pos const &before,
          Pos const &after, bool glide)
{
  auto const numTicks = pattern.getNumTicks ();

  for (index_t step = 0; step < span.length; ++step)
    {
      auto const tick = (span.begin + step) % numTicks;

      if (!glide)
        {
          pattern.setTick (tick, before);
          continue;
        }

      // +1 so the far end lands on `after` rather than one step short of it:
      // the span holds `length` ticks between two played ones.
      auto const t = static_cast<float> (step + 1)
                     / static_cast<float> (span.length + 1);
      pattern.setTick (tick, between (before, after, t));
    }
}
}

namespace
{
/** Close the loop point when both sides of it were played.
 *
 *  Recording in Loop runs several passes, so the last tick carries an early
 *  one and the first a late one. Nothing is missing there -- it is an edge
 *  between two written ticks -- and filling holes, which is all the seam ever
 *  did, has nothing to do with it. The blob snapped from the end back to the
 *  start.
 *
 *  The closing arc is written over the tail, and takes as many ticks as the
 *  trajectory's own speed needs to cover the distance: any shorter and it
 *  reads as a dash across the sphere, any longer and it overwrites more of the
 *  take than closing it costs. Recorded as the seam span, so switching the
 *  mode afterwards refills it like any other seam. */
void
closeLoopPoint (Pattern &pattern, SeamMode mode)
{
  auto const numTicks = pattern.getNumTicks ();
  if (numTicks < 4)
    return;

  auto const positions = pattern.getTicks ().positions;
  auto const last = pattern.getTick (numTicks - 1);
  auto const first = pattern.getTick (0);
  if (!last.isValid () || !first.isValid ())
    return;

  auto const step = typicalTrajectoryStep (positions);
  if (step <= 0.f)
    return;

  auto const gap = std::sqrt (std::pow (last.x () - first.x (), 2.f)
                              + std::pow (last.y () - first.y (), 2.f)
                              + std::pow (last.z () - first.z (), 2.f));
  if (gap <= trajectoryJumpThreshold (step))
    return; // it already comes round to where it started

  // A quarter of the loop is the most this may cost. Past that the take is
  // being replaced by its own closing move rather than closed.
  auto const needed = static_cast<index_t> (std::ceil (gap / step));
  auto const length = std::max (index_t{ 2 },
                                std::min (needed, numTicks / 4));

  // The span is recorded either way, so the mode stays a playback setting that
  // can be changed afterwards. Only Glide writes into it: unlike a stretch
  // nobody played, this one holds recorded motion, and holding it flat would
  // delete what the finger actually did to keep a jump that is already there.
  UnwrittenSpan const span{ numTicks - length, length };
  pattern.setSeamSpan (span);

  if (mode != SeamMode::Glide)
    return;

  fillSpan (pattern, span, pattern.getTick (span.begin - 1), first, true);
}
}

void
applySeamMode (Pattern &pattern, SeamMode mode)
{
  auto const span = pattern.getSeamSpan ();
  auto const numTicks = pattern.getNumTicks ();
  if (span.length == 0 || numTicks == 0)
    return;

  // From the two played positions at either end, never from the last fill —
  // otherwise switching back and forth would walk the seam somewhere else.
  auto const before = pattern.getTick ((span.begin + numTicks - 1) % numTicks);
  auto const after = pattern.getTick ((span.begin + span.length) % numTicks);

  fillSpan (pattern, span, before, after, mode == SeamMode::Glide);
  pattern.markComplete ();
}

void
closeRecordingSeams (Pattern &pattern, SeamMode mode)
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

      auto const glide = isSeam && mode == SeamMode::Glide;
      fillSpan (pattern, span, before, after, glide);
    }

  // A take that wrote across the loop point leaves no hole there, so the loop
  // above never saw it. That edge is a seam too.
  if (pattern.getSeamSpan ().length == 0)
    closeLoopPoint (pattern, mode);

  // Every tick holds something now, and playback has to be told: it reads the
  // last written tick as the pattern's length, and the span filled last is the
  // one across the loop point, which ends near the beginning.
  pattern.markComplete ();

  // Filling is not recording: the mask says what the finger wrote, and these
  // ticks were reasoned about, not performed.
  pattern.clearWrittenTicks ();
}

}
