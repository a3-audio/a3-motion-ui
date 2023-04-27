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

#include <map>
#include <mutex>

#include <JuceHeader.h>

class ClockTimer;

namespace a3
{

/*
 * TempoClock drives the motion engine for playback/recording and OSC
 * communication.
 *
 * The JUCE HighResolutionTimer uses a high priority thread with a
 * millisecond timer resolution. We'll try it and see if the
 * performance is sufficient. Otherwise we might choose to implement
 * the timer ourselves with a realtime thread and a higher resolution.
 */
class TempoClock
{
public:
  struct Config
  {
    int beatsPerMinute = 90;
    int beatsPerBar = 4;
    int ticksPerBeat = 32;
    int timerIntervalMs = 1;
  };

  enum class Notification
  {
    Sync,
    Async
  };
  enum class Event
  {
    Bar,
    Beat,
    Tick
  };

  using CallbackT = void (Event, unsigned long long);
  using PointerT = std::shared_ptr<std::function<CallbackT> >;

  TempoClock ();
  ~TempoClock ();

  PointerT queueEventHandlerAddition (std::function<CallbackT> handler,
                                      Event event, Notification notification,
                                      bool waitForAck = false);
  void queueEventHandlerRemoval (PointerT id, bool waitForAck = false);

  void start ();
  void pause ();
  void stop ();

private:
  Config config;

  std::unique_ptr<ClockTimer> timer;

  // We use a single-producer single-consumer lock-less ring buffer to
  // forward add/delete requests of event handlers to the timer
  // thread. To extend this to multiple callers we lock on the
  // producer side.
  std::mutex mutexWriteFifo;
};

}
