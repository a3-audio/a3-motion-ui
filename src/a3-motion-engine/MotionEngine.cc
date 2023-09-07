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

#include <cstddef>

#include <a3-motion-engine/Channel.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/UserConfig.hh>
#include <a3-motion-engine/backends/SpatBackendA3.hh>
#include <a3-motion-engine/elevation/HeightMap.hh>
#include <a3-motion-engine/elevation/HeightMapFlat.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>
#include <a3-motion-engine/util/Helpers.hh>
#include <a3-motion-engine/util/Timing.hh>

namespace a3
{

MotionEngine::MotionEngine (index_t numChannels)
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
MotionEngine::setRecord2DPosition (Pos const &position)
{
  Message message;
  message.command = Message::Command::SetRecordPosition;

  auto mappedPosition = Pos::fromCartesian (
      position.x (), position.y (), _heightMap->computeHeight (position));
  message.position = mappedPosition;

  submitFifoMessage (message);
}

void
MotionEngine::setRecord3DPosition (Pos const &position)
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

bool
MotionEngine::isRecording () const
{
  return _patternRecording != nullptr;
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
        scheduledForRecording (message.pattern, message.timepoint);
        _messagesStartStop.push (message);
        break;
      }
    case Message::Command::StartPlaying:
      {
        scheduledForPlaying (message.pattern, message.timepoint);
        _messagesStartStop.push (message);
        break;
      }
    case Message::Command::Stop:
      {
        juce::Logger::writeToLog ("scheduling stop: "
                                  + toString (message.timepoint));
        scheduledForStop (message.pattern);
        _messagesStartStop.push (message);
        break;
      }
    }
}

void
MotionEngine::scheduledForRecording (std::shared_ptr<Pattern> pattern,
                                     Measure timepoint)
{
  if (_patternScheduledForRecording)
    {
      _patternScheduledForRecording->restoreStatus ();
      // we do not remove the pattern from the record/play
      // priority queue here but instead compare the scheduled
      // message against _patternScheduledForRecording when the
      // event takes place.
    }

  if (_patternRecording && _patternRecording != pattern)
    {
      stopPattern (_patternRecording, timepoint);
    }
  if (_channels[pattern->getChannel ()]->_patternPlaying
      && _channels[pattern->getChannel ()]->_patternPlaying != pattern)
    {
      stopPattern (_channels[pattern->getChannel ()]->_patternPlaying,
                   timepoint);
    }

  _patternScheduledForRecording = pattern;
  _patternScheduledForRecording->setStatus (
      Pattern::Status::ScheduledForRecording);
}

void
MotionEngine::scheduledForPlaying (std::shared_ptr<Pattern> pattern,
                                   Measure timepoint)
{
  auto &channelScheduled = *_channels[pattern->getChannel ()];

  if (channelScheduled._patternScheduledForPlaying)
    {
      // TODO: do we want to restore the record case?
      channelScheduled._patternScheduledForPlaying->restoreStatus ();
    }

  if (channelScheduled._patternPlaying
      && channelScheduled._patternPlaying != pattern)
    {
      stopPattern (channelScheduled._patternPlaying, timepoint);
    }

  channelScheduled._patternScheduledForPlaying = pattern;
  channelScheduled._patternScheduledForPlaying->setStatus (
      Pattern::Status::ScheduledForPlaying);
}

void
MotionEngine::scheduledForStop (std::shared_ptr<Pattern> pattern)
{
  auto const status = pattern->getStatus ();
  if (status == Pattern::Status::Playing || //
      status == Pattern::Status::Recording)
    {
      pattern->setStatus (Pattern::Status::ScheduledForIdle);
    }
}

void
MotionEngine::handleStartStopMessages ()
{
  while (!_messagesStartStop.empty ()
         && _messagesStartStop.top ().timepoint <= _now)
    {
      auto message = _messagesStartStop.top ();
      _messagesStartStop.pop ();
      juce::Logger::writeToLog ("handling message: "
                                + toString (message.timepoint));

      switch (message.command)
        {
        case Message::Command::StartRecording:
          {
            startRecording (
                message.pattern,
                Measure::convertToTicks (message.length,
                                         _tempoClock.getBeatsPerBar ()));
            break;
          }
        case Message::Command::StartPlaying:
          {
            startPlaying (message.pattern);
            break;
          }
        case Message::Command::Stop:
          {
            stop (message.pattern);
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
MotionEngine::startRecording (std::shared_ptr<Pattern> pattern,
                              int lengthTicks)
{
  if (pattern != _patternScheduledForRecording)
    return;

  if (_patternRecording)
    {
      _patternRecording->setStatus (Pattern::Status::Idle);
    }
  _patternRecording = _patternScheduledForRecording;

  jassert (lengthTicks >= 0);
  _patternRecording->clear ();
  _patternRecording->resize (static_cast<std::size_t> (lengthTicks));

  _patternRecording->setStatus (Pattern::Status::Recording);
  _recordingStarted = _now;

  _patternScheduledForRecording = nullptr;
}

void
MotionEngine::startPlaying (std::shared_ptr<Pattern> pattern)
{
  auto &channel = *_channels[pattern->getChannel ()];
  if (pattern != channel._patternScheduledForPlaying)
    return;

  if (channel._patternPlaying)
    {
      channel._patternPlaying->setStatus (Pattern::Status::Idle);
    }
  channel._patternPlaying = channel._patternScheduledForPlaying;
  channel._patternPlaying->setStatus (Pattern::Status::Playing);
  channel._playingStarted = _now;

  channel._patternScheduledForPlaying = nullptr;
  _patternRecording = nullptr;
}

void
MotionEngine::stop (std::shared_ptr<Pattern> pattern)
{
  pattern->setStatus (Pattern::Status::Idle);
  // _channels[pattern->_channel]->_patternPlaying = nullptr;
  // _channels[pattern->_channel]->_patternScheduledForPlaying = nullptr;
  // _patternRecording = nullptr;
}

void
MotionEngine::performRecording ()
{
  if (!_patternRecording)
    return;

  auto const status = _patternRecording->getStatus ();
  auto const statusLast = _patternRecording->getLastStatus ();
  if (status == Pattern::Status::Recording
      || (status == Pattern::Status::ScheduledForIdle
          && statusLast == Pattern::Status::Recording)
      || (status == Pattern::Status::ScheduledForPlaying
          && statusLast == Pattern::Status::Recording))
    {
      auto const ticksSinceStart = Measure::convertToTicks (
          _now - _recordingStarted, _tempoClock.getBeatsPerBar ());
      if (ticksSinceStart < 0)
        {
          juce::Logger::writeToLog ("now: " + toString (_now));
          juce::Logger::writeToLog ("recording started: "
                                    + toString (_recordingStarted));
        }
      jassert (ticksSinceStart >= 0);

      auto const ticksPatternLength = _patternRecording->getNumTicks ();
      auto const tick
          = static_cast<std::size_t> (ticksSinceStart) % ticksPatternLength;
      _patternRecording->setTick (tick, _recordingPosition);

      if (_recordingPosition.isValid ())
        {
          _channels[_patternRecording->getChannel ()]->setPosition (
              _recordingPosition);
        }
    }
}

void
MotionEngine::performPlayback ()
{
  for (auto &channel : _channels)
    {
      if (channel->_patternPlaying)
        {
          auto const status = channel->_patternPlaying->getStatus ();
          auto const statusLast = channel->_patternPlaying->getLastStatus ();

          if (status == Pattern::Status::Playing
              || (status == Pattern::Status::ScheduledForIdle
                  && statusLast == Pattern::Status::Playing)
              || (status == Pattern::Status::ScheduledForRecording
                  && statusLast == Pattern::Status::Playing))
            {
              auto const ticksSinceStart
                  = Measure::convertToTicks (_now - channel->_playingStarted,
                                             _tempoClock.getBeatsPerBar ());
              if (ticksSinceStart < 0)
                {
                  juce::Logger::writeToLog ("now: " + toString (_now));
                  juce::Logger::writeToLog (
                      "playing started: "
                      + toString (channel->_playingStarted));
                }
              jassert (ticksSinceStart >= 0);

              auto const ticksPatternLength
                  = channel->_patternPlaying->getNumTicks ();

              auto const tick = static_cast<std::size_t> (ticksSinceStart)
                                % ticksPatternLength;

              auto position = channel->_patternPlaying->getTick (tick);
              if (position.isValid ())
                {
                  channel->setPosition (position);
                }
            }
        }
    }
}

}
