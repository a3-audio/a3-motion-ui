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

#include <a3-motion-engine/AsyncCommandQueue.hh>
#include <a3-motion-engine/Master.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/Helpers.hh>

namespace a3
{

class Channel;
class Pattern;
class HeightMap;

class MotionEngine
{
public:
  MotionEngine (index_t numChannels, HeightMap &heightMap);
  ~MotionEngine ();

  TempoClock const &getTempoClock () const;
  TempoClock &getTempoClock ();

  // TODO refactor to access channels directly
  index_t getNumChannels ();

  Pos getChannelPosition (index_t channel);
  void setChannel2DPosition (index_t channel, Pos const &position);
  void setChannel3DPosition (index_t channel, Pos const &position);

  float getChannelPot1 (index_t channel);
  void setChannelPot1 (index_t channel, float pot1);

  float getChannelPot2 (index_t channel);
  void setChannelPot2 (index_t channel, float pot2);

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
  RecordingMode getRecordingMode () const;

  bool isRecording () const;
  std::shared_ptr<Pattern> getRecordingPattern ();
  std::shared_ptr<Pattern> getScheduledForRecordingPattern ();

  void recordPattern (std::shared_ptr<Pattern> pattern, //
                      Measure timepoint, Measure length);

  // Playback
  std::shared_ptr<Pattern> getPlayingPattern (index_t channel);
  void playPattern (std::shared_ptr<Pattern> pattern, Measure timepoint);

  // Stop
  void stopPattern (std::shared_ptr<Pattern> pattern, Measure timepoint);

  // Preview mode: suppress OSC output for a channel while pattern plays
  void setPreviewMode (index_t channel, bool enabled);
  bool isPreviewMode (index_t channel) const;

  // Elevation coverage: controls how far around the sphere patterns wrap
  // 0.5 = hemisphere (default), 1.0 = full sphere, 0.33 = top third
  // Per-channel: each channel has its own coverage value.
  void setChannelCoverage (index_t channel, float coverage);
  float getChannelCoverage (index_t channel) const;

  // Convenience: set/get elevation coverage for ALL channels at once
  void setElevationCoverage (float coverage);
  float getElevationCoverage () const;

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
    } command;

    Pos position;
    Pos position2D;  // original 2D position (for recording ticks)
    std::shared_ptr<Pattern> pattern;
    Measure timepoint;
    Measure length;

    RecordingMode recordingMode;

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
  int _recordingSubSamplingFactor = recordingSamplesPerTick;
  
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
  std::vector<float> _lastSentPot1s;
  std::vector<float> _lastSentPot2s;

  // Per-channel preview mode: when true, suppress OSC output
  std::vector<std::atomic<bool>> _previewMode;

  // Per-channel elevation coverage: each channel wraps its patterns
  // independently around the sphere (default 0.5 = hemisphere)
  std::vector<std::atomic<float>> _channelCoverage;

  void notifyPatternStatusListeners (PatternStatusMessage::Status status,
                                     std::shared_ptr<Pattern> pattern);
  std::set<juce::MessageListener *> _patternStatusListeners;
};

}
