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

AsyncCommandQueue::AsyncCommandQueue () : juce::Thread ("AsyncCommandQueue")
{
  _backend = std::make_unique<SpatBackendIEM> ("127.0.0.1", 8910);
}

AsyncCommandQueue::~AsyncCommandQueue () {}

void
AsyncCommandQueue::submitCommand (Command &&command)
{
  jassert (_abstractFifo.getFreeSpace () > 0);

  const auto scope = _abstractFifo.write (1);
  jassert (scope.blockSize1 == 1);
  jassert (scope.blockSize2 == 0);

  jassert (scope.startIndex1 >= 0);
  auto startIndex = static_cast<std::size_t> (scope.startIndex1);

  _fifo[startIndex] = std::move (command);

  notify ();
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
          processCommand (_fifo[static_cast<std::size_t> (idx)]);
        }
    }

  if (scope.blockSize2 > 0)
    for (int idx = scope.startIndex1;
         idx < scope.startIndex1 + scope.blockSize1; ++idx)
      {
        jassert (idx >= 0);
        processCommand (_fifo[static_cast<std::size_t> (idx)]);
      }
}

void
AsyncCommandQueue::processCommand (Command const &command)
{
  _backend->sendChannelPosition (command.index, command.position);
}

}
