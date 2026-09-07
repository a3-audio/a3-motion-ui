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

#include <atomic>
#include <memory>

#include <a3-motion-engine/Measure.hh>
#include <a3-motion-engine/util/Types.hh>

namespace a3
{

class Pattern;

class Channel
{
public:
  Channel ();

  Pos getPosition () const;
  void setPosition (Pos position);

  float getPot1 () const;
  void setPot1 (float pot1);

  float getPot2 () const;
  void setPot2 (float pot2);

  /** The third value, driven by the channel's pot. What it means is Core's
   *  business; here it is a number between 0 and 1 like the other two. */
  float getPot3 () const;
  void setPot3 (float pot3);

private:
  // TODO reconsider: we want to keep the public API for users of the
  // MotionEngine so that internal state can not be messed with. Can
  // we design this cleaner without exposing _all_ internals to
  // MotionEngine as a friend?
  friend class MotionEngine;
  std::shared_ptr<Pattern> _patternScheduledForPlaying;
  std::shared_ptr<Pattern> _patternPlaying;
  Measure _playingStarted;

  Pos _position;
  std::atomic<float> _pot1 = 0.25f;
  std::atomic<float> _pot2 = 1.f;
  std::atomic<float> _pot3 = 0.f;

  // SeqLock for lock-free position access.
  // Writer (RT timer thread) never blocks.
  // Reader (GL/UI thread) retries on torn read — always consistent, never blocks writer.
  mutable std::atomic<unsigned> _seqCount{ 0 };
};

}
