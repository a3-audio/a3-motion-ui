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

#include "BlobPunch.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
constexpr float floorDb = -60.f;
// A jump of this much over where the channel has been is not a hit yet --
// the ordinary flutter of a mix -- and this much more is a full one.
constexpr float firstHitDb = 4.f;
constexpr float fullHitDb = 12.f;
// The running level rises slower than a hit, so the hit stands above it,
// and falls slower still, so the gap between two kicks is not a new hit.
constexpr float riseSeconds = 0.25f;
constexpr float fallSeconds = 0.6f;
// Gone again long before the next beat: a strike, not a glow.
constexpr float punchSeconds = 0.1f;

float
decibels (float linear)
{
  return linear > 0.001f ? std::max (floorDb, 20.f * std::log10 (linear))
                         : floorDb;
}

float
follow (float current, float target, float seconds, float dt)
{
  return current + (1.f - std::exp (-dt / seconds)) * (target - current);
}
}

float
BlobPunch::update (float peak, float dtSeconds)
{
  auto const peakDb = decibels (peak);
  auto const hit = std::clamp ((peakDb - _runningDb - firstHitDb) / fullHitDb,
                               0.f, 1.f);
  _punch = std::max (hit, _punch * std::exp (-dtSeconds / punchSeconds));

  _runningDb = follow (_runningDb, peakDb,
                       peakDb > _runningDb ? riseSeconds : fallSeconds,
                       dtSeconds);
  return _punch;
}

}
