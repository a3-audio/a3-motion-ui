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

#include "Pattern.hh"

#include <a3-motion-engine/Envelope.hh>

#include "TrajectoryShape.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

// TODO default-initializing to channel 0 is not clean. Needs to be
// redesigned. Patterns should be channel-agnostic to begin with.
Pattern::Pattern () : _channel (0) {}

void
Pattern::clear ()
{
  std::fill (_ticks.begin (), _ticks.end (), Pos::invalid);
  std::fill (_written.begin (), _written.end (), false);
}

void
Pattern::resize (index_t lengthTicks)
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  _ticks.resize (lengthTicks, Pos::invalid);
  _bridgePlanStale = true;
  // Started over rather than grown: the mask describes the recording that is
  // running, and a resize means a different one.
  _written.assign (lengthTicks, false);
}

void
Pattern::setStatus (Status status)
{
  _statusLast = _status.exchange (status);
  if (status == Status::Recording)
    _wasRecording = true;
}

Pattern::Status
Pattern::getStatus () const
{
  return _status;
}

Pattern::Status
Pattern::getLastStatus () const
{
  return _statusLast;
}

bool
Pattern::wasRecording () const
{
  return _wasRecording;
}

void
Pattern::restoreStatus ()
{
  _status.exchange (_statusLast);
}

void
Pattern::setChannel (index_t channel)
{
  _channel = channel;
}

index_t
Pattern::getChannel () const
{
  return _channel;
}

void
Pattern::setName (std::string name)
{
  _name = std::move (name);
}

std::string const &
Pattern::getName () const
{
  return _name;
}

index_t
Pattern::getNumTicks () const
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  return _ticks.size ();
}

Pos
Pattern::getTick (index_t tick) const
{
  jassert (tick < _ticks.size ());
  std::lock_guard<std::mutex> guard (_ticksMutex);
  // Guarded in release too, not only asserted: the assert is compiled out of
  // exactly the build that ships, and reading past the end is undefined.
  if (tick >= _ticks.size ())
    return Pos::invalid;

  return _ticks[tick];
}

void
Pattern::setTick (index_t tick, Pos position)
{
  jassert (tick < _ticks.size ());
  std::lock_guard<std::mutex> guard (_ticksMutex);
  // Same reason, and here it is worse: writing past the end corrupts the heap.
  if (tick >= _ticks.size ())
    return;

  _ticks[tick] = position;
  _bridgePlanStale = true;
  if (tick < _written.size ())
    _written[tick] = true;
  _lastUpdatedTick = tick;
}

index_t
Pattern::getLastUpdatedTick () const
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  return _lastUpdatedTick;
}

std::vector<bool>
Pattern::writtenTicks () const
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  return _written;
}

bool
Pattern::isTickWritten (index_t tick) const
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  return tick < _written.size () && _written[tick];
}






PlayDirection
Pattern::getPlayDirection () const
{
  return _playDirection.load (std::memory_order_relaxed);
}

void
Pattern::setPlayDirection (PlayDirection direction)
{
  _playDirection.store (direction, std::memory_order_relaxed);

  // And which way it is travelling right now, so choosing a direction turns a
  // clip that is already playing rather than waiting for the next start. Only
  // Bounce moves the two apart, and it does that as it goes.
  _playSign.store (initialSign (direction), std::memory_order_relaxed);
}

int
Pattern::getFreqAttack () const
{
  return _freqAttack.load (std::memory_order_relaxed);
}

void
Pattern::setFreqAttack (int step)
{
  _freqAttack.store (juce::jlimit (0, envelopeMaxStep, step),
                     std::memory_order_relaxed);
}

int
Pattern::getFreqDecay () const
{
  return _freqDecay.load (std::memory_order_relaxed);
}

void
Pattern::setFreqDecay (int step)
{
  _freqDecay.store (juce::jlimit (0, envelopeMaxStep, step),
                    std::memory_order_relaxed);
}

float
Pattern::getFreqMax () const
{
  return _freqMax.load (std::memory_order_relaxed);
}

void
Pattern::setFreqMax (float max)
{
  _freqMax.store (juce::jlimit (0.f, 1.f, max), std::memory_order_relaxed);
}

int
Pattern::getQAttack () const
{
  return _qAttack.load (std::memory_order_relaxed);
}

void
Pattern::setQAttack (int step)
{
  _qAttack.store (juce::jlimit (0, envelopeMaxStep, step),
                  std::memory_order_relaxed);
}

int
Pattern::getQDecay () const
{
  return _qDecay.load (std::memory_order_relaxed);
}

void
Pattern::setQDecay (int step)
{
  _qDecay.store (juce::jlimit (0, envelopeMaxStep, step),
                 std::memory_order_relaxed);
}

float
Pattern::getQMax () const
{
  return _qMax.load (std::memory_order_relaxed);
}

void
Pattern::setQMax (float max)
{
  _qMax.store (juce::jlimit (0.f, 1.f, max), std::memory_order_relaxed);
}

float
Pattern::getFadeReach () const
{
  return _fadeReach.load (std::memory_order_relaxed);
}

void
Pattern::setFadeReach (float reach)
{
  _fadeReach.store (juce::jlimit (0.f, 1.f, reach), std::memory_order_relaxed);
  markBridgePlanStale ();
}

int
Pattern::getBridgeBias () const
{
  return _bridgeBias.load (std::memory_order_relaxed);
}

void
Pattern::setBridgeBias (int bias)
{
  _bridgeBias.store (juce::jlimit (-4, 4, bias), std::memory_order_relaxed);
  markBridgePlanStale ();
}

void
Pattern::markBridgePlanStale ()
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  _bridgePlanStale = true;
}

void
Pattern::ensureBridgePlanLocked () const
{
  // The caller holds _ticksMutex. Taking it again here -- which calling
  // getBridgePlan() from getInterpolatedTick() would do -- deadlocks the
  // audio thread against itself.
  if (!_bridgePlanStale)
    return;

  // Only a looping clip travels the step from its last tick to its first, so
  // only a looping clip has a gap there to join.
  _bridgePlan = planBridges (_ticks, _fadeReach.load (), _bridgeBias.load (),
                             seedForTicks (_ticks),
                             _endAction.load () == EndAction::Loop);
  _bridgePlanStale = false;
}

BridgePlan
Pattern::getBridgePlan () const
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  ensureBridgePlanLocked ();
  return _bridgePlan;
}

EndAction
Pattern::getEndAction () const
{
  return _endAction.load (std::memory_order_relaxed);
}

void
Pattern::setEndAction (EndAction action)
{
  _endAction.store (action, std::memory_order_relaxed);

  // Whether the wrap is a gap depends on this -- only a looping clip travels
  // it. See ensureBridgePlanLocked().
  markBridgePlanStale ();
}

ActMode
Pattern::getActMode () const
{
  return _actMode.load ();
}

void
Pattern::setActMode (ActMode mode)
{
  _actMode.store (mode);
}

int
Pattern::getSpeedLog2 () const
{
  return _speedLog2.load ();
}

void
Pattern::setSpeedLog2 (int speedLog2)
{
  _speedLog2.store (speedLog2);
}



float
Pattern::getPlaySign () const
{
  return _playSign.load (std::memory_order_relaxed);
}

void
Pattern::setPlaySign (float sign)
{
  _playSign.store (sign, std::memory_order_relaxed);
}

float
Pattern::getRotate () const
{
  return _rotate.load (std::memory_order_relaxed);
}

void
Pattern::setRotate (float revolutions)
{
  // Wrapped rather than clamped: a turn has no ends, and a control that
  // stopped at one would be a control you could get stuck against.
  auto wrapped = std::fmod (revolutions, 1.f);
  if (wrapped < 0.f)
    wrapped += 1.f;

  _rotate.store (wrapped, std::memory_order_relaxed);
}

float
Pattern::getSqueezeX () const
{
  return _squeezeX.load (std::memory_order_relaxed);
}

void
Pattern::setSqueezeX (float amount)
{
  _squeezeX.store (std::clamp (amount, -1.f, 1.f), std::memory_order_relaxed);
}

float
Pattern::getSqueezeY () const
{
  return _squeezeY.load (std::memory_order_relaxed);
}

void
Pattern::setSqueezeY (float amount)
{
  // Clamped where the value is stored, not where it is used: a script, a file
  // and a finger all reach this setter, and the ends of the travel should be
  // the ends of the travel for all three.
  _squeezeY.store (std::clamp (amount, -1.f, 1.f), std::memory_order_relaxed);
}

int
Pattern::getSqueezeXLfo () const
{
  return _squeezeXLfo.load (std::memory_order_relaxed);
}

void
Pattern::setSqueezeXLfo (int step)
{
  _squeezeXLfo.store (step, std::memory_order_relaxed);
}

int
Pattern::getSqueezeYLfo () const
{
  return _squeezeYLfo.load (std::memory_order_relaxed);
}

void
Pattern::setSqueezeYLfo (int step)
{
  _squeezeYLfo.store (step, std::memory_order_relaxed);
}

float
Pattern::getSqueezeXLfoPhase () const
{
  return _squeezeXLfoPhase.load (std::memory_order_relaxed);
}

void
Pattern::setSqueezeXLfoPhase (float phase)
{
  _squeezeXLfoPhase.store (phase, std::memory_order_relaxed);
}

float
Pattern::getSqueezeYLfoPhase () const
{
  return _squeezeYLfoPhase.load (std::memory_order_relaxed);
}

void
Pattern::setSqueezeYLfoPhase (float phase)
{
  _squeezeYLfoPhase.store (phase, std::memory_order_relaxed);
}

int
Pattern::getSpin () const
{
  return _spin.load (std::memory_order_relaxed);
}

void
Pattern::setSpin (int step)
{
  _spin.store (step, std::memory_order_relaxed);
}

float
Pattern::getSpinPhase () const
{
  return _spinPhase.load (std::memory_order_relaxed);
}

void
Pattern::setSpinPhase (float phase)
{
  _spinPhase.store (phase, std::memory_order_relaxed);
}

int
Pattern::getReachLfo () const
{
  return _reachLfo.load (std::memory_order_relaxed);
}

void
Pattern::setReachLfo (int step)
{
  _reachLfo.store (step, std::memory_order_relaxed);
}

float
Pattern::getReachLfoPhase () const
{
  return _reachLfoPhase.load (std::memory_order_relaxed);
}

void
Pattern::setReachLfoPhase (float phase)
{
  _reachLfoPhase.store (phase, std::memory_order_relaxed);
}

int
Pattern::getElevationLfo () const
{
  return _elevationLfo.load (std::memory_order_relaxed);
}

void
Pattern::setElevationLfo (int step)
{
  _elevationLfo.store (step, std::memory_order_relaxed);
}

float
Pattern::getElevationLfoPhase () const
{
  return _elevationLfoPhase.load (std::memory_order_relaxed);
}

void
Pattern::setElevationLfoPhase (float phase)
{
  _elevationLfoPhase.store (phase, std::memory_order_relaxed);
}

int
Pattern::getEnvelopeAttack () const
{
  return _envelopeAttack.load (std::memory_order_relaxed);
}

void
Pattern::setEnvelopeAttack (int step)
{
  _envelopeAttack.store (step, std::memory_order_relaxed);
}

int
Pattern::getEnvelopeDecay () const
{
  return _envelopeDecay.load (std::memory_order_relaxed);
}

void
Pattern::setEnvelopeDecay (int step)
{
  _envelopeDecay.store (step, std::memory_order_relaxed);
}

float
Pattern::getEnvelopeMax () const
{
  return _envelopeMax.load (std::memory_order_relaxed);
}

void
Pattern::setEnvelopeMax (float value)
{
  _envelopeMax.store (std::clamp (value, 0.f, 1.f), std::memory_order_relaxed);
}




void
Pattern::markComplete ()
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  if (!_ticks.empty ())
    _lastUpdatedTick = _ticks.size () - 1;

  // The same measure the drawing uses to decide where one stroke ends and the
  // next begins, so what is played matches what is shown. The median over all
  // steps, held ticks included: on a tapped take almost every step is zero and
  // the few that are not are the taps themselves, while on a drawn one the
  // median is the motion's own pace and no step comes near eight times it.
  _jumpThreshold = trajectoryJumpThreshold (typicalTrajectoryStep (_ticks));
}

void
Pattern::clearWrittenTicks ()
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  std::fill (_written.begin (), _written.end (), false);
}

Pos
Pattern::getInterpolatedTick (double fractionalTick) const
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  
  if (_ticks.empty ())
    return Pos::invalid;

  auto const numTicks = static_cast<double> (_ticks.size ());
  
  // Use lastValidTick+1 for the effective length (since indices are 0-based)
  // If nothing recorded yet, fall back to full pattern size
  double effectiveLength = (_lastUpdatedTick > 0) 
                             ? static_cast<double> (_lastUpdatedTick + 1) 
                             : numTicks;
  
  // Safety: effectiveLength must be positive
  if (effectiveLength <= 0)
    effectiveLength = numTicks;
  if (effectiveLength <= 0)
    return Pos::invalid;  // No valid ticks at all
  
  // Normalize fractionalTick to [0, effectiveLength) using fmod
  double normalizedTick = std::fmod (fractionalTick, effectiveLength);
  if (normalizedTick < 0)
    normalizedTick += effectiveLength;
  
  // Get floor and ceil indices — wrap within effectiveLength so that
  // partially-recorded patterns don't interpolate with ticks beyond
  // the recorded range.
  auto const effLen = static_cast<index_t> (effectiveLength);
  auto const tickFloor = static_cast<index_t> (std::floor (normalizedTick));
  auto const tickCeil = (tickFloor + 1) % effLen;
  auto const fraction = static_cast<float> (normalizedTick - std::floor (normalizedTick));
  
  // Get keyframes
  auto const posFloor = _ticks[tickFloor];
  auto const posCeil = _ticks[tickCeil];
  
  // If either keyframe is invalid, return the valid one or invalid
  if (!posFloor.isValid () && !posCeil.isValid ())
    return Pos::invalid;
  if (!posFloor.isValid ())
    return posCeil;
  if (!posCeil.isValid ())
    return posFloor;
  
  // A jump is played as a jump: stand on this tick until the next one takes
  // over. Interpolating across it drew the blob at every point along a way
  // nobody played -- so a tapped take slid between its taps instead of
  // standing at them, and a tap held only briefly was crossed without ever
  // being reached.
  //
  // Which jumps are stood on and which are drawn through is the fade's
  // business, and it is decided from the same plan the drawn line is cut by --
  // two independent answers drift apart, and then the sphere shows a line the
  // blob does not run on.
  ensureBridgePlanLocked ();
  auto const *crossing = _bridgePlan.crossingAt (tickFloor, effLen);

  if (crossing == nullptr && _jumpThreshold > 0.f)
    {
      auto const step = std::sqrt (
          std::pow (posCeil.x () - posFloor.x (), 2.f)
          + std::pow (posCeil.y () - posFloor.y (), 2.f)
          + std::pow (posCeil.z () - posFloor.z (), 2.f));
      if (step > _jumpThreshold)
        return posFloor;
    }

  // A crossing is walked, not jumped.
  //
  // It used to be taken in the single tick the gap sits in, however far it
  // reached -- which is the blob shooting across the room, hundreds of times
  // the speed of every other tick of the take. Now it is given a window, out
  // of the two ends it joins, and it is walked across that window at one
  // speed: out to the point it lands on, then on to where it rejoins the
  // timeline. The take keeps its length, its bar and its order; what it gives
  // up is the tail of the run before and the head of the run after, which on
  // a tapped take is time spent standing still.
  if (crossing != nullptr)
    {
      auto const rejoin
          = static_cast<index_t> ((crossing->leaveTick + crossing->windowTicks)
                                  % effLen);

      auto const &posLeave = _ticks[crossing->leaveTick];
      auto const &posVia = _ticks[crossing->viaTick];
      auto const &posRejoin = _ticks[rejoin];

      auto const span = [] (Pos const &a, Pos const &b) {
        return std::sqrt (std::pow (b.x () - a.x (), 2.f)
                          + std::pow (b.y () - a.y (), 2.f)
                          + std::pow (b.z () - a.z (), 2.f));
      };

      auto const outward = span (posLeave, posVia);
      auto const onward = span (posVia, posRejoin);

      auto since = normalizedTick - static_cast<double> (crossing->leaveTick);
      if (since < 0.0)
        since += effectiveLength;

      auto const travelled
          = static_cast<float> (since / crossing->windowTicks)
            * (outward + onward);

      auto const walk = [] (Pos const &from, Pos const &to, float part) {
        return Pos::fromCartesian (
            from.x () + (to.x () - from.x ()) * part,
            from.y () + (to.y () - from.y ()) * part,
            from.z () + (to.z () - from.z ()) * part);
      };

      if (travelled <= outward)
        return outward > 0.f ? walk (posLeave, posVia, travelled / outward)
                             : posVia;

      return onward > 0.f
                 ? walk (posVia, posRejoin, (travelled - outward) / onward)
                 : posRejoin;
    }

  // Interpolate in Cartesian space for smooth, robust interpolation
  // This avoids azimuth discontinuities (e.g., 350° to 10°)
  auto const x0 = posFloor.x ();
  auto const y0 = posFloor.y ();
  auto const z0 = posFloor.z ();
  
  auto const x1 = posCeil.x ();
  auto const y1 = posCeil.y ();
  auto const z1 = posCeil.z ();
  
  auto const xInterp = x0 + (x1 - x0) * fraction;
  auto const yInterp = y0 + (y1 - y0) * fraction;
  auto const zInterp = z0 + (z1 - z0) * fraction;
  
  return Pos::fromCartesian (xInterp, yInterp, zInterp);
}

Pattern::Ticks
Pattern::getTicks () const
{
  // for now we just lock and return a copy while benchmarking and
  // thinking of a better solution.
  std::lock_guard<std::mutex> guard (_ticksMutex);
  return { _ticks, _lastUpdatedTick };
}

Measure
Pattern::getPlaybackLength () const
{
  return _playbackLength;
}

void
Pattern::setPlaybackLength (Measure playbackLength)
{
  _playbackLength = playbackLength;
}

float
Pattern::getPlayPosition () const
{
  return _playPosition;
}

void
Pattern::setPlayPosition (float playPosition)
{
  _playPosition = playPosition;
}

float
Pattern::getReach () const
{
  return _reach;
}

void
Pattern::setReach (float reach)
{
  // Signed: the size is how far the figure spreads from the base and the sign
  // is which way, down or up. It ran 0.05..1 while the cone chose a pole for
  // itself; now that it always grows the way it is told, the only way to put a
  // figure above its base is to say so. A value from a file that only knows
  // the old range means what it always did.
  _reach = std::clamp (reach, -1.0f, 1.0f);
}

float
Pattern::getElevationBase () const
{
  return _elevationBase.load (std::memory_order_relaxed);
}

void
Pattern::setElevationBase (float base)
{
  _elevationBase.store (juce::jlimit (0.f, 1.f, base),
                        std::memory_order_relaxed);
}

bool
Pattern::getMirrorSouth () const
{
  return _mirrorSouth;
}

void
Pattern::setMirrorSouth (bool mirrorSouth)
{
  _mirrorSouth = mirrorSouth;
}

float
Pattern::getClipTop () const
{
  return _clipTop;
}

void
Pattern::setClipTop (float clipTop)
{
  _clipTop = std::clamp (clipTop, 0.0f, 1.0f);
}

float
Pattern::getClipBottom () const
{
  return _clipBottom;
}

void
Pattern::setClipBottom (float clipBottom)
{
  _clipBottom = std::clamp (clipBottom, 0.0f, 1.0f);
}

bool
Pattern::getFlat () const
{
  return _flat;
}

void
Pattern::setFlat (bool flat)
{
  _flat = flat;
}

float
Pattern::getFlatElevation () const
{
  return _flatElevation;
}

void
Pattern::setFlatElevation (float flatElevation)
{
  _flatElevation = std::clamp (flatElevation, 0.0f, 1.0f);
}

ElevationParams
Pattern::getElevationParams () const
{
  ElevationParams params;
  params.reach = _reach;
  params.elevationBase = _elevationBase.load (std::memory_order_relaxed);
  params.mirrorSouth = _mirrorSouth;
  params.clipTop = _clipTop;
  params.clipBottom = _clipBottom;
  params.flat = _flat;
  params.flatElevation = _flatElevation;
  return params;
}

}
