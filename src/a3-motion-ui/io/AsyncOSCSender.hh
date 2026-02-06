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

#include <JuceHeader.h>

#include <array>
#include <cstring>

namespace a3
{

/**
 * Non-blocking OSC sender that enqueues messages into a lock-free
 * FIFO and sends them on a dedicated background thread. This ensures
 * that no socket I/O (including potential DNS resolution via
 * getaddrinfo) ever blocks the UI or timer threads.
 *
 * Uses UDP via JUCE's OSCSender (DatagramSocket) internally.
 */
class AsyncOSCSender : public juce::Thread
{
public:
  AsyncOSCSender () : juce::Thread ("AsyncOSCSender") {}

  ~AsyncOSCSender () override
  {
    disconnect ();
  }

  bool connect (const juce::String &host, int port)
  {
    _host = host;
    _port = port;

    if (!_sender.connect (host, port))
      return false;

    startThread (juce::Thread::Priority::normal);
    return true;
  }

  void disconnect ()
  {
    signalThreadShouldExit ();
    notify ();
    stopThread (2000);
    _sender.disconnect ();
  }

  /** Enqueue an OSC message for async sending. Lock-free, safe to
   *  call from any thread including realtime/timer threads.
   *  Returns false if the FIFO is full (message dropped).
   */
  bool send (const juce::OSCMessage &message)
  {
    FifoMessage fifoMsg;
    fifoMsg.addressPattern = message.getAddressPattern ().toString ();

    fifoMsg.numArgs = std::min (message.size (), maxArgs);
    for (int i = 0; i < fifoMsg.numArgs; ++i)
      {
        auto const &arg = message[i];
        if (arg.isInt32 ())
          {
            fifoMsg.args[i].type = ArgType::Int32;
            fifoMsg.args[i].intVal = arg.getInt32 ();
          }
        else if (arg.isFloat32 ())
          {
            fifoMsg.args[i].type = ArgType::Float32;
            fifoMsg.args[i].floatVal = arg.getFloat32 ();
          }
      }

    const auto scope = _abstractFifo.write (1);
    if (scope.blockSize1 == 0 && scope.blockSize2 == 0)
      return false; // FIFO full, drop message

    auto idx = (scope.blockSize1 > 0) ? scope.startIndex1 : scope.startIndex2;
    _fifo[static_cast<std::size_t> (idx)] = fifoMsg;

    notify ();
    return true;
  }

  void run () override
  {
    while (!threadShouldExit ())
      {
        processFifo ();
        wait (-1); // block until notify() or exit
      }
    // drain remaining messages on shutdown
    processFifo ();
  }

private:
  static constexpr int fifoSize = 256;
  static constexpr int maxArgs = 4;

  enum class ArgType
  {
    Int32,
    Float32
  };

  struct Arg
  {
    ArgType type = ArgType::Int32;
    union
    {
      int intVal;
      float floatVal;
    };
  };

  struct FifoMessage
  {
    juce::String addressPattern;
    int numArgs = 0;
    std::array<Arg, maxArgs> args;
  };

  void
  processFifo ()
  {
    auto const ready = _abstractFifo.getNumReady ();
    if (ready == 0)
      return;

    const auto scope = _abstractFifo.read (ready);

    auto processBlock = [this] (int start, int count) {
      for (int idx = start; idx < start + count; ++idx)
        {
          auto &msg = _fifo[static_cast<std::size_t> (idx)];
          auto oscMsg = juce::OSCMessage (msg.addressPattern);
          for (int a = 0; a < msg.numArgs; ++a)
            {
              if (msg.args[a].type == ArgType::Int32)
                oscMsg.addInt32 (msg.args[a].intVal);
              else
                oscMsg.addFloat32 (msg.args[a].floatVal);
            }
          _sender.send (oscMsg);
        }
    };

    if (scope.blockSize1 > 0)
      processBlock (scope.startIndex1, scope.blockSize1);
    if (scope.blockSize2 > 0)
      processBlock (scope.startIndex2, scope.blockSize2);
  }

  juce::OSCSender _sender;
  juce::String _host;
  int _port = 0;

  juce::AbstractFifo _abstractFifo{ fifoSize };
  std::array<FifoMessage, fifoSize> _fifo;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AsyncOSCSender)
};

}
