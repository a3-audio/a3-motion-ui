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

#include <a3-motion-engine/SpaceTurn.hh>
#include <a3-motion-engine/TakeProjection.hh>
#include <a3-motion-engine/TakeSeed.hh>

#include <a3-motion-engine/util/Slew.hh>

#include <a3-motion-engine/TempoLfo.hh>
#include <a3-motion-engine/TrajectoryShaping.hh>

#include <cstddef>

#include <a3-motion-engine/Channel.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/RecordingTrace.hh>
#include <a3-motion-engine/TakeLaps.hh>
#include <a3-motion-engine/Playhead.hh>
#include <a3-motion-engine/UserConfig.hh>
#include <a3-motion-engine/OscAddresses.hh>
#include <a3-motion-engine/OscEndpoints.hh>
#include <a3-motion-engine/backends/SpatBackendA3.hh>
#include <a3-motion-engine/elevation/HeightMap.hh>
#include <a3-motion-engine/util/Helpers.hh>
#include <a3-motion-engine/util/Timing.hh>
#include <a3-motion-engine/flight/BeatPulse.hh>
#include <a3-motion-engine/flight/Handover.hh>

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
/** The backend the engine sends through, aimed at A3 Core -- where the one
 *  truth says Core listens, through oscEndpointsFrom() like the mixer, so
 *  the spatial position and the mixer cannot disagree about where that is. */
std::unique_ptr<SpatBackendA3>
coreBackend ()
{
  auto const &truth = installedOscTruth ();
  auto const core = oscEndpointsFrom (truth).core;
  return std::make_unique<SpatBackendA3> (core.host, core.port,
                                          oscAddressesFrom (truth));
}

/** Passes the backend through, asserting it exists. Checked here, on the way
 *  into the member initialiser, because by the time the constructor body runs
 *  the parameter has already been moved from and is always null. */
std::unique_ptr<SpatBackend>
requireBackend (std::unique_ptr<SpatBackend> backend)
{
  jassert (backend != nullptr);
  return backend;
}

Vec2
onTheFloor (Pos const &position2D)
{
  return { position2D.x (), position2D.y () };
}
}

MotionEngine::MotionEngine (index_t numChannels, HeightMap &heightMap)
    : MotionEngine (numChannels, heightMap, coreBackend ())
{
}

MotionEngine::MotionEngine (index_t numChannels, HeightMap &heightMap,
                            std::unique_ptr<SpatBackend> backend)
    : _heightMap (heightMap),
      _commandQueue (requireBackend (std::move (backend)))
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
  _positionPacer = PositionPacer (numChannels);
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
  _followFrom.resize (numChannels);
  _follow.resize (numChannels);
  _accentRestore.resize (numChannels);
  _accentView = std::vector<AccentView> (numChannels);
  _freqView = std::vector<AccentView> (numChannels);
  _qView = std::vector<AccentView> (numChannels);
  _previewMode = std::vector<std::atomic<bool>> (numChannels);
  _flightMode = std::vector<std::atomic<int>> (numChannels);
  _flightTarget = std::vector<std::atomic<int>> (numChannels);
  for (auto &target : _flightTarget)
    target.store (noBodyId, std::memory_order_relaxed);
  _flightModeSeen.assign (numChannels, FlightMode::Clip);
  _flightModeThisTick.assign (numChannels, FlightMode::Clip);
  _glideTicksLeft.assign (numChannels, 0);
  _glideFrom.assign (numChannels, Pos::invalid);
  _glideInTicksLeft.assign (numChannels, 0);
  _glideInFrom.assign (numChannels, Pos::invalid);

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
  //
  // Out of the published view, not out of _accentPattern: this is called from
  // the message thread and from the OSC sender, and that is a shared_ptr the
  // clock thread writes.
  return envelopeOver (_channels[channel]->getPot3 (),
                       _accentView[channel].max.load (
                           std::memory_order_relaxed),
                       _accentView[channel].level.load (
                           std::memory_order_relaxed));
}

float
MotionEngine::getChannelPot1Effective (index_t channel)
{
  if (channel >= _freqEnvelope.size ())
    return getChannelPot1 (channel);

  // With no clip firing there is nothing to sweep towards, and zero is below
  // every set value, so envelopeOver() leaves the encoder alone.
  return envelopeOver (_channels[channel]->getPot1 (),
                       _freqView[channel].max.load (std::memory_order_relaxed),
                       _freqView[channel].level.load (
                           std::memory_order_relaxed));
}

float
MotionEngine::getChannelPot2Effective (index_t channel)
{
  if (channel >= _qEnvelope.size ())
    return getChannelPot2 (channel);

  return envelopeOver (_channels[channel]->getPot2 (),
                       _qView[channel].max.load (std::memory_order_relaxed),
                       _qView[channel].level.load (std::memory_order_relaxed));
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
          // Counted, not signalled: what a button's "then" fires is decided
          // on the message thread, which reads this and works the script
          // out there -- never here.
          _accentView[index].ends.fetch_add (1, std::memory_order_relaxed);
        }

      // Let go of the clip once the accent is over, so a slot that was
      // replaced meanwhile is not kept alive by a finished gesture.
      if (_accentEnvelope[index].stage == EnvelopeStage::Idle
          && _accentHeld[index] == 0)
        _accentPattern[index] = nullptr;
    }

  publishAccentView ();
}

/** What the readers may see, copied out once a tick.
 *
 *  The ceilings come from the firing clip and the levels from the envelopes,
 *  and both are read here -- on the thread that owns them -- rather than
 *  wherever somebody asks. With no clip firing the ceilings fall back to what
 *  the getters used to answer for a null pattern: the whole range for 3d,
 *  zero for freq and Q, which is below every set value and so leaves the
 *  encoders alone.
 */
void
MotionEngine::publishAccentView ()
{
  for (auto index = 0u; index < _accentEnvelope.size (); ++index)
    {
      auto const &pattern = _accentPattern[index];

      auto const publish = [] (AccentView &view, float level, float max) {
        view.level.store (level, std::memory_order_relaxed);
        view.max.store (max, std::memory_order_relaxed);
      };

      publish (_accentView[index], _accentEnvelope[index].level,
               pattern ? pattern->getEnvelopeMax () : 1.f);
      _accentView[index].active.store (
          _accentEnvelope[index].stage != EnvelopeStage::Idle
              || _freqEnvelope[index].stage != EnvelopeStage::Idle
              || _qEnvelope[index].stage != EnvelopeStage::Idle
              || _accentRestore[index].has_value (),
          std::memory_order_relaxed);
      publish (_freqView[index], _freqEnvelope[index].level,
               pattern ? pattern->getFreqMax () : 0.f);
      publish (_qView[index], _qEnvelope[index].level,
               pattern ? pattern->getQMax () : 0.f);
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
      // This says what to do at the *take's* end, not at the accent's, and
      // what it says is "keep going". Ending the clip here would make the
      // accent a stop button that only some settings noticed.
      break;

    case EndAction::Clip:
      // Like Loop, for a reason of its own: the follow is bound to the bar
      // grid the pass ends on, and an accent ends wherever a finger let go.
      // The chain carries on at the end of the pass.
      break;
    }
}

void
MotionEngine::setChannelAccentHeld (index_t channel, bool held,
                                    std::shared_ptr<Pattern> pattern)
{
  if (channel >= _accentHeld.size ())
    return;

  // Queued, not written here.
  //
  // This used to set _accentHeld, fire the envelopes and store the pattern
  // straight from the message thread, while the tempo-clock thread was
  // reading all three in advanceAccents() -- and clearing _accentPattern
  // whenever it saw an idle envelope with the finger up. Catch it mid-press
  // and it wiped the pattern of a running accent: the clip then kept the
  // action's values for ever, because restoreAfterAction() does nothing
  // without a pattern, and the envelope fell back to the default steps,
  // twelve seconds at 240 BPM. Measured 2026-09-21, see
  // issues/a3-motion-ui-der-oneshot-faellt-unter-last-nicht-zurueck.md.
  //
  // The queue is drained at the top of the tick, before advanceAccents(), so
  // a press between two ticks is in effect for the whole of the next one.
  // That is four milliseconds at 120 BPM -- the accent is still "now".
  Message message;
  message.command = Message::Command::SetAccentHeld;
  message.channel = channel;
  message.held = held;
  message.pattern = std::move (pattern);

  submitFifoMessage (message);
}

/** The press, on the thread that owns the accent state. */
void
MotionEngine::applyAccentHeld (index_t channel, bool held,
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
      // The clip's own settings are taken once, on the first press: a second
      // press during the fall must not take an action's settings down as the
      // thing to fall back to. What it wears is the latest press's action,
      // over those own settings -- six buttons per channel since 2026-09-27,
      // and the last one pressed wins.
      if (_channelAction[channel])
        {
          if (!_accentRestore[channel])
            _accentRestore[channel] = clipSettingsFrom (*pattern);
          applyClipSettings (
              *pattern,
              actionOver (*_accentRestore[channel], *_channelAction[channel]));
        }

      _accentPattern[channel] = std::move (pattern);
    }

  publishAccentView ();
}

void
MotionEngine::setChannelAction (index_t channel,
                                std::optional<ClipSettings> action)
{
  if (channel >= _channelAction.size ())
    return;

  // Queued for the same reason as the press: _channelAction is read on the
  // tick, and a press reads it to decide what to put on the clip.
  Message message;
  message.command = Message::Command::SetChannelAction;
  message.channel = channel;
  message.action = std::move (action);

  submitFifoMessage (message);
}

unsigned
MotionEngine::accentEndCount (index_t channel) const
{
  if (channel >= _accentView.size ())
    return 0u;
  return _accentView[channel].ends.load (std::memory_order_relaxed);
}

bool
MotionEngine::isChannelAccentActive (index_t channel) const
{
  // Read from the published view, not from the envelopes: those belong to
  // the tempo-clock thread, and this is asked from the message thread by the
  // bar and the pads page several times a second.
  if (channel >= _accentView.size ())
    return false;

  return _accentView[channel].active.load (std::memory_order_relaxed);
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
                             Measure timepoint, Measure length,
                             std::shared_ptr<Pattern> seed)
{
  // Laid out here, on the caller's thread, while the take is still nobody
  // else's: its path over the clip it starts from, moved into the whole
  // sphere. On the clock thread at the downbeat that cost a 64-bar take some
  // 43 ms -- eleven ticks at 120 BPM (2026-10-08).
  if (pattern)
    {
      // Fresh, so that nothing else reads it while it is laid out.
      jassert (pattern->getStatus () == Pattern::Status::Empty);
      prepareTake (*pattern, length, seed.get ());
    }

  Message message;
  message.command = Message::Command::StartRecording;
  message.pattern = pattern;
  message.seed = std::move (seed);
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
MotionEngine::pausePattern (std::shared_ptr<Pattern> pattern,
                            Measure timepoint)
{
  Message message;
  message.command = Message::Command::Stop;
  message.keepsPass = true;
  message.pattern = pattern;
  message.timepoint = timepoint;
  message.length = {};
  submitFifoMessage (message);
}

void
MotionEngine::stopPatternAtEnd (std::shared_ptr<Pattern> pattern)
{
  Message message;
  message.command = Message::Command::StopAtEnd;
  message.pattern = pattern;
  message.timepoint = {};
  message.length = {};
  submitFifoMessage (message);
}

void
MotionEngine::armFollowPattern (index_t channel, std::shared_ptr<Pattern> from,
                                std::shared_ptr<Pattern> follow)
{
  Message message;
  message.command = Message::Command::ArmFollow;
  message.channel = channel;
  message.pattern = std::move (from);
  message.follow = std::move (follow);
  submitFifoMessage (message);
}

void
MotionEngine::cancelScheduledPlay (std::shared_ptr<Pattern> pattern)
{
  Message message;
  message.command = Message::Command::CancelScheduledPlay;
  message.pattern = pattern;
  message.timepoint = {};
  message.length = {};
  submitFifoMessage (message);
}

void
MotionEngine::cancelScheduledRecording (std::shared_ptr<Pattern> pattern)
{
  Message message;
  message.command = Message::Command::CancelScheduledRecording;
  message.pattern = std::move (pattern);
  message.timepoint = {};
  message.length = {};
  submitFifoMessage (message);
}

bool
MotionEngine::isStoppingAtEnd (index_t channel) const
{
  if (channel >= _channels.size ())
    return false;

  auto const &playing = _channels[channel]->_patternPlaying;
  return playing != nullptr && playing->getStopAtEnd ();
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

void
MotionEngine::setFlightMode (index_t channel, FlightMode mode)
{
  if (channel >= _flightMode.size ()
      || channel >= static_cast<index_t> (flightShips))
    return;
  _flightMode[channel].store (static_cast<int> (mode),
                              std::memory_order_relaxed);
}

FlightMode
MotionEngine::getFlightMode (index_t channel) const
{
  if (channel >= _flightMode.size ())
    return FlightMode::Clip;
  return static_cast<FlightMode> (
      _flightMode[channel].load (std::memory_order_relaxed));
}

void
MotionEngine::setFlightTarget (index_t channel, int bodyId)
{
  if (channel >= _flightTarget.size ())
    return;
  _flightTarget[channel].store (bodyId, std::memory_order_relaxed);
}

void
MotionEngine::setFlightBodies (FlightBodies const &bodies)
{
  _bodies.write (bodies);
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

  readFlightModes ();
  
  // Perform playback once per tick. With absolute time-based position calculation,
  // we don't need to worry about accumulation errors or sub-stepping granularity.
  // The position is always precisely calculated from elapsed time.
  performPlayback ();
  performFlight ();
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
      // Paced, not every tick: ~260 a second per channel filled Core's
      // port under load. A skipped position stays "not sent" and goes out
      // on a later tick, so the newest always arrives.
      if (position.isValid () && _lastSentPositions[index] != position
          && _positionPacer.due (index, nowMillis))
        {
          _commandQueue.sendPosition (index, position);
          _lastSentPositions[index] = position;
          _positionPacer.sent (index, nowMillis);
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
    case Message::Command::SetAccentHeld:
      {
        applyAccentHeld (message.channel, message.held, message.pattern);
        break;
      }
    case Message::Command::SetChannelAction:
      {
        if (message.channel < _channelAction.size ())
          _channelAction[message.channel] = message.action;
        break;
      }
    case Message::Command::ArmFollow:
      {
        if (message.channel < _follow.size ())
          {
            _followFrom[message.channel] = message.pattern;
            _follow[message.channel] = message.follow;
          }
        break;
      }
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
    case Message::Command::CancelScheduledPlay:
      {
        // Nothing queued and no timepoint: the pointer startPlaying() checks
        // is cleared here and now, so the message still sitting in the queue
        // finds nothing to start when its beat comes round.
        if (!message.pattern)
          break;

        auto &channel = *_channels[message.pattern->getChannel ()];
        if (channel._patternScheduledForPlaying != message.pattern)
          break;

        channel._patternScheduledForPlaying = nullptr;
        message.pattern->restoreStatus ();
        break;
      }
    case Message::Command::CancelScheduledRecording:
      {
        // As CancelScheduledPlay: the pointer startRecording() checks goes,
        // and the start still queued finds nothing to start.
        if (!message.pattern
            || _patternScheduledForRecording != message.pattern)
          break;

        _patternScheduledForRecording = nullptr;
        message.pattern->restoreStatus ();
        break;
      }
    case Message::Command::StopAtEnd:
      {
        // Nothing is scheduled and nothing is queued: the moment is not a
        // timepoint, it is wherever the pass runs out. Marked on the pattern,
        // and performPlayback() reads it when the lap ends.
        if (message.pattern)
          message.pattern->setStopAtEnd (true);
        break;
      }
    }
}

void
MotionEngine::scheduledForRecording (std::shared_ptr<Pattern> pattern,
                                     Measure timepoint)
{
  // The timepoint belongs to the queued message, which is what actually fires
  // the take; nothing here needs it. Kept in the signature rather than dropped
  // so this reads like its two siblings, which do take one.
  juce::ignoreUnused (timepoint);

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
            startRecording (message.pattern, message.length, message.seed);

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
            stop (message.pattern, message.keepsPass);

            notifyPatternStatusListeners (
                PatternStatusMessage::Status::Stopped, message.pattern);
            break;
          }
        case Message::Command::SetRecordingPosition:
        case Message::Command::ReleaseRecordingPosition:
        case Message::Command::SetRecordingMode:
        case Message::Command::ArmFollow:
          {
            throw std::runtime_error (
                "invalid command message in start/stop queue");
            break;
          }
        }
    }
}

bool
MotionEngine::takeWroteSomething () const
{
  return _takeWrote.load (std::memory_order_relaxed);
}

void
MotionEngine::startRecording (std::shared_ptr<Pattern> pattern, Measure length,
                              std::shared_ptr<Pattern> const &seed)
{
  if (!pattern)
    return;

  // A start whose take is no longer the one scheduled: called off, or
  // replaced by a later REC (see scheduledForRecording()).
  if (_patternScheduledForRecording != pattern)
    return;

  // Stop any currently recording pattern
  if (_patternRecording && _patternRecording != pattern)
    {
      _patternRecording->setStatus (Pattern::Status::Idle);
    }
  
  _patternRecording = pattern;

  // A take is the finger's path, not the physics.
  if (pattern->getChannel () < _flightMode.size ())
    _flightMode[pattern->getChannel ()].store (
        static_cast<int> (FlightMode::Clip), std::memory_order_relaxed);

  // A new take starts on its first lap, with no finished one behind it.
  _recordingLastComplete.clear ();
  _recordingTicks = 0;
  _recordingLap = 0;
  _recordingTicksAtLift = 0;
  _recordingFingerWasDown = false;
  // A new take is new knob recorders too: what they remember is about this
  // take. Its lanes were laid out with its path in prepareTake().
  _knobRecorders = KnobRecorders{};
  _takeWrote.store (false, std::memory_order_relaxed);

  // Calculate adaptive sub-sampling factor based on recording length
  _recordingSubSamplingFactor = calculateSubSamplingFactor (length, _tempoClock.getBeatsPerBar ());
#ifdef DEBUG
  juce::Logger::writeToLog ("Recording with sub-sampling factor: " + juce::String (_recordingSubSamplingFactor));
#endif

  // Its ticks, the clip's path in them and its band were laid out when it
  // was asked for -- prepareTake().
  (void) seed;

  _recordingPosition = Pos::invalid;
  _recordingPosition2D = Pos::invalid;
  _recordingStarted = _now;
  _recordingProgress.store (0.f);

  // Write starts overwriting from its first tick, before anything has been
  // touched, so it needs something to write: where the blob stands as the take
  // begins. Touch and Latch never reach for this.
  _recordingHasTouched = false;
  _recordingHeldDirection = _channels[pattern->getChannel ()]->getPosition ();
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
  // A decision made about a previous lap is not this lap's.
  channel._patternPlaying->setStopAtEnd (false);
  channel._playingStarted = _now;

  channel._patternScheduledForPlaying = nullptr;
  finishRecording ();

  // After a pause the pass goes on where it was left; once.
  if (pattern->resumesOnPlay ())
    {
      pattern->setResumesOnPlay (false);
      return;
    }
  beginPass (*pattern);
}

void
MotionEngine::beginPass (Pattern &pattern)
{
  pattern.setPlayPosition (0.f);
  pattern.setLap (0, 0.f);
  // Unturned, like the take was recorded. A clip that resumed wherever the
  // last pass stopped would come back somewhere different every time, which
  // is not something you can aim at.
  pattern.setSpinPhase (0.f);
  pattern.setReachLfoPhase (0.f);
  pattern.setElevationLfoPhase (0.f);
  pattern.setSqueezeXLfoPhase (0.f);
  pattern.setSqueezeYLfoPhase (0.f);
  pattern.setTiltLfoPhase (0.f);
  pattern.setRollLfoPhase (0.f);

  // Reverse starts at the end and walks back, so the first tick has somewhere
  // to come from; Random drops in at a random phase.
  auto const direction = pattern.getPlayDirection ();
  pattern.setPlaySign (initialSign (direction));
  auto const from = initialPosition (direction, _random.nextFloat ());
  if (from > 0.f)
    pattern.setPlayPosition (from);
}

bool
MotionEngine::rewindToTheBar (Pattern &pattern)
{
  auto const beatsPerBar = _tempoClock.getBeatsPerBar ();
  auto const ticksPerBar = TempoClock::getTicksPerBeat () * beatsPerBar;
  auto const passTicks
      = Measure::convertToTicks (pattern.getPlaybackLength (), beatsPerBar);
  if (passTicks <= 0 || ticksPerBar <= 0)
    return false;

  auto const now = Measure::convertToTicks (_now, beatsPerBar);
  auto const intoTheBar = now % ticksPerBar;
  auto const channel = pattern.getChannel ();
  auto const played
      = channel < _channels.size ()
            ? now
                  - Measure::convertToTicks (_channels[channel]->_playingStarted,
                                             beatsPerBar)
            : 0;
  // Started inside this bar: there is no bar start of its own to go back to,
  // and the top is where it began.
  if (intoTheBar > played)
    return false;

  auto const ticks = static_cast<index_t> (intoTheBar);
  auto const rewound = rewoundPlayhead (
      { pattern.getPlayPosition (), pattern.getPlaySign (), false }, ticks,
      1.f / static_cast<float> (passTicks), pattern.getPlayDirection (),
      pattern.getEndAction ());
  if (!rewound)
    return false;

  pattern.setPlayPosition (rewound->position);
  pattern.setPlaySign (rewound->sign);
  auto const lapLength = static_cast<index_t> (passTicks);
  auto const lapTick
      = rewoundLapTick (pattern.getLapTick (), ticks, lapLength);
  pattern.setLap (lapTick, lapProgress (lapTick, lapLength));

  // The slow movements go back with the place, at the rate their knobs stand
  // at now: the replayed bar is the bar that was heard, as far as a knob that
  // has not been turned since goes.
  auto const back = static_cast<float> (ticks);
  auto const perBar = static_cast<float> (ticksPerBar);
  pattern.setSpinPhase (rewoundLfoPhase (
      pattern.getSpinPhase (), pattern.getKnobStep (Knob::Spin), back, perBar));
  pattern.setReachLfoPhase (
      rewoundLfoPhase (pattern.getReachLfoPhase (),
                       pattern.getKnobStep (Knob::Swell), back, perBar));
  pattern.setElevationLfoPhase (
      rewoundLfoPhase (pattern.getElevationLfoPhase (),
                       pattern.getKnobStep (Knob::Sway), back, perBar));
  pattern.setSqueezeXLfoPhase (
      rewoundLfoPhase (pattern.getSqueezeXLfoPhase (),
                       pattern.getKnobStep (Knob::StretchX), back, perBar));
  pattern.setSqueezeYLfoPhase (
      rewoundLfoPhase (pattern.getSqueezeYLfoPhase (),
                       pattern.getKnobStep (Knob::StretchY), back, perBar));
  pattern.setTiltLfoPhase (
      rewoundLfoPhase (pattern.getTiltLfoPhase (),
                       pattern.getKnobStep (Knob::TiltSweep), back, perBar));
  pattern.setRollLfoPhase (
      rewoundLfoPhase (pattern.getRollLfoPhase (),
                       pattern.getKnobStep (Knob::RollSweep), back, perBar));
  return true;
}

void
MotionEngine::stop (std::shared_ptr<Pattern> pattern, bool keepsPass)
{
  // A pause keeps the place (2026-10-08). Made on the downbeat it keeps it
  // as it is; made at once (Shift) it goes back by the ticks since the
  // music's last downbeat, so the resume -- on a downbeat -- plays that bar
  // again. Only a clip that was playing has a place to keep.
  auto const wasPlaying
      = pattern->getStatus () == Pattern::Status::Playing
        || (pattern->getStatus () == Pattern::Status::ScheduledForIdle
            && pattern->getLastStatus () == Pattern::Status::Playing);
  pattern->setResumesOnPlay (keepsPass && wasPlaying
                             && rewindToTheBar (*pattern));

  pattern->setStatus (Pattern::Status::Idle);
  // The lap it was asked to finish is over either way.
  pattern->setStopAtEnd (false);
  // _channels[pattern->_channel]->_patternPlaying = nullptr;
  // _channels[pattern->_channel]->_patternScheduledForPlaying = nullptr;
  finishRecording ();
}

void
MotionEngine::finishRecording ()
{
  if (!_patternRecording)
    return;

  // The lap that was still being played when the take stopped is dropped: it
  // would meet the lap before it mid-figure, and that join is the jump that
  // reads as a hole. A take that never finished a lap keeps what it has --
  // there would be nothing else to play.
  auto const lapTicks = static_cast<long long> (_patternRecording->getNumTicks ());
  if (!takeKeepsPartialLap (_recordingTicks, lapTicks)
      && static_cast<long long> (_recordingLastComplete.size ()) == lapTicks)
    for (index_t tick = 0; tick < static_cast<index_t> (lapTicks); ++tick)
      _patternRecording->setTick (tick,
                                  _recordingLastComplete[static_cast<std::size_t> (tick)]);

  // Nothing writes the knobs any more; the red goes back to what plays.
  _patternRecording->stopKnobWriting ();

  if (RecordingTrace::device ().isEnabled ())
    RecordingTrace::device ().finished (
        _patternRecording->getName (),
        _patternRecording->getTicks ().positions);

  _patternRecording = nullptr;
  _recordingLastComplete.clear ();
  _recordingTicks = 0;
  _recordingLap = 0;
}

/** Lay onto `take` the sweeps' phases its first pass will have at `tick`
 *  -- so writing and showing a point use the transform it will be played
 *  through, spin and sweeps included. A take is not playing while it
 *  records, so its phases are free; beginPass() zeroes them when it starts. */
void
MotionEngine::takePhasesAt (Pattern &take, index_t tick) const
{
  firstPassPhasesAt (take, tick, _tempoClock.getBeatsPerBar (),
                     _recordingSubSamplingFactor);
}

void
MotionEngine::firstPassPhasesAt (Pattern &take, index_t tick, int beatsPerBar,
                                 int subSampling)
{
  auto const ticksPerBar = static_cast<float> (TempoClock::getTicksPerBeat ())
                           * static_cast<float> (beatsPerBar);
  auto const numTicks = take.getNumTicks ();

  // The pass is as long as the take plays, which is not always as long as it
  // records; a take with no playback length set plays at its own.
  auto passTicks = static_cast<float> (
      Measure::convertToTicks (take.getPlaybackLength (), beatsPerBar));
  if (passTicks <= 0.f)
    passTicks = static_cast<float> (numTicks)
                / static_cast<float> (std::max (subSampling, 1));

  setPassPhases (take,
                 ticksIntoFirstPass (tick, numTicks, take.getPlayDirection (),
                                     passTicks),
                 ticksPerBar);
}

void
MotionEngine::prepareTake (Pattern &take, Measure length,
                           Pattern const *seed) const
{
  auto const beatsPerBar = _tempoClock.getBeatsPerBar ();
  auto const subSampling = calculateSubSamplingFactor (length, beatsPerBar);
  auto const ticks = Measure::convertToTicks (length, beatsPerBar);
  jassert (ticks >= 0);

  // A new take is new knob lanes too: they belong to the path they were
  // turned over.
  take.clearLanes ();
  take.clear ();
  take.resize (static_cast<std::size_t> (std::max<long long> (ticks, 0))
               * static_cast<std::size_t> (subSampling));

  // Over the clip the slot held: TOUCH then changes only what is touched.
  if (seed)
    seedTake (take, *seed);
  openTakeToTheWholeSphere (take, beatsPerBar, subSampling);
  take.setBandHeld (true);
}

/** The take's band becomes the whole sphere (openToTheWholeSphere), and
 *  every point already in it -- the clip it started from -- is moved into the
 *  new band where it was heard, tick by tick through the clip's own band,
 *  lanes and sweeps at the phase its first pass reaches that tick with. */
void
MotionEngine::openTakeToTheWholeSphere (Pattern &take, int beatsPerBar,
                                        int subSampling) const
{
  auto const numTicks = take.getNumTicks ();
  auto const ticks = take.getTicks ().positions;

  std::vector<Pos> heard (numTicks, Pos::invalid);
  for (index_t tick = 0; tick < numTicks; ++tick)
    if (ticks[tick].isValid ())
      {
        firstPassPhasesAt (take, tick, beatsPerBar, subSampling);
        take.playKnobs (static_cast<double> (tick));
        heard[tick] = playedPosition (_heightMap, ticks[tick], take);
      }

  openToTheWholeSphere (take);

  for (index_t tick = 0; tick < numTicks; ++tick)
    if (heard[tick].isValid ())
      {
        firstPassPhasesAt (take, tick, beatsPerBar, subSampling);
        take.playKnobs (static_cast<double> (tick));
        take.setTick (tick, writtenPosition (_heightMap, heard[tick], take));
      }
}

/** Where `direction` is heard once a take has written it at `tick`: the
 *  direction itself inside the take's band, its nearest playable neighbour
 *  outside it (#66). Leaves `take` at that tick's phases. */
Pos
MotionEngine::heardInTake (Pos const &direction, Pattern &take,
                           index_t tick) const
{
  takePhasesAt (take, tick);
  return playedPosition (_heightMap,
                         writtenPosition (_heightMap, direction, take), take);
}

void
MotionEngine::performRecording ()
{
  if (!_patternRecording)
    {
      _recordingProgress.store (-1.f);

      // Armed but still waiting for its downbeat: nothing is written yet, but
      // the finger already steers the blob, so that it is under the finger the
      // moment the take does begin instead of jumping there. The take records
      // over the whole sphere, so the finger is where it will be heard.
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

      // Each lap writes over the one before, so the take is only ever one lap
      // deep. What the lap that just ended left behind is kept, because an
      // unfinished lap is dropped when the take stops -- see finishRecording()
      // and TakeLaps.hh.
      _recordingTicks = static_cast<long long> (ticksSinceStart);
      if (ticksPatternLength > 0)
        {
          auto const lap = _recordingTicks / ticksPatternLength;
          if (lap != _recordingLap)
            {
              _recordingLap = lap;
              _recordingLastComplete = _patternRecording->getTicks ().positions;
            }
        }

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
      // The knobs first: they are written by the same rule as the path and
      // played back at once, so a lap turned over is heard -- and drawn -- on
      // the next, and the path below is undone through the knobs as they
      // stand at this tick.
      {
        auto const mode = _recMode.load (std::memory_order_relaxed);
        for (int slot = 0; slot < _recordingSubSamplingFactor; ++slot)
          if (_patternRecording->recordKnobs (
                  _knobRecorders, mode,
                  static_cast<long long> (baseIndex + slot),
                  static_cast<long long> (ticksPatternLength)))
            _takeWrote.store (true, std::memory_order_relaxed);
        _patternRecording->playKnobs (static_cast<double> (
            baseIndex % std::max<std::size_t> (ticksPatternLength, 1)));
      }

      // A lifted finger writes nothing at all — punch-out. It used to write
      // Pos::invalid, which erased whatever an earlier pass had put there.
      // Since recording wraps and runs as many passes as you let it, that made
      // every pass wipe the one before it, and only the last one ever counted.
      // Protecting what is already there is what makes several passes worth
      // running: rough one out, then mend a corner.
      //
      // The finger arrives as a direction on the sphere and is written through
      // the inverse of what playback will do to that tick (writtenPosition):
      // the clip's lean, band, squeeze and turn, spin and sweeps at the phase
      // the first pass reaches that tick with. So the take plays back where
      // the finger was (decided 2026-10-07, #66) -- or, outside the band, at
      // the nearest direction it can play.
      auto const fingerDown = _recordingPosition.isValid ();
      auto const writeTick = baseIndex % std::max<std::size_t> (
                                             ticksPatternLength, 1);

      if (fingerDown)
        {
          _recordingHeldDirection = heardInTake (
              _recordingPosition, *_patternRecording, writeTick);
          _recordingHasTouched = true;
        }
      else if (_recordingFingerWasDown)
        {
          // Where the write head was when the finger left: the hold belongs
          // to that lap and ends with it. See RecMode::shouldWriteTick.
          _recordingTicksAtLift = static_cast<long long> (ticksSinceStart);
        }
      _recordingFingerWasDown = fingerDown;

      // With the finger up, Latch and Write carry on writing where it was left
      // — or, in Write before it was ever put down, where the take started.
      auto const directionToWrite
          = fingerDown ? _recordingPosition : _recordingHeldDirection;

      if (shouldWriteTick (_recMode.load (std::memory_order_relaxed),
                           { fingerDown, _recordingHasTouched,
                             _recordingTicksAtLift,
                             static_cast<long long> (ticksSinceStart),
                             static_cast<long long> (ticksPatternLength) })
          && directionToWrite.isValid ())
        {
          for (int slot = 0; slot < _recordingSubSamplingFactor; ++slot)
            {
              auto const tick = (baseIndex + slot) % ticksPatternLength;
              takePhasesAt (*_patternRecording, tick);
              auto const written = writtenPosition (
                  _heightMap, directionToWrite, *_patternRecording);
              _patternRecording->setTick (tick, written);
              _takeWrote.store (true, std::memory_order_relaxed);

              if (RecordingTrace::device ().isEnabled ())
                RecordingTrace::device ().wrote (
                    static_cast<int> (tick), written.x (), written.y (),
                    fingerDown, static_cast<long long> (ticksSinceStart));
            }
          // Back to the write head's tick, where the blob is: the take is
          // drawn turned by the phase it is heard at.
          takePhasesAt (*_patternRecording, writeTick);
        }

      if (fingerDown)
        {
          // What was written, played back -- not the finger's own direction.
          // Inside the band the two are the same point; outside it the finger
          // used to lead the blob out of the band, which sounded right while
          // drawing and played back somewhere else (#66).
          _channels[_patternRecording->getChannel ()]->setPosition (
              _recordingHeldDirection);
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

          if (passIsRunning (*channel->_patternPlaying))
            {
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
                  playPositionDelta, playing.getPlayDirection (),
                  playing.getEndAction (), _random.nextFloat (),
                  playing.getStopAtEnd ());

              playing.setPlayPosition (stepped.position);
              playing.setPlaySign (stepped.sign);

              auto const lapLength
                  = static_cast<index_t> (ticksPlaybackLength);
              auto const lapTick
                  = nextLapTick (playing.getLapTick (), lapLength);
              playing.setLap (lapTick, lapProgress (lapTick, lapLength));

              if (stepped.stopped)
                {
                  // Only a pass that ran out on its own hands over: not one a
                  // Play press asked to finish, and not one a stop or a take
                  // is already scheduled over.
                  auto const follows
                      = playing.getEndAction () == EndAction::Clip
                        && !playing.getStopAtEnd ()
                        && status == Pattern::Status::Playing;

                  // Taken out of playback here, so nothing writes this
                  // channel's position again: the blob stands where the pass
                  // left it, which is what stopping at the end means.
                  playing.setStatus (Pattern::Status::Idle);
                  playing.setStopAtEnd (false);

                  auto const follow
                      = follows ? startFollow (chIdx, playing) : nullptr;
                  channel->_patternPlaying = follow;
                  if (!follow)
                    continue;

                  // The follow's first tick is this one -- the tick a looping
                  // clip would have been back at its top on -- so there is no
                  // tick without a clip and none with two.
                  playTick (chIdx, *follow, follow->getPlayPosition ());
                  notifyPatternStatusListeners (
                      PatternStatusMessage::Status::Playing, follow);
                  continue;
                }

              playTick (chIdx, playing, stepped.position);
            }
        }
    }
}

std::shared_ptr<Pattern>
MotionEngine::startFollow (index_t channel, Pattern const &playing)
{
  if (channel >= _follow.size () || _followFrom[channel].get () != &playing)
    return nullptr;

  auto follow = std::move (_follow[channel]);
  _follow[channel] = nullptr;
  _followFrom[channel] = nullptr;
  if (!follow)
    return nullptr;

  follow->setStatus (Pattern::Status::Playing);
  follow->setStopAtEnd (false);
  _channels[channel]->_playingStarted = _now;
  beginPass (*follow);
  return follow;
}

void
MotionEngine::playTick (index_t chIdx, Pattern &playing, float playPosition)
{
  auto &channel = _channels[chIdx];
  auto const ticksPatternLength = playing.getNumTicks ();

  // Use interpolated playback for smooth motion between keyframes.
  // Which ticks the pass spans depends on the end action: a loop
  // includes the seam back to the first tick, a bounce turns round
  // at the last one instead of running into it.
  auto const fractionalTick = fractionalTickForPlayback (
      playPosition, ticksPatternLength, playing.getPlayDirection ());
  auto position2D = playing.getInterpolatedTick (fractionalTick);

  // Before anything reads a knob: the lanes play over the
  // settings this tick -- see Pattern::getKnob().
  playing.playKnobs (fractionalTick);

  // The whole shape turns under the blob. One tick's worth here,
  // and the renderer turns the drawn line by the same phase — the
  // blob has to stay on its line.
  auto const ticksPerBar
      = static_cast<float> (TempoClock::getTicksPerBeat ())
        * static_cast<float> (_tempoClock.getBeatsPerBar ());
  playing.setSpinPhase (advanceLfoPhase (
      playing.getSpinPhase (), playing.getKnobStep (Knob::Spin),
      ticksPerBar));
  playing.setReachLfoPhase (
      advanceLfoPhase (playing.getReachLfoPhase (),
                       playing.getKnobStep (Knob::Swell),
      ticksPerBar));
  playing.setElevationLfoPhase (
      advanceLfoPhase (playing.getElevationLfoPhase (),
                       playing.getKnobStep (Knob::Sway),
      ticksPerBar));
  playing.setSqueezeXLfoPhase (
      advanceLfoPhase (playing.getSqueezeXLfoPhase (),
                       playing.getKnobStep (Knob::StretchX),
      ticksPerBar));
  playing.setSqueezeYLfoPhase (
      advanceLfoPhase (playing.getSqueezeYLfoPhase (),
                       playing.getKnobStep (Knob::StretchY),
      ticksPerBar));
  playing.setTiltLfoPhase (advanceLfoPhase (
      playing.getTiltLfoPhase (),
      playing.getKnobStep (Knob::TiltSweep), ticksPerBar));
  playing.setRollLfoPhase (advanceLfoPhase (
      playing.getRollLfoPhase (),
      playing.getKnobStep (Knob::RollSweep), ticksPerBar));

  // Shaped, projected through the band and leant -- one call, which the
  // recording side inverts (writtenPosition) and the renderer mirrors.
  //
  // Not for an ORBIT channel: its ship writes the position, in
  // performFlight(). Everything above still runs, so the clip comes back in
  // phase with the bar and with its lanes.
  if (position2D.isValid ()
      && _flightModeThisTick[chIdx] == FlightMode::Clip)
    channel->setPosition (playedPosition (_heightMap, position2D, playing));
}

bool
MotionEngine::positionTakenOver (index_t channel) const
{
  if (_positionHeld[channel].load (std::memory_order_relaxed))
    return true;
  if (_patternRecording && _patternRecording->getChannel () == channel)
    return true;
  return _patternScheduledForRecording && _recordingPosition.isValid ()
         && _patternScheduledForRecording->getChannel () == channel;
}

void
MotionEngine::setFlightBreath (bool on)
{
  _flightBreath.store (on, std::memory_order_relaxed);
}

bool
MotionEngine::getFlightBreath () const
{
  return _flightBreath.load (std::memory_order_relaxed);
}

void
MotionEngine::readFlightModes ()
{
  // One read per channel per tick: playTick and performFlight must agree on
  // the mode, or a switch landing between them could leave the channel with
  // two writers or none for that tick.
  for (auto ch = 0u; ch < _flightModeThisTick.size (); ++ch)
    _flightModeThisTick[ch] = getFlightMode (ch);
}

void
MotionEngine::performFlight ()
{
  // Refused mid-write: last tick's floor stays, one tick old. Never waits.
  _bodies.read (_flightBodies);

  auto const beatsPerBar = _tempoClock.getBeatsPerBar ();
  auto const ticksPerBeat = TempoClock::getTicksPerBeat ();
  auto const beats = static_cast<double> (
                         Measure::convertToTicks (_now, beatsPerBar))
                     / ticksPerBeat;

  std::array<ShipOrders, flightShips> orders{};
  std::array<Pattern const *, flightShips> flyingClip{};

  auto const ships
      = std::min (_channels.size (), static_cast<std::size_t> (flightShips));
  for (auto ch = 0u; ch < ships; ++ch)
    {
      auto const *playing = _channels[ch]->_patternPlaying.get ();
      auto const running = playing != nullptr && passIsRunning (*playing);

      // A finger or a take wins, as it does over playback; and with no clip
      // running there is nothing to fly. Either way the ship is no longer
      // where it was: it launches afresh, from wherever the channel is then.
      if (positionTakenOver (ch) || !running)
        {
          _flightModeSeen[ch] = FlightMode::Clip;
          _glideTicksLeft[ch] = 0;
          _glideInTicksLeft[ch] = 0;
          continue;
        }

      if (_flightModeThisTick[ch] == FlightMode::Clip)
        {
          if (_flightModeSeen[ch] == FlightMode::Orbit)
            startGlide (ch);
          _flightModeSeen[ch] = FlightMode::Clip;
          _glideInTicksLeft[ch] = 0;
          glideTowardsTheClip (ch);
          continue;
        }

      _glideTicksLeft[ch] = 0;
      if (_flightModeSeen[ch] == FlightMode::Clip)
        {
          auto const here = _heightMap.mapTo2D (
              _channels[ch]->getPosition (), playing->getElevationParams ());
          _flight.launch (static_cast<int> (ch), onTheFloor (here), beats,
                          beatsPerBar);
          startGlideIn (ch);
        }
      _flightModeSeen[ch] = FlightMode::Orbit;

      auto const target = _flightTarget[ch].load (std::memory_order_relaxed);
      orders[ch] = { true,
                     target == noBodyId ? FlightGoal::Patrol
                                        : FlightGoal::Escort,
                     target };
      flyingClip[ch] = playing;
    }

  _flight.setBreathing (_flightBreath.load (std::memory_order_relaxed));
  _flight.step (orders, _flightBodies, beats, beatsPerBar,
                gravityPulse (_now, beatsPerBar, _flightTuning),
                1.f / static_cast<float> (ticksPerBeat));

  for (auto ch = 0u; ch < ships; ++ch)
    {
      if (!orders[ch].flying)
        continue;
      auto const p = _flight.ship (static_cast<int> (ch)).p;
      auto const heard
          = _heightMap.mapTo3D (Pos::fromCartesian (p.x, p.y, 0.f),
                                flyingClip[ch]->getElevationParams ());
      auto const written = glideIntoTheFlight (ch, heard);
      _channels[ch]->setPosition (written);
      _glideFrom[ch] = written;
    }
}

void
MotionEngine::startGlide (index_t channel)
{
  // From where the ship was last heard, kept by performFlight() rather than
  // read off the channel: when the switch lands between playTick and here,
  // playTick has already put the clip's spot on the channel this tick.
  _glideTicksLeft[channel] = TempoClock::getTicksPerBeat ();
}

void
MotionEngine::glideTowardsTheClip (index_t channel)
{
  auto &left = _glideTicksLeft[channel];
  if (left <= 0)
    return;

  // playTick has just put the clip's own position on the channel; the glide
  // writes over it. The first tick is still exactly where the ship was.
  auto const clipPosition = _channels[channel]->getPosition ();
  _channels[channel]->setPosition (
      glideStep (_glideFrom[channel], clipPosition, left));
}

void
MotionEngine::startGlideIn (index_t channel)
{
  // Where the channel is heard: the clip's played position, swept and leant.
  // The ship flies its clip's plain band, so a leant clip's spot can lie
  // outside it -- mapTo2D finds the nearest floor point, and the ship's
  // first position would be heard tens of degrees away from it.
  _glideInFrom[channel] = _channels[channel]->getPosition ();
  _glideInTicksLeft[channel] = TempoClock::getTicksPerBeat ();
}

Pos
MotionEngine::glideIntoTheFlight (index_t channel, Pos const &flown)
{
  auto &left = _glideInTicksLeft[channel];
  if (left <= 0 || !_glideInFrom[channel].isValid ())
    return flown;
  return glideStep (_glideInFrom[channel], flown, left);
}

Pos
MotionEngine::glideStep (Pos const &from, Pos const &to, int &ticksLeft)
{
  // One beat, eased at both ends; the first tick is still exactly `from`.
  auto const ticksPerBeat = static_cast<float> (TempoClock::getTicksPerBeat ());
  auto const progress = 1.f - static_cast<float> (ticksLeft) / ticksPerBeat;
  --ticksLeft;
  return handover (from, to, smoothstep (progress));
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
