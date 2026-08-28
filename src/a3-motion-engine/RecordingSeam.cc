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

namespace a3
{

namespace
{
Pos
between (Pos const &from, Pos const &to, float t)
{
  return Pos::fromCartesian (from.x () + (to.x () - from.x ()) * t,
                             from.y () + (to.y () - from.y ()) * t,
                             from.z () + (to.z () - from.z ()) * t);
}
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
      auto const glide = isSeam && mode == SeamMode::Glide;

      for (index_t step = 0; step < span.length; ++step)
        {
          auto const tick = (span.begin + step) % numTicks;

          if (!glide)
            {
              pattern.setTick (tick, before);
              continue;
            }

          // +1 so the far end lands on `after` rather than one step short of
          // it: the span holds `length` ticks between two written ones.
          auto const t = static_cast<float> (step + 1)
                         / static_cast<float> (span.length + 1);
          pattern.setTick (tick, between (before, after, t));
        }
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
