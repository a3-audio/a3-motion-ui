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

namespace a3
{

MotionEngine::MotionEngine (unsigned int const numChannels)
{
  createChannels (numChannels);

  _tempoClock.start ();

  // TODO: move to a unit (?) test
  // testAddRemoveHandlers ();
}

MotionEngine::~MotionEngine ()
{
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

std::vector<std::unique_ptr<Channel> > &
MotionEngine::getChannels ()
{
  return _channels;
}

void
MotionEngine::testAddRemoveHandlers ()
{
  Timings<std::chrono::steady_clock> timings;

  {
    // ScopedTimer timer{ timings, "sync tick" };
    _callbackHandleTimer = _tempoClock.scheduleEventHandlerAddition (
        [] (auto measure) { print (measure, "==="); }, TempoClock::Event::Tick,
        TempoClock::Execution::TimerThread, true);
  }

  {
    // ScopedTimer timer (timings, "async tick");
    _callbackHandleMessage = _tempoClock.scheduleEventHandlerAddition (
        [] (auto measure) { print (measure, "---"); }, TempoClock::Event::Tick,
        TempoClock::Execution::JuceMessageThread, true);
  }

  // juce::Thread::sleep (2000);
  // callbackHandleTimer = nullptr;
  // callbackHandleMessage = nullptr;

  // print (timings);
}

}
