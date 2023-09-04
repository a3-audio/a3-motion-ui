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

  void setRecordPosition (Pos const &position);
  void releaseRecordPosition ();

  void recordToPattern (std::shared_ptr<Pattern> pattern, //
                        Measure timepoint, Measure length);

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
      SetRecordPosition,
      ReleaseRecordPosition,
      RecordStart,
      RecordStop,
    } command;

    Pos position;
    std::shared_ptr<Pattern> pattern;
    Measure timepoint;
    Measure length;

    friend bool
    operator< (const Message &lhs, const Message &rhs)
    {
      return lhs.timepoint < rhs.timepoint;
    }
  };

  void submitFifoMessage (Message const &message);
  void processFifo ();
  void handleFifoMessage (Message const &message);
  static constexpr int fifoSize = 32;
  juce::AbstractFifo _abstractFifo{ fifoSize };
  std::array<Message, fifoSize> _fifo;

  void handleStartStopMessages ();
  std::priority_queue<Message> _messagesStartStop;

  void performRecording ();
  void performPlayback ();
  Measure _now;
  Measure _recordLength;
  std::optional<Pos> _recordPosition;
  // NOTE: the MotionEngine holding a set of shared_ptr might lead to
  // pattern deallocations on the realtime thread. If this turns out to be
  // problematic, implement garbage collection on a low-prio thread as
  // suggested by Timur Doumler, see TempoClock.hh.
  std::set<std::shared_ptr<Pattern> > _recordPatterns;

  // The command dispatcher runs on its own high-priority thread and
  // receives motion / effect commands from the high-prio TempoClock
  // thread via a lockless command queue. It passes the messages to a
  // backend implementation that in turn performs the network
  // communication.
  AsyncCommandQueue _commandQueue;
  std::vector<Pos> _lastSentPositions;
};

}
