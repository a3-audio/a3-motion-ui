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

#include <cstdio>
#include <mutex>
#include <string>

namespace a3
{

/** A timeline of the beat as it passes through this device, for measuring.
 *
 *  Written to find out why A³ Motion falls out of time with the music, which
 *  cannot be settled by looking: the tempo from the beat-analyzer, when a
 *  /beat arrives, when it is handled, and when the engine's own clock beats
 *  are four different moments, and the question is how far apart they drift.
 *
 *  Every line is stamped on the monotonic clock, the one JACK stamps its
 *  frames in, so a click played in through JACK can be laid on the same axis.
 *  The header carries the offset to wall time for the beat-analyzer's journal.
 *
 *  Off unless given a path, and then it costs one formatted line per beat.
 *  Safe to call from any thread. */
class BeatTrace
{
public:
  explicit BeatTrace (std::string const &path);
  ~BeatTrace ();

  BeatTrace (BeatTrace const &) = delete;
  BeatTrace &operator= (BeatTrace const &) = delete;

  bool isEnabled () const;

  void record (char const *event, int beat, int bar, float bpm);

  /** The one the device uses, opened from $A3_BEAT_TRACE. */
  static BeatTrace &device ();

private:
  std::mutex _mutex;
  std::FILE *_file = nullptr;
};

}
