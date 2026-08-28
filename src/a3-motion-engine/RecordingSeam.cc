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

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
float
length (Pos const &p)
{
  return std::sqrt (p.x () * p.x () + p.y () * p.y () + p.z () * p.z ());
}

Pos
straightBetween (Pos const &from, Pos const &to, float t)
{
  return Pos::fromCartesian (from.x () + (to.x () - from.x ()) * t,
                             from.y () + (to.y () - from.y ()) * t,
                             from.z () + (to.z () - from.z ()) * t);
}

/** A glide from one recorded position to another.
 *
 *  Recorded positions are directions on the unit sphere, and a straight line
 *  between two of them runs through the sphere rather than across it: halfway
 *  between two a quarter turn apart the vector is 0.707 long, and over a long
 *  seam between distant points it dives towards the centre and back out. The
 *  blob leaves the surface and swings.
 *
 *  So the direction turns and the distance from the centre is carried across
 *  separately. Degenerate ends — a zero-length position, or two exactly
 *  opposite ones, where no one arc is the way round — have nothing to turn
 *  about, and fall back to the straight line. */
Pos
between (Pos const &from, Pos const &to, float t)
{
  auto const fromLength = length (from);
  auto const toLength = length (to);

  constexpr float epsilon = 1e-6f;
  if (fromLength < epsilon || toLength < epsilon)
    return straightBetween (from, to, t);

  auto const dot = (from.x () * to.x () + from.y () * to.y ()
                    + from.z () * to.z ())
                   / (fromLength * toLength);
  auto const angle = std::acos (std::max (-1.f, std::min (1.f, dot)));
  auto const sinAngle = std::sin (angle);

  if (sinAngle < epsilon)
    return straightBetween (from, to, t);

  auto const fromWeight = std::sin ((1.f - t) * angle) / sinAngle;
  auto const toWeight = std::sin (t * angle) / sinAngle;

  // Turn the unit directions, then put back the distance from the centre the
  // two ends had. For positions on the sphere that distance is 1 at both ends
  // and stays 1 all the way across.
  auto const radius = fromLength + (toLength - fromLength) * t;

  return Pos::fromCartesian (
      (from.x () / fromLength * fromWeight + to.x () / toLength * toWeight)
          * radius,
      (from.y () / fromLength * fromWeight + to.y () / toLength * toWeight)
          * radius,
      (from.z () / fromLength * fromWeight + to.z () / toLength * toWeight)
          * radius);
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

  // Every tick holds something now, and playback has to be told: it reads the
  // last written tick as the pattern's length, and the span filled last is the
  // one across the loop point, which ends near the beginning.
  pattern.markComplete ();

  // Filling is not recording: the mask says what the finger wrote, and these
  // ticks were reasoned about, not performed.
  pattern.clearWrittenTicks ();
}

}
