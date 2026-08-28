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

#include <string>

#include <a3-motion-engine/elevation/HeightMap.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/Types.hh>

namespace a3
{

class Pattern
{
public:
  enum class Status
  {
    Empty,
    ScheduledForIdle,
    Idle,
    ScheduledForRecording,
    Recording,
    ScheduledForPlaying,
    Playing,
  };

  Pattern ();

  void clear ();
  /** Length in **ticks**, not beats — callers pass lengthBeats * ppqn. The
   *  parameter was named lengthBeats here and lengthTicks in the definition,
   *  which is a name that lies in the place people read first. */
  void resize (index_t lengthTicks);

  void setStatus (Status status);
  Status getStatus () const;

  Status getLastStatus () const;
  void restoreStatus ();

  /** Returns true if this pattern has been in the Recording state
   *  at any point during its lifetime. */
  bool wasRecording () const;

  void setChannel (index_t channel);
  index_t getChannel () const;

  void setName (std::string name);
  std::string const &getName () const;

  index_t getNumTicks () const;
  Pos getTick (index_t tick) const;
  void setTick (index_t tick, Pos position);
  index_t getLastUpdatedTick () const;

  /** Which ticks this session's recording has written.
   *
   *  Punch-out needs to tell "never touched" from "touched, and holding a
   *  position that happens to look like nothing". Only setTick() marks; a
   *  resize starts the mask over, because it is about the recording in
   *  progress and not about what a file once held. */
  std::vector<bool> writtenTicks () const;
  bool isTickWritten (index_t tick) const;
  void clearWrittenTicks ();

  /** Say that every tick now holds something.
   *
   *  Playback reads getLastUpdatedTick() + 1 as the pattern's effective
   *  length — a leftover from when a take only ever filled a prefix and the
   *  rest was empty. Once the spans are filled that is no longer true, and a
   *  pattern that does not say so is played inside whatever fraction of
   *  itself was written last. */
  void markComplete ();

  // Interpolated playback: returns position with linear interpolation between keyframes
  // This provides smooth motion even with sparse keyframes during slow playback
  Pos getInterpolatedTick (double fractionalTick) const;

  // The Ticks struct enables us to atomically return the positions
  // together with the last updated value.
  struct Ticks
  {
    std::vector<Pos> positions;
    index_t lastUpdatedTick;
  };
  Ticks getTicks () const;

  Measure getPlaybackLength () const;
  void setPlaybackLength (Measure playbackLength);

  float getPlayPosition () const;
  void setPlayPosition (float playPosition);

  // Elevation is a per-clip property: each Pattern remembers its own
  // reach/mirrorSouth/clipTop/clipBottom/flat/flatElevation so playback/
  // recording/preview all read the same values regardless of which other
  // clip is currently being edited on the same channel. Strictly monotonic
  // in the recorded 2D radius r: r=0 is always the pole, r=1 is always
  // `reach`'s point (0.5 = hemisphere/equator, 1.0 = full sphere/opposite
  // pole) — see HeightMap::mapTo3D()'s ElevationParams overload for full
  // semantics, and ElevationParams itself for field-by-field docs.
  float getReach () const;
  void setReach (float reach); // clamped to [0.05, 1.0]

  bool getMirrorSouth () const;
  void setMirrorSouth (bool mirrorSouth);

  float getClipTop () const;
  void setClipTop (float clipTop); // clamped to [0.0, 1.0]

  float getClipBottom () const;
  void setClipBottom (float clipBottom); // clamped to [0.0, 1.0]

  bool getFlat () const;
  void setFlat (bool flat);

  float getFlatElevation () const;
  void setFlatElevation (float flatElevation); // clamped to [0.0, 1.0]

  /** Convenience bundle of the above, ready to pass to
   *  HeightMap::mapTo3D()/mapTo2D(). */
  ElevationParams getElevationParams () const;

private:
  static_assert (std::atomic<Status>::is_always_lock_free);
  std::atomic<Status> _status = Status::Empty;
  std::atomic<Status> _statusLast = Status::Empty;
  std::atomic<bool> _wasRecording{ false };

  // for now patterns are fixed to a channel, this will probably
  // change later on.
  std::atomic<index_t> _channel;

  std::string _name;

  index_t _lastUpdatedTick{ 0 };
  std::vector<Pos> _ticks;
  std::vector<bool> _written;
  mutable std::mutex _ticksMutex;

  // TODO is float precision sufficient here? do the math!
  static_assert (std::atomic<float>::is_always_lock_free);
  std::atomic<float> _playPosition = 0.;
  std::atomic<Measure> _playbackLength;

  std::atomic<float> _reach{ 0.5f };
  std::atomic<bool> _mirrorSouth{ false };
  std::atomic<float> _clipTop{ 0.0f };
  std::atomic<float> _clipBottom{ 0.0f };
  std::atomic<bool> _flat{ false };
  std::atomic<float> _flatElevation{ 0.5f };
};

}
