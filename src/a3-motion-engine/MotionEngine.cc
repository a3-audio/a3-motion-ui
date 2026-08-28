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
#include <a3-motion-engine/util/Helpers.hh>
#include <a3-motion-engine/util/Timing.hh>

namespace a3
{

// Calculate adaptive sub-sampling based on recording length
// Longer recordings get significantly higher sub-sampling to support smooth slow-motion playback
// Strategy: allocate enough samples to handle even extreme slowdowns (1/16 speed)
int
MotionEngine::calculateSubSamplingFactor (Measure recordingLength, int beatsPerBar)
{
  // Get recording length in beats (for future use if we want adaptive scaling)
  auto const recordingConsolidated = recordingLength.consolidate (beatsPerBar);
  (void) recordingConsolidated; // Use variable to avoid warning
  
  // Ultra-high recording resolution for silky-smooth interpolation
  // 512 samples per tick ensures we capture motion detail at sub-millisecond precision
  // This is independent of playback speed - we always record at maximum quality
  int factor = recordingSamplesPerTick;
  
  return factor;
}

MotionEngine::MotionEngine (index_t numChannels, HeightMap &heightMap)
    : _heightMap (heightMap), _commandQueue (std::make_unique<SpatBackendA3> (
                                  userConfig["oscSender"]["host"], 
                                  static_cast<int> (userConfig["oscSender"]["port"])))
{
  createChannels (numChannels);

  _callbackHandleTick = _tempoClock.scheduleEventHandlerAddition (
      { [this] (auto measure) {
        _now = measure;
        tickCallback ();
      } },
      TempoClock::Event::Tick, TempoClock::Execution::TimerThread, false);

  _tempoClock.start ();
  _commandQueue.startThread (juce::Thread::Priority::normal);  // lower than timer thread
}

MotionEngine::~MotionEngine ()
{
  jassert (_patternStatusListeners.empty ());
  _commandQueue.stopThread (-1);
  _tempoClock.stop ();
}

void
MotionEngine::createChannels (index_t const numChannels)
{
  _channels.resize (numChannels);
  _lastSentPositions.resize (numChannels);
  _positionHeld = std::vector<std::atomic<bool>> (numChannels);
  _lastSentPot1s.resize (numChannels);
  _lastSentPot2s.resize (numChannels);
  _previewMode = std::vector<std::atomic<bool>> (numChannels);

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

TempoClock const &
MotionEngine::getTempoClock () const
{
  return _tempoClock;
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
  // No Pattern context for a manual/live drag — use whatever clip is
  // currently playing on this channel (if any), else a neutral default.
  auto const playing = getPlayingPattern (channel);
  auto const params
      = playing ? playing->getElevationParams () : ElevationParams{};
  auto mappedPosition = _heightMap.mapTo3D (position, params);
  _channels[channel]->setPosition (mappedPosition);
}

void
MotionEngine::setChannel3DPosition (index_t channel, Pos const &position)
{
  _channels[channel]->setPosition (position);
}

void
MotionEngine::setChannelPositionHeld (index_t channel, bool held)
{
  _positionHeld[channel].store (held, std::memory_order_relaxed);
}

bool
MotionEngine::isChannelPositionHeld (index_t channel) const
{
  return _positionHeld[channel].load (std::memory_order_relaxed);
}

float
MotionEngine::getChannelPot1 (index_t channel)
{
  return _channels[channel]->getPot1 ();
}

void
MotionEngine::setChannelPot1 (index_t channel, float pot1)
{
  _channels[channel]->setPot1 (pot1);
}

float
MotionEngine::getChannelPot2 (index_t channel)
{
  return _channels[channel]->getPot2 ();
}

void
MotionEngine::setChannelPot2 (index_t channel, float pot2)
{
  _channels[channel]->setPot2 (pot2);
}

std::shared_ptr<Pattern>
MotionEngine::getPlayingPattern (index_t channel)
{
  return _channels[channel]->_patternPlaying;
}

void
MotionEngine::setRecording2DPosition (Pos const &position)
{
  Message message;
  message.command = Message::Command::SetRecordingPosition;

  // Defer 3D mapping to performRecording where we know the channel.
  // Store a placeholder for position; the real mapping uses per-channel coverage.
  message.position = Pos::invalid;
  message.position2D = position;  // keep original 2D for pattern ticks

  submitFifoMessage (message);
}

void
MotionEngine::setRecording3DPosition (Pos const &position)
{
  Message message;
  message.command = Message::Command::SetRecordingPosition;
  message.position = position;
  // Not derived here: turning a direction into a pattern coordinate needs the
  // recording pattern's own elevation parameters, and dropping z instead is
  // exactly the wrong conversion — it reads the radius in the drawing's space
  // rather than the pattern's, short by 1/sqrt(2). performRecording() does it
  // with mapTo2D, where the parameters are known.
  message.position2D = Pos::invalid;
  submitFifoMessage (message);
}

void
MotionEngine::releaseRecordingPosition ()
{
  Message message;
  message.command = Message::Command::ReleaseRecordingPosition;

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
MotionEngine::setPreviewMode (index_t channel, bool enabled)
{
  jassert (channel < _previewMode.size ());
  _previewMode[channel].store (enabled, std::memory_order_relaxed);
}

bool
MotionEngine::isPreviewMode (index_t channel) const
{
  jassert (channel < _previewMode.size ());
  return _previewMode[channel].load (std::memory_order_relaxed);
}

TempoClock::TapResult
MotionEngine::tap (juce::int64 timeMicros)
{
  return _tempoClock.tap (timeMicros);
}

float
MotionEngine::getTempoBPM () const
{
  return _tempoClock.getTempoBPM ();
}

void
MotionEngine::setTempoBPM (float bpm)
{
  _tempoClock.setTempoBPM (bpm);
}

int
MotionEngine::getBeatsPerBar () const
{
  return _tempoClock.getBeatsPerBar ();
}

void
MotionEngine::resetTempo ()
{
  _tempoClock.reset ();
}

void
MotionEngine::setRecordingMode (RecordingMode recordingMode)
{
  Message message;
  message.command = Message::Command::SetRecordingMode;
  message.recordingMode = recordingMode;
  submitFifoMessage (message);
}

void
MotionEngine::setAutomationMode (AutomationMode mode)
{
  _automationMode.store (mode, std::memory_order_relaxed);
}

AutomationMode
MotionEngine::getAutomationMode () const
{
  return _automationMode.load (std::memory_order_relaxed);
}

MotionEngine::RecordingMode
MotionEngine::getRecordingMode () const
{
  return _recordingMode;
}

bool
MotionEngine::isRecording () const
{
  return _patternRecording != nullptr;
}

bool
MotionEngine::isRecordingOrScheduled () const
{
  return _patternRecording != nullptr
         || _patternScheduledForRecording != nullptr;
}

float
MotionEngine::getRecordingProgress () const
{
  return _recordingProgress.load ();
}

std::shared_ptr<Pattern>
MotionEngine::getRecordingPattern ()
{
  return _patternRecording;
}

std::shared_ptr<Pattern>
MotionEngine::getScheduledForRecordingPattern ()
{
  return _patternScheduledForRecording;
}

void
MotionEngine::addPatternStatusListener (juce::MessageListener *listener)
{
  _patternStatusListeners.insert (listener);
}

void
MotionEngine::removePatternStatusListener (juce::MessageListener *listener)
{
  _patternStatusListeners.erase (listener);
}

void
MotionEngine::tickCallback ()
{
  processFifo ();

  handleStartStopMessages ();

  performRecording ();
  
  // Perform playback once per tick. With absolute time-based position calculation,
  // we don't need to worry about accumulation errors or sub-stepping granularity.
  // The position is always precisely calculated from elapsed time.
  performPlayback ();

  // compare with last enqueued values and enqueue on change
  for (auto index = 0u; index < _channels.size (); ++index)
    {
      // Skip OSC output for channels in preview mode
      if (_previewMode[index].load (std::memory_order_relaxed))
        continue;

      auto const position = _channels[index]->getPosition ();
      if (position.isValid () && _lastSentPositions[index] != position)
        {
          _commandQueue.sendPosition (index, position);
          _lastSentPositions[index] = position;
        }

      auto const pot1 = _channels[index]->getPot1 ();
      if (!juce::approximatelyEqual (_lastSentPot1s[index], pot1))
        {
          _commandQueue.sendPot1 (index, pot1);
          _lastSentPot1s[index] = pot1;
        }

      auto const pot2 = _channels[index]->getPot2 ();
      if (!juce::approximatelyEqual (_lastSentPot2s[index], pot2))
        {
          _commandQueue.sendPot2 (index, pot2);
          _lastSentPot2s[index] = pot2;
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
    case Message::Command::SetRecordingPosition:
      {
        _recordingPosition = message.position;
        _recordingPosition2D = message.position2D;
        break;
      }
    case Message::Command::ReleaseRecordingPosition:
      {
        _recordingPosition = Pos::invalid;
        _recordingPosition2D = Pos::invalid;
        break;
      }
    case Message::Command::SetRecordingMode:
      {
        _recordingMode = message.recordingMode;
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
#ifdef DEBUG
        juce::Logger::writeToLog ("scheduling stop: "
                                  + toString (message.timepoint));
#endif
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

  // NOTE: Do NOT stop _patternRecording here - if one pad is currently recording,
  // and another pad is scheduled for recording at the next downbeat, we should
  // allow both to proceed. The currently recording pattern will be stopped by
  // its own scheduled stop message.

  // However, immediately stop any playback in the same channel
  auto &channel = *_channels[pattern->getChannel ()];
  if (channel._patternPlaying)
    {
      channel._patternPlaying->setStatus (Pattern::Status::Idle);
      channel._patternPlaying = nullptr;
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
#ifdef DEBUG
  if (!_messagesStartStop.empty ())
    {
      juce::Logger::writeToLog ("handleStartStopMessages called - queue has "
                                + juce::String (static_cast<int> (_messagesStartStop.size ())) + " items");
      juce::Logger::writeToLog ("  Front timepoint: " + toString (_messagesStartStop.top ().timepoint));
      juce::Logger::writeToLog ("  Current _now:    " + toString (_now));
    }
#endif
  
  while (!_messagesStartStop.empty ()
         && _messagesStartStop.top ().timepoint <= _now)
    {
      auto message = _messagesStartStop.top ();
      _messagesStartStop.pop ();
#ifdef DEBUG
      juce::Logger::writeToLog ("handling message: "
                                + toString (message.timepoint));
#endif

      switch (message.command)
        {
        case Message::Command::StartRecording:
          {
            startRecording (message.pattern, message.length);

            // one-shot recording: schedule stop right away
            if (_recordingMode == RecordingMode::OneShot)
              {
                auto const timepointStop
                    = (message.timepoint + message.length)
                          .consolidate (_tempoClock.getBeatsPerBar ());
                stopPattern (message.pattern, timepointStop);
              }

            notifyPatternStatusListeners (
                PatternStatusMessage::Status::Recording, message.pattern);
            break;
          }
        case Message::Command::StartPlaying:
          {
            startPlaying (message.pattern);

            notifyPatternStatusListeners (
                PatternStatusMessage::Status::Playing, message.pattern);
            break;
          }
        case Message::Command::Stop:
          {
            stop (message.pattern);

            notifyPatternStatusListeners (
                PatternStatusMessage::Status::Stopped, message.pattern);
            break;
          }
        case Message::Command::SetRecordingPosition:
        case Message::Command::ReleaseRecordingPosition:
        case Message::Command::SetRecordingMode:
          {
            throw std::runtime_error (
                "invalid command message in start/stop queue");
            break;
          }
        }
    }
}

void
MotionEngine::startRecording (std::shared_ptr<Pattern> pattern, Measure length)
{
  if (!pattern)
    return;

  // Stop any currently recording pattern
  if (_patternRecording && _patternRecording != pattern)
    {
      _patternRecording->setStatus (Pattern::Status::Idle);
    }
  
  _patternRecording = pattern;

  // Calculate adaptive sub-sampling factor based on recording length
  _recordingSubSamplingFactor = calculateSubSamplingFactor (length, _tempoClock.getBeatsPerBar ());
#ifdef DEBUG
  juce::Logger::writeToLog ("Recording with sub-sampling factor: " + juce::String (_recordingSubSamplingFactor));
#endif

  auto const ticks
      = Measure::convertToTicks (length, _tempoClock.getBeatsPerBar ());
  jassert (ticks >= 0);

  _patternRecording->clear ();
  // Allocate with adaptive sub-sampling for smooth playback at any speed
  auto const ticksWithSubSampling = static_cast<std::size_t> (ticks) * _recordingSubSamplingFactor;
  _patternRecording->resize (ticksWithSubSampling);

  _recordingPosition = Pos::invalid;
  _recordingPosition2D = Pos::invalid;
  _recordingStarted = _now;
  _recordingProgress.store (0.f);

  // Write starts overwriting from its first tick, before anything has been
  // touched, so it needs something to write: where the blob stands as the take
  // begins. Touch and Latch never reach for this.
  _recordingHasTouched = false;
  auto const startPosition = _channels[pattern->getChannel ()]->getPosition ();
  _recordingHeldPosition2D
      = startPosition.isValid ()
            ? _heightMap.mapTo2D (startPosition, pattern->getElevationParams ())
            : Pos::invalid;
  _patternRecording->setStatus (Pattern::Status::Recording);

  // Clear scheduled flag only if this pattern was scheduled
  if (_patternScheduledForRecording == pattern)
    {
      _patternScheduledForRecording = nullptr;
    }
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

  pattern->setPlayPosition (0.f);
}

void
MotionEngine::stop (std::shared_ptr<Pattern> pattern)
{
  pattern->setStatus (Pattern::Status::Idle);
  // _channels[pattern->_channel]->_patternPlaying = nullptr;
  // _channels[pattern->_channel]->_patternScheduledForPlaying = nullptr;
  _patternRecording = nullptr;
}

void
MotionEngine::performRecording ()
{
  if (!_patternRecording)
    {
      _recordingProgress.store (-1.f);

      // Armed but still waiting for its downbeat: nothing is written yet, but
      // the finger already steers the blob, so that it is under the finger the
      // moment the take does begin instead of jumping there.
      if (_patternScheduledForRecording && _recordingPosition.isValid ())
        _channels[_patternScheduledForRecording->getChannel ()]->setPosition (
            _recordingPosition);

      return;
    }

  auto const status = _patternRecording->getStatus ();
  auto const statusLast = _patternRecording->getLastStatus ();
  if (status == Pattern::Status::Recording
      || (status == Pattern::Status::ScheduledForIdle
          && statusLast == Pattern::Status::Recording)
      || (status == Pattern::Status::ScheduledForPlaying
          && statusLast == Pattern::Status::Recording))
    {
      // Store each recording position multiple times to get high-frequency sampling
      // This is done by recording the same position multiple times as time advances
      auto const ticksSinceStart = Measure::convertToTicks (
          _now - _recordingStarted, _tempoClock.getBeatsPerBar ());
      jassert (ticksSinceStart >= 0);

      auto const ticksPatternLength = _patternRecording->getNumTicks ();

      // Where the write head is, for whoever wants to show it. A take never
      // reaches updatePlayPosition, so the pattern's own play position stays
      // at zero and cannot answer this.
      if (ticksPatternLength > 0)
        _recordingProgress.store (
            static_cast<float> (static_cast<index_t> (ticksSinceStart)
                                % ticksPatternLength)
            / static_cast<float> (ticksPatternLength));
      
      // Map continuous time to pattern indices with sub-sampling
      // Each tick-advance gets _recordingSubSamplingFactor slots
      auto const baseIndex = static_cast<std::size_t> (ticksSinceStart) * _recordingSubSamplingFactor;
      
      // Record at each sub-sample slot for the current tick
      // This fills in gaps between ticks with interpolation-friendly keyframes
      // Store 2D positions so elevation coverage can be changed later
      //
      // A lifted finger writes nothing at all — punch-out. It used to write
      // Pos::invalid, which erased whatever an earlier pass had put there.
      // Since recording wraps and runs as many passes as you let it, that made
      // every pass wipe the one before it, and only the last one ever counted.
      // Protecting what is already there is what makes several passes worth
      // running: rough one out, then mend a corner.
      // The finger arrives as a direction on the sphere. A pattern stores 2D
      // so elevation coverage can be changed later, and mapTo2D is the exact
      // inverse of the mapTo3D playback uses — see
      // HeightMapSphere.MapTo2DRoundTripLeavesAPositionWhereItWas.
      auto const params = _patternRecording->getElevationParams ();
      auto const fingerDown = _recordingPosition.isValid ();

      if (fingerDown)
        {
          _recordingPosition2D = _heightMap.mapTo2D (_recordingPosition, params);
          _recordingHeldPosition2D = _recordingPosition2D;
          _recordingHasTouched = true;
        }

      // With the finger up, Latch and Write carry on writing where it was left
      // — or, in Write before it was ever put down, where the take started.
      auto const positionToWrite
          = fingerDown ? _recordingPosition2D : _recordingHeldPosition2D;

      if (shouldWriteTick (_automationMode.load (std::memory_order_relaxed),
                           fingerDown, _recordingHasTouched)
          && positionToWrite.isValid ())
        for (int slot = 0; slot < _recordingSubSamplingFactor; ++slot)
          {
            auto const tick = (baseIndex + slot) % ticksPatternLength;
            _patternRecording->setTick (tick, positionToWrite);
          }

      if (_recordingPosition.isValid ())
        {
          // The finger's own direction, not a round trip through the pattern
          // space — that is what puts the blob exactly under it.
          auto const recChannel = _patternRecording->getChannel ();
          auto pos3D = _recordingPosition;
          _channels[recChannel]->setPosition (pos3D);
        }
    }
}

void
MotionEngine::performPlayback ()
{
  for (auto chIdx = 0u; chIdx < _channels.size (); ++chIdx)
    {
      // A finger is on this one: playback keeps running, but it does not
      // get to write the position, or the blob slides out from under it.
      if (_positionHeld[chIdx].load (std::memory_order_relaxed))
        continue;

      // Same for the finger steering a take that has not started yet: the
      // outgoing clip carries on running, but it stops writing the position,
      // or it drags the blob back out from under the finger every tick.
      if (_patternScheduledForRecording && _recordingPosition.isValid ()
          && _patternScheduledForRecording->getChannel () == chIdx)
        continue;

      auto &channel = _channels[chIdx];
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
              auto const ticksPatternLength = channel->_patternPlaying->getNumTicks ();
              auto const ticksPlaybackLength = Measure::convertToTicks (
                  channel->_patternPlaying->getPlaybackLength (), _tempoClock.getBeatsPerBar ());

              // Use incremental position updates to handle tempo changes smoothly.
              // CRITICAL: Do NOT use fmod here - let playPosition grow beyond 1.0.
              // The interpolation function handles normalization and avoids precision issues.
              auto const playPositionDelta = 1. / static_cast<double> (ticksPlaybackLength);
              auto playPosition = channel->_patternPlaying->getPlayPosition ();
              playPosition += playPositionDelta;
              
              // Store the un-wrapped position for smooth loop transitions
              channel->_patternPlaying->setPlayPosition (static_cast<float> (playPosition));

              // Use interpolated playback for smooth motion between keyframes
              // The interpolation function handles wrapping at pattern boundaries
              auto const fractionalTick = ticksPatternLength * playPosition;
              auto position2D = channel->_patternPlaying->getInterpolatedTick (fractionalTick);
              
              if (position2D.isValid ())
                {
                  // Apply this clip's own elevation mapping (sphere
                  // projection) at playback time — elevation parameters
                  // live on the Pattern itself, not the channel.
                  auto const params
                      = channel->_patternPlaying->getElevationParams ();
                  auto position = _heightMap.mapTo3D (position2D, params);
                  channel->setPosition (position);
                }
            }
        }
    }
}

index_t
MotionEngine::updatePlayPosition (Pattern &pattern)
{
  auto const ticksPatternLength = pattern.getNumTicks ();
  auto const ticksPlaybackLength = Measure::convertToTicks (
      pattern.getPlaybackLength (), _tempoClock.getBeatsPerBar ());

  auto const playPositionDelta = 1. / double (ticksPlaybackLength);

  auto playPosition
      = std::fmod (pattern.getPlayPosition () + playPositionDelta, 1.);
  pattern.setPlayPosition (static_cast<float> (playPosition));

  auto step = static_cast<index_t> (ticksPatternLength * playPosition);
  jassert (step < ticksPatternLength);

  return step;
}

void
MotionEngine::notifyPatternStatusListeners (
    PatternStatusMessage::Status status, std::shared_ptr<Pattern> pattern)
{
  for (auto listener : _patternStatusListeners)
    {
      jassert (listener != nullptr);
      auto message = new PatternStatusMessage ();
      message->status = status;
      message->pattern = pattern;
      listener->postMessage (message);
    }
}
}
