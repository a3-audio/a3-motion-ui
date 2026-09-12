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

#include <a3-motion-engine/util/Slew.hh>

#include <a3-motion-engine/TempoLfo.hh>
#include <a3-motion-engine/TrajectoryShaping.hh>

#include <cstddef>

#include <a3-motion-engine/Channel.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/Playhead.hh>
#include <a3-motion-engine/UserConfig.hh>
#include <a3-motion-engine/OscAddresses.hh>
#include <a3-motion-engine/OscEndpoints.hh>
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

namespace
{
/** The backend the engine sends through, aimed at A3 Core.
 *
 *  Through loadOscEndpoints() rather than off the config directly, so that
 *  the spatial position and the mixer read where Core is from one function
 *  instead of each picking a key out of the same block by hand. */
std::unique_ptr<SpatBackendA3>
coreBackend ()
{
  auto const endpoints = loadOscEndpoints (userConfig);
  return std::make_unique<SpatBackendA3> (endpoints.host, endpoints.corePort,
                                          loadOscAddresses (userConfig));
}
}

MotionEngine::MotionEngine (index_t numChannels, HeightMap &heightMap)
    : _heightMap (heightMap), _commandQueue (coreBackend ())
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
MotionEngine::setOscAddresses (OscAddresses const &addresses)
{
  _commandQueue.setAddresses (addresses);
}

void
MotionEngine::createChannels (index_t const numChannels)
{
  _channels.resize (numChannels);
  _lastSentPositions.resize (numChannels);
  _positionHeld = std::vector<std::atomic<bool>> (numChannels);
  _lastSentPot1s.resize (numChannels);
  _lastSentPot2s.resize (numChannels);
  _lastSentPot3s.resize (numChannels);
  _accentHeld.assign (numChannels, 0);
  _accentEnvelope.resize (numChannels);
  _freqEnvelope.resize (numChannels);
  _qEnvelope.resize (numChannels);
  _accentPattern.resize (numChannels);
  _channelAction.resize (numChannels);
  _accentRestore.resize (numChannels);
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

float
MotionEngine::getChannelPot3 (index_t channel)
{
  return _channels[channel]->getPot3 ();
}

float
MotionEngine::getChannelPot3Effective (index_t channel)
{
  if (channel >= _accentEnvelope.size ())
    return getChannelPot3 (channel);

  // The ceiling belongs to the clip whose accent is running; with none, the
  // whole range, which is what it did before there was a ceiling at all.
  auto const &pattern = _accentPattern[channel];
  auto const max = pattern ? pattern->getEnvelopeMax () : 1.f;

  return envelopeOver (_channels[channel]->getPot3 (), max,
                       _accentEnvelope[channel].level);
}

float
MotionEngine::getChannelPot1Effective (index_t channel)
{
  if (channel >= _freqEnvelope.size ())
    return getChannelPot1 (channel);

  // With no clip firing there is nothing to sweep towards, and zero is below
  // every set value, so envelopeOver() leaves the encoder alone.
  auto const &pattern = _accentPattern[channel];
  auto const max = pattern ? pattern->getFreqMax () : 0.f;

  return envelopeOver (_channels[channel]->getPot1 (), max,
                       _freqEnvelope[channel].level);
}

float
MotionEngine::getChannelPot2Effective (index_t channel)
{
  if (channel >= _qEnvelope.size ())
    return getChannelPot2 (channel);

  auto const &pattern = _accentPattern[channel];
  auto const max = pattern ? pattern->getQMax () : 0.f;

  return envelopeOver (_channels[channel]->getPot2 (), max,
                       _qEnvelope[channel].level);
}

void
MotionEngine::setChannelPot3 (index_t channel, float pot3)
{
  _channels[channel]->setPot3 (pot3);
}

void
MotionEngine::advanceAccents ()
{
  auto const ticksPerBar = static_cast<float> (TempoClock::getTicksPerBeat ())
                           * static_cast<float> (_tempoClock.getBeatsPerBar ());

  for (auto index = 0u; index < _accentEnvelope.size (); ++index)
    {
      auto const &pattern = _accentPattern[index];

      // A channel that has never been accented has no shape to run and
      // nothing to run it on; the default steps stand in so a press before
      // any clip is loaded still does something rather than nothing.
      auto const attack
          = pattern ? pattern->getEnvelopeAttack () : 2;
      auto const decay = pattern ? pattern->getEnvelopeDecay () : 3;

      // The mode decides whether the finger holds the top: one-shot fires and
      // falls however long the pad is down, hold is the finger. This passed
      // the finger straight through, which made a one-shot a hold with another
      // name on it.
      auto const mode = pattern ? pattern->getActMode () : ActMode::OneShot;

      auto const fingerDown = _accentHeld[index] != 0;

      auto const before = _accentEnvelope[index].stage;
      _accentEnvelope[index] = advanceEnvelope (
          _accentEnvelope[index], mode, fingerDown, attack, decay,
          ticksPerBar);

      // Cutoff and resonance ride the same finger, each in its own time: a
      // slow sweep can sit under a short stab, and the resonance can arrive
      // long after the cutoff has or well before it.
      _freqEnvelope[index] = advanceEnvelope (
          _freqEnvelope[index], mode, fingerDown,
          pattern ? pattern->getFreqAttack () : 2,
          pattern ? pattern->getFreqDecay () : 3, ticksPerBar);
      _qEnvelope[index] = advanceEnvelope (
          _qEnvelope[index], mode, fingerDown,
          pattern ? pattern->getQAttack () : 2,
          pattern ? pattern->getQDecay () : 3, ticksPerBar);

      // The decay running out is the end of the gesture, so the clip does
      // whatever its end action says — the accent is a one-shot you played,
      // and what happens after a one-shot is a setting the clip already
      // carries. Only on the edge: once, as it lands on Idle.
      if (before == EnvelopeStage::Decay
          && _accentEnvelope[index].stage == EnvelopeStage::Idle)
        {
          // The end action first, and the fall back after it. An action
          // carries an end action like it carries everything else, and this
          // edge is the only moment one can act -- putting the clip back
          // first would hand that moment to the clip's own setting, so an
          // action that said "stop" would have said nothing at all.
          applyEndActionAfterAccent (index);
          restoreAfterAction (index);
        }

      // Let go of the clip once the accent is over, so a slot that was
      // replaced meanwhile is not kept alive by a finished gesture.
      if (_accentEnvelope[index].stage == EnvelopeStage::Idle
          && _accentHeld[index] == 0)
        _accentPattern[index] = nullptr;
    }
}

void
MotionEngine::applyEndActionAfterAccent (index_t channel)
{
  auto &playing = _channels[channel]->_patternPlaying;
  if (!playing)
    return;

  switch (playing->getEndAction ())
    {
    case EndAction::Stop:
      // Back to the beginning and out of playback, which is what a stop is —
      // the next start is visibly a start.
      playing->setPlayPosition (0.f);
      playing->setStatus (Pattern::Status::Idle);
      _channels[channel]->_patternPlaying = nullptr;
      break;

    case EndAction::Pause:
      // Standing still where it got to, and starting again from there.
      playing->setStatus (Pattern::Status::Idle);
      _channels[channel]->_patternPlaying = nullptr;
      break;

    case EndAction::Loop:
    case EndAction::Bounce:
    case EndAction::Random:
      // These say what to do at the *take's* end, not at the accent's, and
      // what they say is "keep going". Ending the clip here would make the
      // accent a stop button that only some settings noticed.
      break;
    }
}

void
MotionEngine::setChannelAccentHeld (index_t channel, bool held,
                                    std::shared_ptr<Pattern> pattern)
{
  if (channel >= _accentHeld.size ())
    return;

  _accentHeld[channel] = held ? 1 : 0;

  // The press is what starts both envelopes -- in either mode. Asking the
  // tick "is it held" starts nothing for a one-shot, because a one-shot is
  // never held; that is what left every one-shot silent.
  if (held)
    {
      _accentEnvelope[channel] = fireEnvelope (_accentEnvelope[channel]);
      _freqEnvelope[channel] = fireEnvelope (_freqEnvelope[channel]);
      _qEnvelope[channel] = fireEnvelope (_qEnvelope[channel]);
    }

  // The shape is the firing clip's, taken at the press and kept until the
  // envelope has finished — swapping clips mid-accent would change how long
  // the fall lasts while it is falling.
  if (held && pattern)
    {
      // Fired once, on the way down, and only onto a clip that is not already
      // wearing an action: a second press during the fall must not take the
      // action's own settings down as the thing to fall back to.
      if (_channelAction[channel] && !_accentRestore[channel])
        {
          _accentRestore[channel] = clipSettingsFrom (*pattern);
          applyClipSettings (
              *pattern,
              actionOver (*_accentRestore[channel], *_channelAction[channel]));
        }

      _accentPattern[channel] = std::move (pattern);
    }
}

void
MotionEngine::setChannelAction (index_t channel,
                                std::optional<ClipSettings> action)
{
  if (channel >= _channelAction.size ())
    return;

  _channelAction[channel] = std::move (action);
}

bool
MotionEngine::isChannelAccentActive (index_t channel) const
{
  if (channel >= _accentEnvelope.size ())
    return false;

  return _accentEnvelope[channel].stage != EnvelopeStage::Idle
         || _freqEnvelope[channel].stage != EnvelopeStage::Idle
         || _qEnvelope[channel].stage != EnvelopeStage::Idle
         || _accentRestore[channel].has_value ();
}

void
MotionEngine::restoreAfterAction (index_t channel)
{
  if (!_accentRestore[channel])
    return;

  if (auto const &pattern = _accentPattern[channel])
    applyClipSettings (*pattern, *_accentRestore[channel]);

  _accentRestore[channel].reset ();
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
MotionEngine::holdOutputUntil (double millisecondCounter)
{
  _outputHeldUntil.store (millisecondCounter, std::memory_order_relaxed);
}

bool
MotionEngine::outputHeld () const
{
  return juce::Time::getMillisecondCounterHiRes ()
         < _outputHeldUntil.load (std::memory_order_relaxed);
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
MotionEngine::setRecMode (RecMode mode)
{
  _recMode.store (mode, std::memory_order_relaxed);
}

RecMode
MotionEngine::getRecMode () const
{
  return _recMode.load (std::memory_order_relaxed);
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
  advanceAccents ();

  // Wall clock rather than ticks: a tick is two to eight milliseconds
  // depending on tempo, and a fade meant to be inaudible cannot be a
  // different length at 180 BPM than at 60.
  auto const nowMillis = juce::Time::getMillisecondCounterHiRes ();
  auto const elapsedMillis
      = _lastSendMillis > 0. ? nowMillis - _lastSendMillis : 0.;
  _lastSendMillis = nowMillis;

  // Asked once for the whole loop rather than per channel: the answer is the
  // same for all four, and reading a clock four times to get one answer is
  // four chances for them to disagree.
  auto const holding = outputHeld ();

  // compare with last enqueued values and enqueue on change
  for (auto index = 0u; index < _channels.size (); ++index)
    {
      // Skip OSC output for channels in preview mode
      if (_previewMode[index].load (std::memory_order_relaxed))
        continue;

      // Nothing at all until Core has had its chance to answer — and
      // nothing recorded as sent either, so what does go out afterwards is
      // measured against what the far end really has.
      if (holding)
        continue;

      auto const position = _channels[index]->getPosition ();
      if (position.isValid () && _lastSentPositions[index] != position)
        {
          _commandQueue.sendPosition (index, position);
          _lastSentPositions[index] = position;
        }

      // All three go out on a ramp rather than straight from the value they
      // have reached. A3 Core hands what it receives to a REAPER parameter
      // with no interpolation, so every message is a step and a step big
      // enough is a click in the room -- and the steps here are not small:
      // an accent's ceiling changes the instant its clip stops being the one
      // firing, a hardware pot arrives in its own increments, and at start-up
      // the very first value is a jump from wherever Core was left.
      //
      // It limits a rate, so anything already moving slower passes through
      // untouched: the shortest attack the envelope has is an eighth of a
      // second and never touches this. See util/Slew.hh.
      //
      // The ramp runs from the last value *sent*, which is what the far end
      // actually has -- ramping from the target would smooth nothing.
      // The very first value goes out whole. A ramp starts from what the far
      // end has, and at start-up nothing here knows what that is -- Core is
      // sitting wherever the last session left it. Ramping from this side's
      // zero would send Core *to* zero on the first message and climb back
      // up, which is a bigger jump than the one being smoothed, in the wrong
      // direction first. One honest jump beats a fade from a fiction.
      //
      // Since 2026-09-12 the recall answers for these three as well — the
      // pots through Core's reverse table, the crossfade out of Core's own
      // memory — and they arrive during the start-up hold, before anything
      // has been sent. So the first value that does go out is already the far
      // end's own, and the jump it would have been is gone.
      //
      // What is *not* done is priming _lastSentPot*s from it, so that first
      // send still happens and still goes out whole: one message telling Core
      // what Core just said. Harmless, and cheaper than reaching into the
      // tick thread's state from the message thread.
      // See issues/a3-motion-ui-total-recall-at-startup.md.
      auto const primed = _potsPrimed;

      auto const sendSlewed
          = [this, index, elapsedMillis, primed] (auto &lastSent, float target,
                                                  auto send) {
              auto const next
                  = primed ? slewTowards (lastSent[index], target,
                                          elapsedMillis, potSlewMillis)
                           : target;
              if (primed && juce::approximatelyEqual (lastSent[index], next))
                return;

              (_commandQueue.*send) (index, next);
              lastSent[index] = next;
            };

      // At rest all three are exactly what the hand set -- slewTowards()
      // lands on its target rather than approaching it, which is the whole
      // reason it is a ramp and not the one-pole this would usually be.
      sendSlewed (_lastSentPot1s, getChannelPot1Effective (index),
                  &AsyncCommandQueue::sendPot1);
      sendSlewed (_lastSentPot2s, getChannelPot2Effective (index),
                  &AsyncCommandQueue::sendPot2);
      // What the pot and the grid set is the floor; the accent raises it and
      // lets it back down to exactly there.
      sendSlewed (_lastSentPot3s, getChannelPot3Effective (index),
                  &AsyncCommandQueue::sendPot3);
    }

  // Not while the loop was skipped. "Primed" means the ramps have a value to
  // start from, and they only do once something has actually been sent --
  // while output is held, nothing was. Setting it anyway left every ramp
  // starting from a zero it had never sent, and any value that happened to
  // *be* zero was then never sent at all: it compared equal to what this side
  // wrongly believed the far end had. Measured on the rig, 2026-09-12 --
  // three of twelve channel values never reached Core, and they were exactly
  // the three sitting at 0.0.
  if (!holding)
    _potsPrimed = true;
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
  // Unturned, like the take was recorded. A clip that resumed wherever the
  // last pass stopped would come back somewhere different every time, which
  // is not something you can aim at.
  pattern->setSpinPhase (0.f);
  pattern->setReachLfoPhase (0.f);
  pattern->setElevationLfoPhase (0.f);
  pattern->setSqueezeXLfoPhase (0.f);
  pattern->setSqueezeYLfoPhase (0.f);

  // Reverse starts at the end and walks back, so the first tick has somewhere
  // to come from.
  auto const sign = initialSign (pattern->getPlayDirection ());
  pattern->setPlaySign (sign);
  if (sign < 0.f)
    pattern->setPlayPosition (1.f);
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

      if (shouldWriteTick (_recMode.load (std::memory_order_relaxed),
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

              // One tick on, in whichever direction the clip is travelling and
              // doing at the end whatever its end action says. The phase a
              // random end action would jump to is drawn here rather than in
              // there, so the decision itself stays a function that can be
              // checked.
              auto const playPositionDelta
                  = 1.f / static_cast<float> (ticksPlaybackLength);

              auto &playing = *channel->_patternPlaying;
              auto const stepped = advancePlayhead (
                  { playing.getPlayPosition (), playing.getPlaySign (), false },
                  playPositionDelta, playing.getEndAction (),
                  _random.nextFloat ());

              playing.setPlayPosition (stepped.position);
              playing.setPlaySign (stepped.sign);

              if (stepped.stopped)
                {
                  // Taken out of playback here, so nothing writes this
                  // channel's position again: the blob stands where the pass
                  // left it, which is what stopping at the end means.
                  playing.setStatus (Pattern::Status::Idle);
                  channel->_patternPlaying = nullptr;
                  continue;
                }

              auto const playPosition = stepped.position;

              // Use interpolated playback for smooth motion between keyframes.
              // Which ticks the pass spans depends on the end action: a loop
              // includes the seam back to the first tick, a bounce turns round
              // at the last one instead of running into it.
              auto const fractionalTick = fractionalTickForPlayback (
                  playPosition, ticksPatternLength, playing.getEndAction ());
              auto position2D = channel->_patternPlaying->getInterpolatedTick (fractionalTick);

              // The whole shape turns under the blob. One tick's worth here,
              // and the renderer turns the drawn line by the same phase — the
              // blob has to stay on its line.
              auto const ticksPerBar
                  = static_cast<float> (TempoClock::getTicksPerBeat ())
                    * static_cast<float> (_tempoClock.getBeatsPerBar ());
              playing.setSpinPhase (advanceLfoPhase (
                  playing.getSpinPhase (), playing.getSpin (), ticksPerBar));
              playing.setReachLfoPhase (
                  advanceLfoPhase (playing.getReachLfoPhase (),
                                   playing.getReachLfo (), ticksPerBar));
              playing.setElevationLfoPhase (
                  advanceLfoPhase (playing.getElevationLfoPhase (),
                                   playing.getElevationLfo (), ticksPerBar));
              playing.setSqueezeXLfoPhase (
                  advanceLfoPhase (playing.getSqueezeXLfoPhase (),
                                   playing.getSqueezeXLfo (), ticksPerBar));
              playing.setSqueezeYLfoPhase (
                  advanceLfoPhase (playing.getSqueezeYLfoPhase (),
                                   playing.getSqueezeYLfo (), ticksPerBar));

              if (position2D.isValid ())
                {
                  // Shaped before it is projected: in the recorded 2D disc
                  // the radius is the elevation and the angle is the azimuth,
                  // so turning the disc turns the trajectory around the pole
                  // and leaves every point at the height it was played in at,
                  // while squeezing an axis of it presses the figure flat
                  // without moving where it sits.
                  //
                  // One call, and the renderer makes the same one -- see
                  // shapedPosition(), which also fixes the order the two
                  // happen in.
                  position2D = shapedPosition (position2D, shapingOf (playing));

                  // Apply this clip's own elevation mapping (sphere
                  // projection) at playback time — elevation parameters
                  // live on the Pattern itself, not the channel.
                  auto params
                      = channel->_patternPlaying->getElevationParams ();
                  // ... with both slow sweeps laid over it, if they are
                  // sweeping. The renderer calls the same function from the
                  // same phases -- see sweptElevation() -- or the line would
                  // be drawn somewhere the blob is not running.
                  params = sweptElevation (params, playing);
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
