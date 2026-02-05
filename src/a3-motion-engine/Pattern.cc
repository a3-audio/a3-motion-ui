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

namespace a3
{

// TODO default-initializing to channel 0 is not clean. Needs to be
// redesigned. Patterns should be channel-agnostic to begin with.
Pattern::Pattern () : _channel (0) {}

void
Pattern::clear ()
{
  std::fill (_ticks.begin (), _ticks.end (), Pos::invalid);
}

void
Pattern::resize (index_t lengthTicks)
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  _ticks.resize (lengthTicks, Pos::invalid);
}

void
Pattern::setStatus (Status status)
{
  _statusLast = _status.exchange (status);
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
  return _ticks[tick];
}

void
Pattern::setTick (index_t tick, Pos position)
{
  jassert (tick < _ticks.size ());
  std::lock_guard<std::mutex> guard (_ticksMutex);
  _ticks[tick] = position;
  _lastUpdatedTick = tick;
}

index_t
Pattern::getLastUpdatedTick () const
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  return _lastUpdatedTick;
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
  
  // Get floor and ceil indices
  auto const tickFloor = static_cast<index_t> (std::floor (normalizedTick));
  auto const tickCeil = (tickFloor + 1) % static_cast<index_t> (numTicks);
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

}
