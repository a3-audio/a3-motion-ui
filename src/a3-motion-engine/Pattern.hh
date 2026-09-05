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

#include <a3-motion-engine/TrajectoryBridges.hh>

#include <string>

#include <a3-motion-engine/elevation/HeightMap.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/Types.hh>
#include <a3-motion-engine/Playhead.hh>
#include <a3-motion-engine/RecordingSpans.hh>

#include <optional>

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

  /** Where this take's seam is — the stretch between the last thing played and
   *  the first, which nobody played.
   *
   *  Remembered rather than only filled, because how it is filled is a
   *  playback setting and not a property of the take: the positions at either
   *  end are real ticks, so it can be filled either way at any time. A length
   *  of zero means the take has no seam. */


  /** Where the take stopped, when it stopped anywhere: the last tick its
   *  freshest pass wrote, with the previous pass still sitting after it.
   *
   *  Kept apart from the seam span on purpose. A span is a hole with a played
   *  tick at each end, and its length is what it is; this is a single edge, and
   *  how long the closing move across it lasts is a setting that can be turned
   *  at any time. Sharing one field made turning it move the join, because the
   *  far end of a shortened span landed in the previous fill instead of on
   *  something somebody played. */


  /** The take as it was played, before any closing move was laid over it.
   *
   *  The fade used to be written straight into the ticks, which made it a
   *  one-way door: lengthening it read its far end from material nobody had
   *  touched yet and worked, shortening it read from the previous fill and
   *  changed nothing. Keeping what was played means the closing move can be
   *  recomputed at any length, including back to none at all. */


  /** How long the closing move currently laid over the take is, in ticks. */


  /** Which way the clip sets off, and what it does when it gets to the end.
   *
   *  Clip settings like the playback length and the fade, so they live here
   *  rather than in the UI's own table -- otherwise the engine cannot see them
   *  and they survive nothing. */
  PlayDirection getPlayDirection () const;
  void setPlayDirection (PlayDirection direction);
  EndAction getEndAction () const;
  void setEndAction (EndAction action);

  /** Whether the Action key fires this clip or holds it. Per clip, like the
   *  end action beside it: one slot can be a stab and its neighbour a cue. */
  ActMode getActMode () const;
  void setActMode (ActMode mode);

  /** How long one cycle takes, as a power of two of a bar. 0 is one bar,
   *  negative is faster, positive is slower -- see speedLog2Min/Max.
   *
   *  On the Pattern rather than in the UI's per-slot table, where it used to
   *  live: the engine reads it and it has to survive being saved. A clip's
   *  tempo is the clip's, not the screen's. */
  int getSpeedLog2 () const;
  void setSpeedLog2 (int speedLog2);

  /** How long the take's closing move lasts, in sixteenths of a beat. Zero
   *  holds and jumps instead of travelling back.
   *
   *  Sixteenths rather than ticks, which is what getFade() reports: ticks come
   *  out of the PPQN the take was written with, so the same number means
   *  something else on a device set up differently. */


  /** The filter envelopes, one for the cutoff and one for the resonance --
   *  each with its own times and ceiling. See ClipSettings::freqAttack. */
  int getFreqAttack () const;
  void setFreqAttack (int step);
  int getFreqDecay () const;
  void setFreqDecay (int step);
  float getFreqMax () const;
  void setFreqMax (float max);

  int getQAttack () const;
  void setQAttack (int step);
  int getQDecay () const;
  void setQDecay (int step);
  float getQMax () const;
  void setQMax (float max);

  /** How far a gap may be for the fade to draw through it, 0..1 of the
   *  sphere's diameter. See ClipSettings::fadeReach. */
  float getFadeReach () const;
  void setFadeReach (float reach);

  /** Where a bridged gap leads, -4..+4. See ClipSettings::bridgeBias. */
  int getBridgeBias () const;
  void setBridgeBias (int bias);

  /** Which of this take's gaps are drawn through, and where each one leads.
   *
   *  Playback and drawing both read this rather than each working out what a
   *  gap is: two independent answers drift apart, and then the sphere shows a
   *  line the blob does not run on. Recomputed when the ticks or either
   *  setting change, never per tick. */
  BridgePlan getBridgePlan () const;

  /** Which way the playhead is travelling right now. Set from the direction
   *  when playback starts; only Bounce ever turns it round. */
  float getPlaySign () const;
  void setPlaySign (float sign);

  /** How far the whole trajectory is turned around the vertical axis, in
   *  revolutions [0, 1). A standing angle, not a movement: where the shape
   *  faces.
   *
   *  Beside the spin because they are the same operation — the spin adds a
   *  turn that keeps growing, this one adds a turn that stays — and they are
   *  summed at the one place that turns anything (spinPosition). Two ways to
   *  rotate a trajectory would be two chances to disagree about which way
   *  round that is. */
  float getRotate () const;
  void setRotate (float revolutions);

  /** How fast the whole trajectory turns around the vertical axis while the
   *  blob runs along it, as the signed power-of-two step TrajectorySpin
   *  describes. Zero stands still.
   *
   *  A clip setting like the fade and the end action, and here for the same
   *  reason: the engine has to see it, and it has to survive being saved. */
  int getSpin () const;
  void setSpin (int step);

  /** How far round it has turned, in revolutions [0, 1). Advanced by the
   *  engine while the clip plays and read by the renderer, which has to draw
   *  the line in the same place the blob is running.
   *
   *  Reset when playback starts, so a clip fired again begins where it was
   *  recorded rather than wherever the last pass happened to leave it. */
  float getSpinPhase () const;
  void setSpinPhase (float phase);

  /** How fast `reach` sweeps out of where it was set and back, as a TempoLfo
   *  step. Zero holds it still.
   *
   *  The sign is which way out: positive opens the coverage towards the far
   *  pole, negative closes it towards the near one. The distance is whatever
   *  is left between the set reach and that end, so there is no setting at
   *  which the two controls conspire to do nothing. */
  int getReachLfo () const;
  void setReachLfo (int step);

  /** How far through that sweep it is, in cycles [0, 1). Advanced by the
   *  engine while the clip plays and read by the renderer, which has to draw
   *  the coverage the blob is actually running in. Reset with the spin's
   *  phase when playback starts. */
  float getReachLfoPhase () const;
  void setReachLfoPhase (float phase);

  /** The accent's shape: how long it takes to rise while ACT is held, and how
   *  long to fall once it is let go, as Envelope steps.
   *
   *  Per clip, like the spin and the swell, because the clip you fire brings
   *  its own accent. What it *drives* is the channel's 3d, whose set value it
   *  can only raise — see envelopeOver(). */
  int getEnvelopeAttack () const;
  void setEnvelopeAttack (int step);
  int getEnvelopeDecay () const;
  void setEnvelopeDecay (int step);

  /** How far the accent throws: the 3d it rises to while ACT is held. The
   *  channel's own 3d is the floor and this is the ceiling, so a clip carries
   *  how big its accent is rather than every accent going all the way up. */
  float getEnvelopeMax () const;
  void setEnvelopeMax (float value);

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

  /** Past which step from one tick to the next the motion is a jump rather
   *  than a movement. Worked out once when the ticks are finished, because
   *  playback asks on every tick and must not walk the whole pattern to find
   *  out. Zero means nothing is treated as a jump. */
  float _jumpThreshold = 0.f;
  /** Guarded by _ticksMutex; ensureBridgePlanLocked() expects it held. */
  mutable BridgePlan _bridgePlan;
  mutable bool _bridgePlanStale = true;
  void ensureBridgePlanLocked () const;
  void markBridgePlanStale ();

  std::atomic<int> _freqAttack{ 2 };
  std::atomic<int> _freqDecay{ 3 };
  std::atomic<float> _freqMax{ 0.f };
  std::atomic<int> _qAttack{ 2 };
  std::atomic<int> _qDecay{ 3 };
  std::atomic<float> _qMax{ 0.f };
  std::atomic<float> _fadeReach{ 0.25f };
  std::atomic<int> _bridgeBias{ 0 };
  std::atomic<PlayDirection> _playDirection{ PlayDirection::Forward };
  std::atomic<EndAction> _endAction{ EndAction::Loop };
  std::atomic<ActMode> _actMode{ ActMode::OneShot };
  std::atomic<int> _speedLog2{ 0 };
  std::atomic<float> _playSign{ 1.f };
  std::atomic<float> _rotate{ 0.f };
  std::atomic<int> _spin{ 0 };
  std::atomic<float> _spinPhase{ 0.f };
  std::atomic<int> _reachLfo{ 0 };
  std::atomic<float> _reachLfoPhase{ 0.f };
  std::atomic<int> _envelopeAttack{ 2 };
  std::atomic<int> _envelopeDecay{ 3 };
  std::atomic<float> _envelopeMax{ 1.f };
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
