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

#include "Channel.hh"

#include <JuceHeader.h>

namespace a3
{

Channel::Channel () : _position (Pos::fromSpherical (0, 0, 1)) {}

void
Channel::setPosition (Pos position)
{
  // SeqLock write — increment to odd (write in progress), store, increment to even (done).
  // This is lock-free: the writer (RT thread) never blocks.
  auto seq = _seqCount.load (std::memory_order_relaxed);
  _seqCount.store (seq + 1, std::memory_order_release);  // odd = writing
  _position = position;
  _seqCount.store (seq + 2, std::memory_order_release);  // even = done
}

Pos
Channel::getPosition () const
{
  // SeqLock read — retry if we caught a torn write.
  // Typically completes on first try (< 1 µs).
  Pos pos;
  unsigned seq0, seq1;
  do
    {
      seq0 = _seqCount.load (std::memory_order_acquire);
      pos = _position;
      seq1 = _seqCount.load (std::memory_order_acquire);
    }
  while (seq0 != seq1 || (seq0 & 1));  // retry if odd or changed
  return pos;
}

float
Channel::getPot1 () const
{
  return _pot1;
}

void
Channel::setPot1 (float pot1)
{
  _pot1 = pot1;
}

float
Channel::getPot2 () const
{
  return _pot2;
}

void
Channel::setPot2 (float pot2)
{
  _pot2 = pot2;
}

float
Channel::getPot3 () const
{
  return _pot3;
}

void
Channel::setPot3 (float pot3)
{
  _pot3 = pot3;
}

}
