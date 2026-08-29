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

#include "TrajectoryShape.hh"

#include <algorithm>

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

UnwrittenSpan
Pattern::getSeamSpan () const
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  return _seamSpan;
}

void
Pattern::setFadeBaseline (std::vector<Pos> positions)
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  _fadeBaseline = std::move (positions);
}

std::vector<Pos>
Pattern::getFadeBaseline () const
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  return _fadeBaseline;
}

index_t
Pattern::getFade () const
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  return _fade;
}

void
Pattern::setFade (index_t ticks)
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  _fade = ticks;
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

EndAction
Pattern::getEndAction () const
{
  return _endAction.load (std::memory_order_relaxed);
}

void
Pattern::setEndAction (EndAction action)
{
  _endAction.store (action, std::memory_order_relaxed);
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

void
Pattern::setSeamJoin (std::optional<index_t> tick)
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  _seamJoin = tick;
}

std::optional<index_t>
Pattern::getSeamJoin () const
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  return _seamJoin;
}

void
Pattern::setSeamSpan (UnwrittenSpan span)
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  _seamSpan = span;
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
  if (_jumpThreshold > 0.f)
    {
      auto const step = std::sqrt (
          std::pow (posCeil.x () - posFloor.x (), 2.f)
          + std::pow (posCeil.y () - posFloor.y (), 2.f)
          + std::pow (posCeil.z () - posFloor.z (), 2.f));
      if (step > _jumpThreshold)
        return posFloor;
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
  _reach = std::clamp (reach, 0.05f, 1.0f);
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
  params.mirrorSouth = _mirrorSouth;
  params.clipTop = _clipTop;
  params.clipBottom = _clipBottom;
  params.flat = _flat;
  params.flatElevation = _flatElevation;
  return params;
}

}
