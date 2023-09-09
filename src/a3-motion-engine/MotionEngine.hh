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
  MotionEngine (index_t numChannels);
  ~MotionEngine ();

  TempoClock &getTempoClock ();

  index_t getNumChannels ();

  Pos getChannelPosition (index_t channel);
  void setChannel2DPosition (index_t channel, Pos const &position);
  void setChannel3DPosition (index_t channel, Pos const &position);

  float getChannelWidth (index_t channel);
  void setChannelWidth (index_t channel, float width);

  enum class RecordingMode
  {
    Loop,
    OneShot
  };

  void setRecording2DPosition (Pos const &position);
  void setRecording3DPosition (Pos const &position);
  void releaseRecordingPosition ();
  void setRecordingMode (RecordingMode recordingMode);
  RecordingMode getRecordingMode () const;
  bool isRecording () const;
  void recordPattern (std::shared_ptr<Pattern> pattern, //
                      Measure timepoint, Measure length);

  void playPattern (std::shared_ptr<Pattern> pattern, Measure timepoint);
  void stopPattern (std::shared_ptr<Pattern> pattern, Measure timepoint);

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
  std::unique_ptr<HeightMap> _heightMap;

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
  Measure _now;
  Measure _recordingStarted;
  Pos _recordingPosition = Pos::invalid;
  std::atomic<RecordingMode> _recordingMode = RecordingMode::Loop;

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
  std::vector<float> _lastSentWidths;

  void notifyPatternStatusListeners (PatternStatusMessage::Status status,
                                     std::shared_ptr<Pattern> pattern);
  std::set<juce::MessageListener *> _patternStatusListeners;
};

}
