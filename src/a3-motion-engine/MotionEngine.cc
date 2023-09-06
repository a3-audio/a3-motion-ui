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

#include <a3-motion-engine/Channel.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/UserConfig.hh>
#include <a3-motion-engine/backends/SpatBackendA3.hh>
#include <a3-motion-engine/elevation/HeightMap.hh>
#include <a3-motion-engine/elevation/HeightMapFlat.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>
#include <a3-motion-engine/util/Helpers.hh>
#include <a3-motion-engine/util/Timing.hh>
#include <cstddef>

namespace a3
{

MotionEngine::MotionEngine (unsigned int const numChannels)
    : _heightMap (std::make_unique<HeightMapSphere> ()),
      _commandQueue (std::make_unique<SpatBackendA3> (userConfig["hostname"],
                                                      userConfig["port"]))
{
  createChannels (numChannels);

  _callbackHandleTick = _tempoClock.scheduleEventHandlerAddition (
      { [this] (auto measure) {
        _now = measure;
        tickCallback ();
      } },
      TempoClock::Event::Tick, TempoClock::Execution::TimerThread, false);

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

TempoClock &
MotionEngine::getTempoClock ()
{
  return _tempoClock;
}

index_t
MotionEngine::getNumChannels ()
{
  return _channels.size ();
}

Pos
MotionEngine::getChannelPosition (index_t channel)
{
  return _channels[channel]->getPosition ();
}

void
MotionEngine::setChannel2DPosition (index_t channel, Pos const &position)
{
  jassert (_heightMap);
  auto mappedPosition = Pos::fromCartesian (
      position.x (), position.y (), _heightMap->computeHeight (position));
  _channels[channel]->setPosition (mappedPosition);
}

void
MotionEngine::setChannel3DPosition (index_t channel, Pos const &position)
{
  _channels[channel]->setPosition (position);
}

void
MotionEngine::setRecordPosition (Pos const &position)
{
  Message message;
  message.command = Message::Command::SetRecordPosition;
  message.position = position;

  submitFifoMessage (message);
}

void
MotionEngine::releaseRecordPosition ()
{
  Message message;
  message.command = Message::Command::ReleaseRecordPosition;

  submitFifoMessage (message);
}

void
MotionEngine::recordPattern (std::shared_ptr<Pattern> pattern,
                             Measure timepoint, Measure length)
{
  Message message;
  message.command = Message::Command::StartRecording;
  message.pattern = pattern;
  message.timepoint = timepoint;
  message.length = length;
  submitFifoMessage (message);
}

void
MotionEngine::playPattern (std::shared_ptr<Pattern> pattern, Measure timepoint)
{
  Message message;
  message.command = Message::Command::StartPlaying;
  message.pattern = pattern;
  message.timepoint = timepoint;
  message.length = {};
  submitFifoMessage (message);
}

void
MotionEngine::stopPattern (std::shared_ptr<Pattern> pattern, Measure timepoint)
{
  Message message;
  message.command = Message::Command::Stop;
  message.pattern = pattern;
  message.timepoint = timepoint;
  message.length = {};
  submitFifoMessage (message);
}

void
MotionEngine::tickCallback ()
{
  processFifo ();

  handleStartStopMessages ();

  performRecording ();
  performPlayback ();

  // compare with last enqueued values and enqueue on change
  for (auto index = 0u; index < _channels.size (); ++index)
    {
      auto &channel = *_channels[index];
      auto pos = channel.getPosition ();
      if (channel._lastSentPosition != pos)
        {
          _commandQueue.submitCommand ({ int (index), pos });
          channel._lastSentPosition = pos;
        }
    }
}

void
MotionEngine::submitFifoMessage (Message const &message)
{
  jassert (_abstractFifo.getFreeSpace () > 0);

  const auto scope = _abstractFifo.write (1);
  jassert (scope.blockSize1 == 1);
  jassert (scope.blockSize2 == 0);

  jassert (scope.startIndex1 >= 0);
  auto startIndex = static_cast<std::size_t> (scope.startIndex1);

  _fifo[startIndex] = message;
}

void
MotionEngine::processFifo ()
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
          handleFifoMessage (_fifo[static_cast<std::size_t> (idx)]);
        }
    }

  if (scope.blockSize2 > 0)
    {
      for (int idx = scope.startIndex2;
           idx < scope.startIndex2 + scope.blockSize2; ++idx)
        {
          jassert (idx >= 0);
          handleFifoMessage (_fifo[static_cast<std::size_t> (idx)]);
        }
    }
}

void
MotionEngine::handleFifoMessage (Message const &message)
{
  switch (message.command)
    {
    case Message::Command::SetRecordPosition:
      {
        _recordingPosition = message.position;
        break;
      }
    case Message::Command::ReleaseRecordPosition:
      {
        _recordingPosition = Pos::invalid;
        break;
      }
    case Message::Command::StartRecording:
      {
        schedulePatternForRecording (message.pattern);
        _messagesStartStop.push (message);
        break;
      }
    case Message::Command::StartPlaying:
      {
        schedulePatternForPlaying (message.pattern);
        _messagesStartStop.push (message);
        break;
      }
    case Message::Command::Stop:
      {
        message.pattern->setStatus (Pattern::Status::ScheduledForIdle);
        _messagesStartStop.push (message);
        break;
      }
    }
}

void
MotionEngine::schedulePatternForRecording (std::shared_ptr<Pattern> pattern)
{
  if (_patternScheduledForRecording)
    {
      _patternScheduledForRecording->restoreStatus ();
      // we do not remove the pattern from the record/play
      // priority queue here but instead compare the scheduled
      // message against _patternScheduledForRecording when the
      // event takes place.
    }

  if (_patternRecording)
    {
      _patternRecording->setStatus (Pattern::Status::ScheduledForIdle);
    }

  _patternScheduledForRecording = pattern;
  _patternScheduledForRecording->setStatus (
      Pattern::Status::ScheduledForRecording);
}

void
MotionEngine::schedulePatternForPlaying (std::shared_ptr<Pattern> pattern)
{
  auto &channelScheduled = *_channels[pattern->getChannel ()];

  if (channelScheduled._patternScheduledForPlaying)
    {
      // TODO: do we want to restore the record case?
      channelScheduled._patternScheduledForPlaying->restoreStatus ();
    }

  if (channelScheduled._patternPlaying)
    {
      channelScheduled._patternPlaying->setStatus (
          Pattern::Status::ScheduledForIdle);
    }

  channelScheduled._patternScheduledForPlaying = pattern;
  channelScheduled._patternScheduledForPlaying->setStatus (
      Pattern::Status::ScheduledForPlaying);
}

void
MotionEngine::handleStartStopMessages ()
{
  while (!_messagesStartStop.empty ()
         && _messagesStartStop.top ().timepoint <= _now)
    {
      auto message = _messagesStartStop.top ();
      _messagesStartStop.pop ();

      switch (message.command)
        {
        case Message::Command::StartRecording:
          {
            if (message.pattern != _patternScheduledForRecording)
              continue;

            if (_patternRecording)
              {
                _patternRecording->setStatus (Pattern::Status::Idle);
              }
            _patternRecording = _patternScheduledForRecording;
            _patternRecording->setStatus (Pattern::Status::Recording);
            _recordingStarted = _now;

            _patternScheduledForRecording = nullptr;
            break;
          }
        case Message::Command::StartPlaying:
          {
            auto &channel = *_channels[message.pattern->getChannel ()];
            if (message.pattern != channel._patternScheduledForPlaying)
              continue;

            if (channel._patternPlaying)
              {
                channel._patternPlaying->setStatus (Pattern::Status::Idle);
              }
            channel._patternPlaying = channel._patternScheduledForPlaying;
            channel._patternPlaying->setStatus (Pattern::Status::Playing);
            channel._patternScheduledForPlaying = nullptr;
            break;
          }
        case Message::Command::Stop:
          {
            // juce::Logger::writeToLog ("MotionEngine: stopping playback");
            message.pattern->setStatus (Pattern::Status::Idle);
            break;
          }
        case Message::Command::SetRecordPosition:
        case Message::Command::ReleaseRecordPosition:
          {
            throw std::runtime_error (
                "invalid command message in start/stop queue");
            break;
          }
        }
    }
}

void
MotionEngine::performRecording ()
{
  if (!_patternRecording)
    return;
}

void
MotionEngine::performPlayback ()
{
  for (auto &channel : _channels)
    {
      if (channel->_patternPlaying)
        {
          // _channels[channel]->setPosition
        }
    }
}

}
