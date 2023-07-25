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

#include "MotionEngine.hh"

#include <a3-motion-engine/util/Helpers.hh>
#include <a3-motion-engine/util/Timing.hh>

#include <a3-motion-engine/backends/SpatBackendA3.hh>

namespace a3
{

MotionEngine::MotionEngine (unsigned int const numChannels) :
  _commandQueue(std::make_unique<SpatBackendA3> ("192.168.43.50", 9000))
{
  createChannels (numChannels);
  _lastSentPositions.resize (numChannels);

  _callbackHandleTick = _tempoClock.scheduleEventHandlerAddition (
      { [this] (auto) { tickCallback (); } }, TempoClock::Event::Tick,
      TempoClock::Execution::TimerThread, false);

  _tempoClock.start ();
  _commandQueue.startThread (juce::Thread::Priority::high);
}

MotionEngine::~MotionEngine ()
{
  _commandQueue.stopThread (-1);
  _tempoClock.stop ();
}

void
MotionEngine::createChannels (unsigned int const numChannels)
{
  _channels.resize (numChannels);

  auto constexpr spread = 120.f;
  auto const azimuthSpacing = spread / (numChannels - 1);
  auto azimuth = (numChannels - 1) * azimuthSpacing / 2.f;
  for (auto &channel : _channels)
    {
      channel = std::make_unique<Channel> ();
      auto position = Pos::fromSpherical (azimuth, 0, 1.f);
      channel->setPosition (position);
      azimuth -= azimuthSpacing;
    }
}

std::vector<std::unique_ptr<Channel> > const &
MotionEngine::getChannels () const
{
  return _channels;
}

void
MotionEngine::tickCallback ()
{
  // juce::Logger::writeToLog ("MotionEngine: tick callback");

  // execute record / playback engine

  // compare with last enqueued values and enqueue on change
  // scheduleChangedPositionsForSending ();
  for (auto index = 0u; index < _channels.size (); ++index)
    {
      auto pos = _channels[index]->getPosition ();
      if (_lastSentPositions[index] != pos)
        {
          _commandQueue.submitCommand ({ int (index), pos });
          _lastSentPositions[index] = pos;
        }
    }
}

}
