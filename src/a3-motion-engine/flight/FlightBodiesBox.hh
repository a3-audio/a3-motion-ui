/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <a3-motion-engine/flight/FlightField.hh>

#include <atomic>
#include <type_traits>

namespace a3
{

/** The bodies the DJ placed, handed from the message thread to the clock
 *  thread: the Channel position's SeqLock, for a whole FlightBodies.
 *
 *  Newest wins, with no queue: a body being dragged is written on every
 *  mouse event, and the clock thread only ever wants the latest floor.
 *
 *  Neither side waits. `write` runs on the message thread (one writer) and
 *  never blocks. `read` runs on the clock thread and does not retry: a copy
 *  caught half-written is refused, and the caller keeps last tick's, which is
 *  one tick old -- where Channel's reader spins until it gets a clean one. */
class FlightBodiesBox
{
  // A SeqLock copies the bytes and checks afterwards whether they were torn;
  // that is only sound for a type whose copy is a plain copy of its bytes.
  static_assert (std::is_trivially_copyable_v<FlightBodies>,
                 "FlightBodiesBox copies under a SeqLock");

public:
  /** One writer only: the message thread. Two writers could both see an
   *  even count, and the reader would take a mix of their bodies as clean. */
  void
  write (FlightBodies const &bodies)
  {
    beginWrite ();
    _bodies = bodies;
    endWrite ();
  }

  /** Copies the bodies into `out` and answers true, or answers false and
   *  leaves `out` alone when a write was under way. */
  bool
  read (FlightBodies &out) const
  {
    auto const before = _seqCount.load (std::memory_order_acquire);
    if (before & 1u)
      return false;
    FlightBodies const copy = _bodies;
    std::atomic_thread_fence (std::memory_order_acquire);
    if (_seqCount.load (std::memory_order_relaxed) != before)
      return false;
    out = copy;
    return true;
  }

  /** The two halves of `write`, public so a test can hold a write open and
   *  show that a read in the middle of it is refused. Nothing else calls
   *  them. */
  void
  beginWrite ()
  {
    auto const seq = _seqCount.load (std::memory_order_relaxed);
    _seqCount.store (seq + 1, std::memory_order_relaxed); // odd = writing
    // The odd count is seen before any of the bodies change.
    std::atomic_thread_fence (std::memory_order_release);
  }

  void
  endWrite ()
  {
    auto const seq = _seqCount.load (std::memory_order_relaxed);
    _seqCount.store (seq + 1, std::memory_order_release); // even = done
  }

private:
  FlightBodies _bodies{};
  std::atomic<unsigned> _seqCount{ 0 };
};

}
