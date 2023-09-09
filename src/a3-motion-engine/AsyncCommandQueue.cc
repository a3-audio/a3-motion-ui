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

#include "AsyncCommandQueue.hh"

#include <a3-motion-engine/backends/SpatBackendIEM.hh>

namespace a3
{

AsyncCommandQueue::AsyncCommandQueue (std::unique_ptr<SpatBackend> backend)
    : juce::Thread ("AsyncCommandQueue")
{
  _backend = std::move (backend);
}

AsyncCommandQueue::~AsyncCommandQueue () {}

void
AsyncCommandQueue::sendPosition (index_t channel, Pos position)
{
  Message message;
  message.command = Message::Command::SendPosition;
  message.channel = channel;
  message.position = position;
  submitMessage (std::move (message));
}

void
AsyncCommandQueue::sendWidth (index_t channel, float width)
{
  Message message;
  message.command = Message::Command::SendWidth;
  message.channel = channel;
  message.width = width;
  submitMessage (std::move (message));
}

void
AsyncCommandQueue::sendAmbisonicsOrder (index_t channel, int order)
{
  Message message;
  message.command = Message::Command::SendAmbisonicsOrder;
  message.channel = channel;
  message.order = order;
  submitMessage (std::move (message));
}

void
AsyncCommandQueue::run ()
{
  while (true)
    {
      wait (-1); // block until woken up
      if (threadShouldExit ())
        break;

      processFifo ();
    }
}

void
AsyncCommandQueue::submitMessage (Message &&message)
{
  jassert (_abstractFifo.getFreeSpace () > 0);

  const auto scope = _abstractFifo.write (1);
  jassert (scope.blockSize1 == 1);
  jassert (scope.blockSize2 == 0);

  jassert (scope.startIndex1 >= 0);
  auto startIndex = static_cast<std::size_t> (scope.startIndex1);

  _fifo[startIndex] = std::move (message);

  notify ();
}

void
AsyncCommandQueue::processFifo ()
{
  auto const ready = _abstractFifo.getNumReady ();
  const auto scope = _abstractFifo.read (ready);

  jassert (scope.blockSize1 + scope.blockSize2 == ready);

  if (scope.blockSize1 > 0)
    {
      for (int idx = scope.startIndex1;
           idx < scope.startIndex1 + scope.blockSize1; ++idx)
        {
          jassert (idx >= 0);
          processMessage (_fifo[static_cast<std::size_t> (idx)]);
        }
    }

  if (scope.blockSize2 > 0)
    for (int idx = scope.startIndex2;
         idx < scope.startIndex2 + scope.blockSize2; ++idx)
      {
        jassert (idx >= 0);
        processMessage (_fifo[static_cast<std::size_t> (idx)]);
      }
}

void
AsyncCommandQueue::processMessage (Message const &message)
{
  switch (message.command)
    {
    case Message::Command::SendPosition:
      {
        _backend->sendPosition (message.channel, message.position);
        break;
      }
    case Message::Command::SendWidth:
      {
        _backend->sendWidth (message.channel, message.width);
        break;
      }
    case Message::Command::SendAmbisonicsOrder:
      {
        _backend->sendAmbisonicsOrder (message.channel, message.order);
        break;
      }
    }
}

}
