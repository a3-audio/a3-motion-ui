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
#include <a3-motion-engine/Channel.hh>
#include <a3-motion-engine/Master.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>

namespace a3
{

class TempoEstimator;

class MotionEngine
{
public:
  MotionEngine (unsigned int const numChannels);
  ~MotionEngine ();

  // TODO: provide iterator interface instead of exposing
  // implementation details!
  std::vector<std::unique_ptr<Channel> > const &getChannels () const;

  enum class TapResult
  {
    TempoAvailable,
    TempoNotAvailable
  };

  float getTempoBPM ();
  void setTempoBPM (float tempoBPM);
  TapResult tap (juce::int64 timeMicros);

private:
  void createChannels (unsigned int const numChannels);

  // MotionEngine runs the record/playback engine, checks for changed
  // parameters and and schedules corresponding commands with the
  // dispatcher.
  void tickCallback ();

  std::vector<std::unique_ptr<Channel> > _channels;

  // The tempo clock is the main timing engine that runs at a 'tick'
  // resolution relative to the current metrum. Callbacks for metrum
  // events (tick, beat, bar) can be registered to be called either
  // from within the high-priority thread, or the main JUCE event
  // thread.
  TempoClock _tempoClock;
  TempoClock::PointerT _callbackHandleTick;
  std::unique_ptr<TempoEstimator> _tempoEstimator;

  // The command dispatcher runs on its own high-priority thread and
  // receives motion / effect commands from the high-prio TempoClock
  // thread via a lockless command queue. It passes the messages to a
  // backend implementation that in turn performs the network
  // communication.
  AsyncCommandQueue _commandQueue;
  std::vector<Pos> _lastSentPositions;
};

}
