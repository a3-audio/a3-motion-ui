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

#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/Envelope.hh>
#include <a3-motion-engine/RecMode.hh>
#include <a3-motion-engine/AsyncCommandQueue.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/Helpers.hh>

#include <atomic>
#include <optional>

namespace a3
{

class Channel;
class Pattern;
class HeightMap;

class MotionEngine
{
public:
  MotionEngine (index_t numChannels, HeightMap &heightMap);

  /** The same engine, sending through `backend` instead of to A3 Core. The
   *  app with its own audio engine hands in a SpatBackendInternal here. */
  MotionEngine (index_t numChannels, HeightMap &heightMap,
                std::unique_ptr<SpatBackend> backend);

  /** New OSC addresses, e.g. after config.json was edited on the device.
   *  Reaches the backend on its own thread — see AsyncCommandQueue. */
  void setOscAddresses (OscAddresses const &addresses);
  ~MotionEngine ();

  TempoClock const &getTempoClock () const;
  TempoClock &getTempoClock ();

  // Tempo facade: forwards to the internal TempoClock so callers (in
  // particular the UI layer) never need to reach into TempoClock directly.
  TempoClock::TapResult tap (juce::int64 timeMicros);
  float getTempoBPM () const;
  void setTempoBPM (float bpm);
  int getBeatsPerBar () const;
  void resetTempo ();

  // TODO refactor to access channels directly
  index_t getNumChannels ();

  Pos getChannelPosition (index_t channel);
  void setChannel2DPosition (index_t channel, Pos const &position);
  void setChannel3DPosition (index_t channel, Pos const &position);

  /** Hold a channel's position where it was put, so playback leaves it
   *  alone until it is let go.
   *
   *  Both a drag and playback write the same channel position, and playback
   *  writes it on every tick — so dragging a blob on a channel with a clip
   *  running was a tug of war the finger could not win. */
  void setChannelPositionHeld (index_t channel, bool held);
  bool isChannelPositionHeld (index_t channel) const;

  float getChannelPot1 (index_t channel);
  void setChannelPot1 (index_t channel, float pot1);

  float getChannelPot2 (index_t channel);
  void setChannelPot2 (index_t channel, float pot2);

  float getChannelPot3 (index_t channel);
  void setChannelPot3 (index_t channel, float pot3);

  /** What actually goes out: the set value with the accent laid over it. The
   *  grid shows this rather than the setting, so an envelope you fired is an
   *  envelope you can see — a modulation nothing on screen moves for is one
   *  you have to take on trust. */
  float getChannelPot3Effective (index_t channel);

  /** The same for the two filter values, each off an envelope of its own:
   *  pot1 is the cutoff, pot2 the resonance, and they have separate times and
   *  separate ceilings because a sweep whose resonance had to arrive exactly
   *  when its cutoff does only has one shape.
   *
   *  Both only ever raise, like pot3 -- so a ceiling under where the encoder
   *  already stands leaves it alone. Q sits at the top of its range by
   *  default, which is why its envelope is silent until Q is turned down. */
  float getChannelPot1Effective (index_t channel);
  float getChannelPot2Effective (index_t channel);

  /** ACT went down or came up on this channel. The accent rises while it is
   *  down and falls when it is let go; its shape comes from the clip that was
   *  fired, and it can only ever raise the channel's 3d above what the pot
   *  and the grid set — see envelopeOver(). */
  void setChannelAccentHeld (index_t channel, bool held,
                             std::shared_ptr<Pattern> pattern);

  /** What ACT throws this channel's clip *to*, for the length of the accent.
   *
   *  Set just before the press, or cleared with an empty optional when the
   *  slot has no action on it. The clip's settings as they stand are taken
   *  down first and put back when the envelope has finished falling — not
   *  when the finger lifts, which for a hold is the middle of an audible
   *  decay and the worst moment to snap a trajectory back.
   *
   *  Comes in ready to use rather than as a file: the settings are read off
   *  disk when the action is assigned, because the thread that fires this is
   *  the one that must never touch a disk. */
  void setChannelAction (index_t channel,
                         std::optional<ClipSettings> action);

  /** Whether anything about this channel is still moving on its own: the
   *  accent's level, or a clip still wearing the action fired at it.
   *
   *  The screen asks so that it keeps redrawing while neither the transport
   *  nor a hand is doing anything. Without it, a clip an action stopped would
   *  keep showing the action's settings for good -- the values came home and
   *  nothing was left running to notice. */
  bool isChannelAccentActive (index_t channel) const;

private:
  /** One tick of every channel's accent. Runs on the tempo-clock thread with
   *  the rest of playback, so the envelope and the trajectory move together.
   */
  void advanceAccents ();

  /** The accent has finished falling: the clip does what its end action says.
   *  Stop and Pause end the pass; the ones that mean "keep going" keep going,
   *  or the accent would be a stop button that only some settings noticed. */
  void applyEndActionAfterAccent (index_t channel);

  /** The accent has finished falling: the clip goes back to what it was
   *  before the action was fired at it. After the end action, which the
   *  action is entitled to have brought with it. */
  void restoreAfterAction (index_t channel);

public:

  enum class RecordingMode
  {
    Loop,
    OneShot
  };

  // Recording
  void setRecording2DPosition (Pos const &position);
  void setRecording3DPosition (Pos const &position);
  void releaseRecordingPosition ();

  void setRecordingMode (RecordingMode recordingMode);

  /** What a take's passes write where the finger is not.
   *
   *  Set straight rather than through the command FIFO: the FIFO is there to
   *  order commands against the clock, and this is a preference chosen in a
   *  menu between takes, with nothing to order it against. */
  void setRecMode (RecMode mode);
  RecMode getRecMode () const;
  RecordingMode getRecordingMode () const;

  bool isRecording () const;

  /** Whether a take owns the recording finger — running, or armed and still
   *  waiting for its downbeat.
   *
   *  The input path has to ask this rather than isRecording(): between the
   *  Record press and the downbeat the finger already belongs to the take,
   *  and treating it as an ordinary grab in that gap means it only catches a
   *  blob it lands near, while the outgoing clip keeps pulling that blob
   *  away from it. */
  bool isRecordingOrScheduled () const;

  /** How far the running take has got, from 0 to 1, or -1 when none is
   *  running or one is still waiting for its downbeat.
   *
   *  A take does not go through updatePlayPosition — that is for patterns
   *  being played — so a pattern's play position stays at zero throughout,
   *  and asking it where the write head is gives the wrong answer. */
  float getRecordingProgress () const;
  std::shared_ptr<Pattern> getRecordingPattern ();
  std::shared_ptr<Pattern> getScheduledForRecordingPattern ();

  void recordPattern (std::shared_ptr<Pattern> pattern, //
                      Measure timepoint, Measure length);

  // Playback
  std::shared_ptr<Pattern> getPlayingPattern (index_t channel);
  void playPattern (std::shared_ptr<Pattern> pattern, Measure timepoint);

  // Stop
  void stopPattern (std::shared_ptr<Pattern> pattern, Measure timepoint);

  /** Finish the lap, then stop -- whatever the end action says.
   *
   *  What pressing play on a running clip means. Not a timepoint, because the
   *  moment is not a beat somebody can name in advance: it is wherever this
   *  pass runs out, which depends on the playback length the clip is being
   *  played at. See advancePlayhead(). */
  void stopPatternAtEnd (std::shared_ptr<Pattern> pattern);

  /** Take back a start that has not happened yet.
   *
   *  Not a stop scheduled on top of it, which is what this used to be: `stop()`
   *  sets the pattern Idle but leaves `_patternScheduledForPlaying` pointing at
   *  it, and `startPlaying()` asks only that question -- so the clip started
   *  anyway a moment later. It looked right only because both messages carried
   *  the *same* timepoint and the queue happened to pop them in the forgiving
   *  order. With the start moved to the downbeat the cancel lands first, and
   *  the bug would have become the normal case.
   *
   *  Immediate, and deliberately: taking back a press is not a musical event.
   */
  void cancelScheduledPlay (std::shared_ptr<Pattern> pattern);

  /** Whether that has been asked for and has not happened yet.
   *
   *  The clip goes on playing meanwhile -- the status stays Playing, because
   *  it *is* playing -- so this is the only way a screen can tell that a press
   *  was taken. It is what makes the play key pulse instead of sitting there
   *  lit as though nothing had been asked. */
  bool isStoppingAtEnd (index_t channel) const;

  // Preview mode: suppress OSC output for a channel while pattern plays
  void setPreviewMode (index_t channel, bool enabled);
  bool isPreviewMode (index_t channel) const;

  /** Keep this device's own per-channel output in until
   *  `millisecondCounter` — position, both pots and the crossfade.
   *
   *  For the gap between start-up and the answer to /state/recall. The send
   *  loop announces every channel's values as soon as the engine runs, A3
   *  Core acts on them at once, and the room hears this device's idea of the
   *  mix before anyone has asked what it actually was. Core then answers with
   *  what it was just told, which confirms the jump rather than preventing it
   *  — measured 2026-09-12, see
   *  issues/a3-motion-ui-recall-kommt-zu-spaet.md.
   *
   *  It holds the **output**, not the values: anything arriving from Core
   *  during the hold reaches the blob and the knobs as usual. Only this
   *  side's announcements wait.
   *
   *  A deadline rather than a flag somebody has to clear. The failure mode of
   *  a flag is a device that never sends again, which is worse than the jump
   *  it was meant to stop; this one runs out on its own. Given as an absolute
   *  `juce::Time::getMillisecondCounterHiRes()` value rather than a duration
   *  so that a caller — a test especially — can name a deadline that has
   *  already passed without sleeping through it. */
  void holdOutputUntil (double millisecondCounter);
  bool outputHeld () const;

  // Access the HeightMap (for re-applying coverage to loaded patterns)
  HeightMap const &getHeightMap () const { return _heightMap; }

  class PatternStatusMessage : public juce::Message
  {
  public:
    enum class Status
    {
      Recording,
      Playing,
      Stopped,
    } status;
    std::shared_ptr<Pattern> pattern;
  };
  void addPatternStatusListener (juce::MessageListener *listener);
  void removePatternStatusListener (juce::MessageListener *listener);

private:
  void createChannels (index_t numChannels);
  std::vector<std::unique_ptr<Channel> > _channels;
  HeightMap &_heightMap;

  // MotionEngine runs the record/playback engine, checks for changed
  // parameters and and schedules corresponding commands with the
  // dispatcher.
  void tickCallback ();

  // The tempo clock is the main timing engine that runs at a 'tick'
  // resolution relative to the current metrum. Callbacks for metrum
  // events (tick, beat, bar) can be registered to be called either
  // from within the high-priority thread, or the main JUCE event
  // thread.
  TempoClock _tempoClock;
  TempoClock::PointerT _callbackHandleTick;

  // This is designed like a union currently, where not all fields are
  // valid for all message types. TODO: make this more explicit and
  // safe by using std::variant.
  struct Message
  {
    enum class Command
    {
      SetRecordingPosition,
      ReleaseRecordingPosition,
      SetRecordingMode,
      StartRecording,
      StartPlaying,
      Stop,
      StopAtEnd,
      CancelScheduledPlay,
      /** A pad went down or came up. Queued like everything else rather than
       *  written where it was pressed: the accent state is the clock
       *  thread's, and it used to be written from the message thread while
       *  the clock thread read and cleared it. See
       *  issues/a3-motion-ui-der-oneshot-faellt-unter-last-nicht-zurueck.md.
       */
      SetAccentHeld,
      /** What ACT does to a slot. Same reason. */
      SetChannelAction,
    } command;

    Pos position;
    Pos position2D;  // original 2D position (for recording ticks)
    std::shared_ptr<Pattern> pattern;
    Measure timepoint;
    Measure length;

    RecordingMode recordingMode;

    index_t channel{ 0 };
    bool held{ false };
    std::optional<ClipSettings> action;

    friend bool
    operator> (const Message &lhs, const Message &rhs)
    {
      return lhs.timepoint > rhs.timepoint;
    }
  };

  void submitFifoMessage (Message const &message);
  void processFifo ();
  void handleFifoMessage (Message const &message);

  static constexpr int fifoSize = 32;
  juce::AbstractFifo _abstractFifo{ fifoSize };
  std::array<Message, fifoSize> _fifo;

  void scheduledForRecording (std::shared_ptr<Pattern> pattern,
                              Measure timepoint);
  void scheduledForPlaying (std::shared_ptr<Pattern> pattern,
                            Measure timepoint);
  void scheduledForStop (std::shared_ptr<Pattern> pattern);
  void handleStartStopMessages ();
  void startRecording (std::shared_ptr<Pattern> pattern, Measure length);
  void startPlaying (std::shared_ptr<Pattern> pattern);
  void stop (std::shared_ptr<Pattern> pattern);

  std::priority_queue<Message, std::vector<Message>, std::greater<Message> >
      _messagesStartStop;

  void performRecording ();
  void performPlayback ();
  index_t updatePlayPosition (Pattern &pattern);

  // Dynamic playback sub-stepping for smooth slow-motion
  // Encoder range: -2 to +4 (log2), which translates to:
  //   Min playbackLength: 2^-2 * 4 beats/bar = 1 beat
  //   Max playbackLength: 2^4 * 4 beats/bar = 64 beats
  // So max slowdown is 64x. We use this to calculate sub-steps dynamically.
  static constexpr int minPlaybackLengthBeats = 1;
  static constexpr int maxPlaybackLengthBeats = 64;
  
  // Keyframe-based recording: record one sample per tick, like hardcoded patterns
  // This matches the pattern generator approach and avoids redundant data.
  // Smooth interpolation happens during playback via Cartesian interpolation.
  static constexpr int recordingSamplesPerTick = 1;
  static constexpr int minRecordingSamplesPerTick = 1;
  
  // Calculate adaptive sub-sampling: higher when recording longer patterns
  // This ensures smooth motion even at extreme slowdown speeds
  static int calculateSubSamplingFactor (Measure recordingLength, int beatsPerBar);

  Measure _now;
  Measure _recordingStarted;
  Pos _recordingPosition = Pos::invalid;     // 3D (mapped) — for OSC + visual
  Pos _recordingPosition2D = Pos::invalid;   // 2D (original) — for storing in ticks
  std::atomic<RecordingMode> _recordingMode = RecordingMode::OneShot;
  std::atomic<RecMode> _recMode = RecMode::Touch;

  /** Whether the finger has been down at any point in this take, and the last
   *  2D position it was at. Latch and Write keep writing that position after
   *  the finger lifts, so it must outlive the release that invalidates
   *  _recordingPosition2D. Both are reset when a take starts. */
  bool _recordingHasTouched = false;
  Pos _recordingHeldPosition2D = Pos::invalid;
  /** Written on the clock thread each tick a take is running, read by the UI. */
  std::atomic<float> _recordingProgress{ -1.f };

  /** Where a Random end action carries on. Lives here so the decision itself
   *  stays a pure function; only the clock thread draws from it. */
  juce::Random _random;
  int _recordingSubSamplingFactor = recordingSamplesPerTick;

  /** The take as the last finished lap left it, and how far the recording has
   *  run. What an unfinished lap is rolled back to — see TakeLaps.hh. */
  std::vector<Pos> _recordingLastComplete;
  long long _recordingTicks = 0;
  long long _recordingLap = 0;
  /** Where the write head was when the finger last came up, and whether it
   *  was down on the previous tick — a hold ends with the lap it began in,
   *  see RecMode::shouldWriteTick. */
  long long _recordingTicksAtLift = 0;
  bool _recordingFingerWasDown = false;
  void finishRecording ();
  
  // High-resolution recording counter to sample motion between ticks
  // Records at ~1000Hz regardless of tempo/ticks
  std::atomic<int> _recordingSampleCounter = 0;

  // NOTE: the MotionEngine holding shared_ptrs might lead to pattern
  // deallocations on the realtime thread. If this turns out to be
  // problematic, implement garbage collection on a low-prio thread as
  // suggested by Timur Doumler, see TempoClock.hh.
  std::shared_ptr<Pattern> _patternRecording;
  std::shared_ptr<Pattern> _patternScheduledForRecording;

  // The command dispatcher runs on its own high-priority thread and
  // receives motion / effect commands from the high-prio TempoClock
  // thread via a lockless command queue. It passes the messages to a
  // backend implementation that in turn performs the network
  // communication.
  AsyncCommandQueue _commandQueue;
  std::vector<Pos> _lastSentPositions;
  /** The last value each channel value went out at -- which is what the far
   *  end actually has, and so what a ramp has to start from. See util/Slew.hh
   *  and the send loop in tickCallback(). */
  std::vector<float> _lastSentPot1s;
  std::vector<float> _lastSentPot2s;
  std::vector<float> _lastSentPot3s;
  /** When the last send ran, on the wall clock. A tick is two to eight
   *  milliseconds depending on tempo, and a fade meant to be inaudible cannot
   *  be a different length at 180 BPM than at 60. */
  double _lastSendMillis = 0.;
  /** Whether the channel values have gone out once. Until they have there is
   *  nothing to ramp *from*: see the send loop in tickCallback(). */
  bool _potsPrimed = false;

  /** Read by the send loop on the tick thread, written from the message
   *  thread at start-up. Zero means "not held", which is what it starts as:
   *  a millisecond counter is never below zero. */
  std::atomic<double> _outputHeldUntil{ 0. };

  /** The accent per channel: whether ACT is down, where the envelope stands,
   *  and whose shape it is running. Live only — an accent is a gesture, and a
   *  gesture is not a thing to reload at startup. */
  std::vector<char> _accentHeld;
  std::vector<EnvelopeState> _accentEnvelope;
  std::vector<EnvelopeState> _freqEnvelope;
  std::vector<EnvelopeState> _qEnvelope;
  std::vector<std::shared_ptr<Pattern> > _accentPattern;

  /** The action waiting on each channel, and the clip's own settings taken
   *  down at the moment one was fired. The second is what "empty" means here:
   *  no snapshot, nothing to fall back to, so nothing is written back. */
  std::vector<std::optional<ClipSettings> > _channelAction;
  std::vector<std::optional<ClipSettings> > _accentRestore;

  /** What the readers of an accent are allowed to see.
   *
   *  Everything above belongs to the tempo-clock thread alone. These are the
   *  only things any other thread may look at, and they are atomic because
   *  the grid, the OSC sender and the tests all read them while the clock
   *  writes them.
   *
   *  Published rather than shared for one reason: the getters used to read
   *  `_accentPattern`, a `std::shared_ptr`, from whichever thread asked --
   *  and a shared_ptr read while another thread writes it can corrupt the
   *  reference count, not merely give a stale answer. What they actually
   *  wanted was three floats.
   */
  struct AccentView
  {
    std::atomic<float> level{ 0.f };
    std::atomic<float> max{ 0.f };
  };

  std::vector<AccentView> _accentView;
  std::vector<AccentView> _freqView;
  std::vector<AccentView> _qView;

  /** Copy what the readers may see out of the state the clock owns. Called at
   *  the end of every advanceAccents(), which is the only place any of it
   *  changes. */
  void publishAccentView ();

  /** The press itself, on the tempo-clock thread. */
  void applyAccentHeld (index_t channel, bool held,
                        std::shared_ptr<Pattern> pattern);


  // Per-channel preview mode: when true, suppress OSC output
  std::vector<std::atomic<bool>> _previewMode;
  /** A channel whose position a finger is holding. */
  std::vector<std::atomic<bool>> _positionHeld;

  void notifyPatternStatusListeners (PatternStatusMessage::Status status,
                                     std::shared_ptr<Pattern> pattern);
  std::set<juce::MessageListener *> _patternStatusListeners;
};

}
