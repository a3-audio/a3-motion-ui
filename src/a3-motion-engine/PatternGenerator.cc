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

#include "PatternGenerator.hh"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <utility>
#include <vector>

#include <a3-motion-engine/elevation/HeightMap.hh>

namespace a3
{

namespace
{

// Hypotrochoids/epicycloids x(angle) = (R∓r)*cos(angle) + d*cos((R∓r)/r *
// angle) only return to their start after `angle` sweeps a whole number of
// full turns of *both* cosine terms — a single 2*pi is only enough when
// (R∓r)/r happens to be an integer. In general the curve closes after
// r / gcd(R, r) turns (R∓r and r share the same gcd as R and r), so a
// single-loop parametric sweep must scale `angle` by that many turns, or
// the path is left open and the SVG writer's closing logic draws a
// straight line across the gap.
float
closureTurns (float R, float r)
{
  auto const Ri = static_cast<long> (std::lround (R));
  auto const ri = static_cast<long> (std::lround (r));
  auto const g = std::gcd (Ri, ri);
  return g > 0 ? static_cast<float> (ri / g) : 1.f;
}

}

std::unique_ptr<Pattern>
PatternGenerator::createCircle (index_t lengthBeats, float radius,
                                float degrees, HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Circle");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto phase = float (tick) / numTicks * degrees;
      auto position = Pos::fromSpherical (phase, 0.f, radius);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createFigureOfEight (index_t lengthBeats, float radius,
                                       HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Figure 8");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto phase = float (tick) / numTicks;

      // Through the middle, twice, as a figure of eight does. It used to be
      // nudged 0.05 sideways "to avoid the azimuth singularity", which moved
      // the whole shape off the room's centre -- and a shape off the centre
      // swings round instead of turning in place, because rotation is about
      // the origin.
      //
      // There is nothing to avoid. Azimuth is undefined at r = 0, but so is
      // the direction of a sound that is directly overhead: HeightMapSphere
      // takes r to the north pole continuously, so the crossing rises over the
      // listener and comes down the other side. The azimuth flips there, and
      // at the pole a flipped azimuth points at the same place.
      auto y = radius * std::sin (phase * 2.f * pi<float> ());
      auto x = radius * std::sin (phase * 4.f * pi<float> ());

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createCornerStep (index_t lengthBeats, float radius,
                                    HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Corner");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  auto const ticksPerQuadrant = numTicks / 4;
  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      if (tick % ticksPerQuadrant == ticksPerQuadrant - 1)
        {
          pattern->setTick (tick, Pos::invalid);
        }
      else
        {
          auto const quadrant = static_cast<int> (tick * 4 / numTicks);
          auto const x = -radius * (2 * (quadrant % 2) - 1) / std::sqrt (2.f);
          auto const y = -radius * (2 * (quadrant / 2) - 1) / std::sqrt (2.f);

          auto position = Pos::fromCartesian (x, y, 0);
          position.setZ (heightMap.computeHeight (position));
          pattern->setTick (tick, position);
        }
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createSpiral (index_t lengthBeats, float radius,
                                HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Spiral");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * 4.f * 2.f * pi<float> ();
      // out-and-back: grow for first half, shrink for second half
      auto const phase = t < 0.5f ? t * 2.f : (1.f - t) * 2.f;
      auto const rad = radius * phase;
      auto const x = rad * std::cos (angle);
      auto const y = rad * std::sin (angle);

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createLissajous (index_t lengthBeats, float radius,
                                   float freqA, float freqB,
                                   float phaseOffset,
                                   HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Lissajous");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * 2.f * pi<float> ();
      auto const x = radius * std::sin (freqA * angle + phaseOffset);
      auto const y = radius * std::sin (freqB * angle);

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createRose (index_t lengthBeats, float radius, float k,
                              HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Rose");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  // r = cos(k*theta) over theta in [0, 2*pi) closes exactly for integer k
  // (odd k -> k petals traced once each; even k -> 2k petals, each traced
  // once, since cos(k*theta) goes negative and the negative radius flips
  // the point to the opposite side, filling in the "missing" petals).
  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const theta = t * 2.f * pi<float> ();
      auto const rad = radius * std::cos (k * theta);
      auto const x = rad * std::cos (theta);
      auto const y = rad * std::sin (theta);

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createZigzag (index_t lengthBeats, float radius,
                                HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Zigzag");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * 2.f * pi<float> ();
      // triangle wave for zigzag perturbation
      auto const tri = 2.f * std::abs (2.f * (t * 4.f - std::floor (t * 4.f + 0.5f))) - 1.f;
      auto const x = radius * std::cos (angle) + radius * 0.3f * tri;
      auto const y = radius * std::sin (angle);

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createEllipse (index_t lengthBeats, float radius,
                                 HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Ellipse");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * 2.f * pi<float> ();
      auto const x = radius * std::cos (angle);
      auto const y = radius * 0.5f * std::sin (angle);

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createPendulum (index_t lengthBeats, float radius,
                                  HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Pendulum");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * 2.f * pi<float> ();
      auto const x = radius * std::sin (angle);
      auto const y = radius * 0.15f * std::sin (2.f * angle);

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createTriangle (index_t lengthBeats, float radius,
                                  HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Triangle");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  // 3 vertices of equilateral triangle
  float vx[3], vy[3];
  for (int i = 0; i < 3; ++i)
    {
      auto const angle = float (i) / 3.f * 2.f * pi<float> () - pi<float> () / 2.f;
      vx[i] = radius * std::cos (angle);
      vy[i] = radius * std::sin (angle);
    }

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const segment = t * 3.f;
      auto const seg = static_cast<int> (segment) % 3;
      auto const frac = segment - std::floor (segment);
      auto const x = vx[seg] + (vx[(seg + 1) % 3] - vx[seg]) * frac;
      auto const y = vy[seg] + (vy[(seg + 1) % 3] - vy[seg]) * frac;

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createSquare (index_t lengthBeats, float radius,
                                HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Square");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  // 4 corners of a square
  auto const d = radius / std::sqrt (2.f);
  float vx[4] = { -d,  d,  d, -d };
  float vy[4] = { -d, -d,  d,  d };

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const segment = t * 4.f;
      auto const seg = static_cast<int> (segment) % 4;
      auto const frac = segment - std::floor (segment);
      auto const x = vx[seg] + (vx[(seg + 1) % 4] - vx[seg]) * frac;
      auto const y = vy[seg] + (vy[(seg + 1) % 4] - vy[seg]) * frac;

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createStar (index_t lengthBeats, float radius,
                              HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Star");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  // 5-pointed star: visit every other vertex of a pentagon
  float vx[5], vy[5];
  for (int i = 0; i < 5; ++i)
    {
      auto const angle = float (i * 2 % 5) / 5.f * 2.f * pi<float> () - pi<float> () / 2.f;
      vx[i] = radius * std::cos (angle);
      vy[i] = radius * std::sin (angle);
    }

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const segment = t * 5.f;
      auto const seg = static_cast<int> (segment) % 5;
      auto const frac = segment - std::floor (segment);
      auto const x = vx[seg] + (vx[(seg + 1) % 5] - vx[seg]) * frac;
      auto const y = vy[seg] + (vy[(seg + 1) % 5] - vy[seg]) * frac;

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createBounce (index_t lengthBeats, float radius,
                                HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Bounce");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  // 3 dots at 120° apart — jump between them
  auto const ticksPerDot = numTicks / 3;
  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const dotIndex = tick / ticksPerDot;
      auto const posInDot = tick % ticksPerDot;

      if (posInDot == ticksPerDot - 1)
        {
          // jump tick
          pattern->setTick (tick, Pos::invalid);
        }
      else
        {
          auto const angle = float (dotIndex) / 3.f * 2.f * pi<float> ();
          auto const x = radius * 0.65f * std::cos (angle);
          auto const y = radius * 0.65f * std::sin (angle);

          auto position = Pos::fromCartesian (x, y, 0);
          position.setZ (heightMap.computeHeight (position));
          pattern->setTick (tick, position);
        }
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createHelix (index_t lengthBeats, float radius,
                               HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Helix");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * 6.f * 2.f * pi<float> ();
      // shrink and grow radius: out for first half, back for second half
      auto const phase = t < 0.5f ? t * 2.f : (1.f - t) * 2.f;
      auto const rad = radius * phase;
      auto const x = rad * std::cos (angle);
      auto const y = rad * std::sin (angle);

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createOrbit (index_t lengthBeats, float radius,
                               HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Orbit");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  auto constexpr e = 0.6f; // eccentricity
  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * 2.f * pi<float> ();
      auto const rad = radius * (1.f - e * e) / (1.f + e * std::cos (angle));
      auto const x = rad * std::cos (angle);
      auto const y = rad * std::sin (angle);

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createCross (index_t lengthBeats, float radius,
                               HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Cross");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  // 4 dots on axes — jump between them
  auto const ticksPerDot = numTicks / 4;
  float dx[4] = {  1.f, 0.f, -1.f,  0.f };
  float dy[4] = {  0.f, -1.f, 0.f,  1.f };
  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const dotIndex = tick / ticksPerDot;
      auto const posInDot = tick % ticksPerDot;

      if (posInDot == ticksPerDot - 1)
        {
          pattern->setTick (tick, Pos::invalid);
        }
      else
        {
          auto const x = radius * 0.65f * dx[dotIndex];
          auto const y = radius * 0.65f * dy[dotIndex];

          auto position = Pos::fromCartesian (x, y, 0);
          position.setZ (heightMap.computeHeight (position));
          pattern->setTick (tick, position);
        }
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createWave (index_t lengthBeats, float radius,
                              HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Wave");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * 2.f * pi<float> ();
      // circle with 6 sinusoidal bumps
      auto const rad = radius * (0.6f + 0.4f * std::sin (6.f * angle));
      auto const x = rad * std::cos (angle);
      auto const y = rad * std::sin (angle);

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createHypo (index_t lengthBeats, float radius, float R,
                              float r, float d, HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Hypo");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  // Normalises against the same (R - r + d) bound the original fixed
  // R=5,r=3,d=5 used — not a tight bound for every ratio, but keeps the
  // curve comfortably inside the unit disc without per-ratio tuning.
  auto const scale = radius / (R - r + d);
  auto const turns = closureTurns (R, r);
  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * turns * 2.f * pi<float> ();
      auto const x = scale * ((R - r) * std::cos (angle)
                               + d * std::cos ((R - r) / r * angle));
      auto const y = scale * ((R - r) * std::sin (angle)
                               - d * std::sin ((R - r) / r * angle));

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createEpicycloid (index_t lengthBeats, float radius,
                                    float R, float r, float d,
                                    HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Epicycloid");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  auto const scale = radius / (R + r + d);
  auto const turns = closureTurns (R, r);
  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * turns * 2.f * pi<float> ();
      auto const x = scale * ((R + r) * std::cos (angle)
                               - d * std::cos ((R + r) / r * angle));
      auto const y = scale * ((R + r) * std::sin (angle)
                               - d * std::sin ((R + r) / r * angle));

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createDiamond (index_t lengthBeats, float radius,
                                 HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Diamond");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  // Diamond (rhombus): 4 corners at top/right/bottom/left
  auto const quarterTicks = numTicks / 4;
  struct Pt { float x, y; };
  Pt corners[4] = {
    { 0.f, radius },
    { radius, 0.f },
    { 0.f, -radius },
    { -radius, 0.f }
  };

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const seg = tick / quarterTicks;
      auto const frac = float (tick % quarterTicks) / float (quarterTicks);
      auto const i0 = seg % 4;
      auto const i1 = (seg + 1) % 4;
      auto const x = corners[i0].x + frac * (corners[i1].x - corners[i0].x);
      auto const y = corners[i0].y + frac * (corners[i1].y - corners[i0].y);
      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createClover (index_t lengthBeats, float radius,
                                HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Clover");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  // 4-leaf clover: r = radius * cos(2*theta)
  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * 2.f * pi<float> ();
      auto const r = radius * std::abs (std::cos (2.f * angle));
      auto const x = r * std::cos (angle);
      auto const y = r * std::sin (angle);
      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createInfinity (index_t lengthBeats, float radius,
                                  HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Infinity");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  // Lemniscate of Bernoulli: x = a*cos(t)/(1+sin^2(t)), y = a*sin(t)*cos(t)/(1+sin^2(t))
  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * 2.f * pi<float> ();
      auto const s = std::sin (angle);
      auto const c = std::cos (angle);
      auto const denom = 1.f + s * s;
      auto const x = radius * c / denom;
      auto const y = radius * s * c / denom;
      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createPetal (index_t lengthBeats, float radius,
                               HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Petal");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  // 3-petal rose: r = radius * sin(3*theta)
  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * pi<float> (); // 0 to pi for a full 3-petal rose
      auto const r = radius * std::abs (std::sin (3.f * angle));
      auto const x = r * std::cos (angle);
      auto const y = r * std::sin (angle);
      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createArc (index_t lengthBeats, float radius,
                             HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Arc");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  // Semi-circle: 180° arc, ping-pong (forward then backward)
  auto const halfTicks = numTicks / 2;
  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      float frac;
      if (tick < halfTicks)
        frac = float (tick) / float (halfTicks);
      else
        frac = float (numTicks - tick) / float (numTicks - halfTicks);
      auto const angle = frac * pi<float> ();
      auto const x = radius * std::cos (angle);
      auto const y = radius * std::sin (angle);
      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createHeart (index_t lengthBeats, float radius,
                               HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Heart");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  // Heart curve parametric: x = sin^3(t), y = cos(t) - cos^2(t) ... etc
  // Simplified cardioid-based heart
  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * 2.f * pi<float> ();
      auto const s = std::sin (angle);
      auto const c = std::cos (angle);
      auto const x = radius * 0.6f * s * s * s;
      auto const y = radius * 0.55f * (
          13.f * c - 5.f * std::cos (2.f * angle)
          - 2.f * std::cos (3.f * angle) - std::cos (4.f * angle)) / 16.f;
      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createRandom (index_t lengthBeats, float radius,
                                HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Random");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  // Random walk: jump to random positions at beat boundaries,
  // linearly interpolate between them
  auto const ticksPerBeat
      = static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  auto const numBeats = lengthBeats + 1; // +1 for wrap-around target

  // Generate random control points using simple LCG
  std::vector<float> cx (numBeats), cy (numBeats);
  unsigned int seed = 42;
  auto nextRand = [&seed] () -> float {
    seed = seed * 1103515245u + 12345u;
    return (static_cast<float> ((seed >> 16) & 0x7FFF) / 32767.f) * 2.f - 1.f;
  };
  for (std::size_t i = 0; i < numBeats; ++i)
    {
      cx[i] = nextRand () * radius;
      cy[i] = nextRand () * radius;
    }
  // Wrap: last point = first point
  cx[numBeats - 1] = cx[0];
  cy[numBeats - 1] = cy[0];

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const beat = tick / ticksPerBeat;
      auto const frac = float (tick % ticksPerBeat) / float (ticksPerBeat);
      auto const x = cx[beat] + frac * (cx[beat + 1] - cx[beat]);
      auto const y = cy[beat] + frac * (cy[beat + 1] - cy[beat]);
      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

// -- The fifty-shape library (2026-09-28) ------------------------------------
//
// Two ways to draw: a curve over one lap, or steps that land on sixteenths.
// Everything below is one of those with the idea written as numbers.

namespace
{
using Curve = std::function<std::pair<float, float> (float lap)>;

std::unique_ptr<Pattern>
emptyPattern (char const *name, index_t lengthBeats)
{
  auto pattern = std::make_unique<Pattern> ();
  pattern->setName (name);
  pattern->resize (lengthBeats
                   * static_cast<index_t> (TempoClock::getTicksPerBeat ()));
  pattern->setStatus (Pattern::Status::Idle);
  return pattern;
}

void
place (Pattern &pattern, index_t tick, float x, float y,
       HeightMap const &heightMap)
{
  auto position = Pos::fromCartesian (x, y, 0);
  position.setZ (heightMap.computeHeight (position));
  pattern.setTick (tick, position);
}

/** A curve on the unit disc, scaled to `radius`. `jumpsHome` leaves the
 *  last tick empty, for a gesture that goes one way and starts again rather
 *  than coming back: the seam then reads as the jump it is, not as a line
 *  drawn across the room. */
std::unique_ptr<Pattern>
fromCurve (char const *name, index_t lengthBeats, float radius,
           Curve const &curve, HeightMap const &heightMap,
           bool jumpsHome = false)
{
  auto pattern = emptyPattern (name, lengthBeats);
  auto const numTicks = pattern->getNumTicks ();
  for (index_t tick = 0; tick < numTicks; ++tick)
    {
      if (jumpsHome && tick + 1 == numTicks)
        break;
      auto const [x, y] = curve (float (tick) / float (numTicks));
      place (*pattern, tick, radius * x, radius * y, heightMap);
    }
  return pattern;
}

/** Where a stepped figure lands, and on which sixteenth. */
struct Step
{
  int sixteenth;
  float x;
  float y;
};

/** Held positions that change on the given sixteenths -- a rhythm, spoken
 *  as places. The tick before each landing is left empty, as Corner does,
 *  so a jump is drawn as a jump. */
std::unique_ptr<Pattern>
fromSteps (char const *name, index_t lengthBeats, float radius,
           std::vector<Step> const &steps, HeightMap const &heightMap)
{
  auto pattern = emptyPattern (name, lengthBeats);
  auto const numTicks = pattern->getNumTicks ();
  auto const perSixteenth
      = static_cast<index_t> (TempoClock::getTicksPerBeat () / 4);

  for (index_t tick = 0; tick < numTicks; ++tick)
    {
      auto const sixteenth = static_cast<int> (tick / perSixteenth);
      auto current = steps.back ();
      for (auto const &step : steps)
        if (step.sixteenth <= sixteenth)
          current = step;

      auto const nextTick = tick + 1;
      auto const landsNext
          = nextTick % perSixteenth == 0
            && std::any_of (steps.begin (), steps.end (),
                            [&] (Step const &s) {
                              return static_cast<index_t> (s.sixteenth)
                                         * perSixteenth
                                     == nextTick % numTicks;
                            });
      if (landsNext)
        continue; // the gap before a landing

      place (*pattern, tick, radius * current.x, radius * current.y,
             heightMap);
    }
  return pattern;
}

/** Points evenly round the disc, starting at the front. */
std::vector<Step>
roundTheRoom (std::vector<int> const &sixteenths)
{
  std::vector<Step> steps;
  auto const n = static_cast<float> (sixteenths.size ());
  for (size_t i = 0; i < sixteenths.size (); ++i)
    {
      auto const angle = pi<float> () / 2.f + 2.f * pi<float> () * i / n;
      steps.push_back ({ sixteenths[i], std::cos (angle), std::sin (angle) });
    }
  return steps;
}

constexpr float turn = 2.f * pi<float> ();
}

// Tresillo, 3+3+2 in eighths: the cell under clave, and the one EDM builds
// its tension on before a drop. Three places, jumped to on 1, the "and" of
// 2 and 4.
std::unique_ptr<Pattern>
PatternGenerator::createTresillo (index_t lengthBeats, float radius,
                                  HeightMap const &heightMap)
{
  return fromSteps ("Tresillo", lengthBeats, radius,
                    roundTheRoom ({ 0, 6, 12 }), heightMap);
}

// Son clave 3-2: three strokes in the first bar, two in the second.
std::unique_ptr<Pattern>
PatternGenerator::createClave32 (index_t lengthBeats, float radius,
                                 HeightMap const &heightMap)
{
  return fromSteps ("Clave 3-2", lengthBeats, radius,
                    roundTheRoom ({ 0, 6, 12, 20, 24 }), heightMap);
}

// Call and response: left on one beat, right on the next.
std::unique_ptr<Pattern>
PatternGenerator::createPingPong (index_t lengthBeats, float radius,
                                  HeightMap const &heightMap)
{
  std::vector<Step> steps;
  for (int beat = 0; beat < static_cast<int> (lengthBeats); ++beat)
    steps.push_back ({ beat * 4, beat % 2 == 0 ? -1.f : 1.f, 0.f });
  return fromSteps ("Ping Pong", lengthBeats, radius, steps, heightMap);
}

// Smalley's centrifugence, and the riser of a build-up: out from the middle
// in four turns, one way, then home in a jump.
std::unique_ptr<Pattern>
PatternGenerator::createRiser (index_t lengthBeats, float radius,
                               HeightMap const &heightMap)
{
  return fromCurve (
      "Riser", lengthBeats, radius,
      [] (float t) {
        return std::pair{ t * std::cos (4.f * turn * t),
                          t * std::sin (4.f * turn * t) };
      },
      heightMap, true);
}

// Centripetence, contraction: in from the edge, three turns.
std::unique_ptr<Pattern>
PatternGenerator::createCollapse (index_t lengthBeats, float radius,
                                  HeightMap const &heightMap)
{
  return fromCurve (
      "Collapse", lengthBeats, radius,
      [] (float t) {
        return std::pair{ (1.f - t) * std::cos (3.f * turn * t),
                          (1.f - t) * std::sin (3.f * turn * t) };
      },
      heightMap, true);
}

// Smalley's vortex: inwards, and faster the tighter it gets -- the turn goes
// with the square of the lap, the way a skater pulls in their arms.
std::unique_ptr<Pattern>
PatternGenerator::createVortex (index_t lengthBeats, float radius,
                                HeightMap const &heightMap)
{
  return fromCurve (
      "Vortex", lengthBeats, radius,
      [] (float t) {
        auto const r = 1.f - 0.9f * t;
        auto const a = 6.f * turn * t * t;
        return std::pair{ r * std::cos (a), r * std::sin (a) };
      },
      heightMap, true);
}

// A dub echo: thrown out and back four times, each throw half the last, the
// way a tape delay's repeats fall away.
std::unique_ptr<Pattern>
PatternGenerator::createEcho (index_t lengthBeats, float radius,
                              HeightMap const &heightMap)
{
  auto const lap = static_cast<int> (lengthBeats) * 4;
  auto const d = 1.f / std::sqrt (2.f);
  std::vector<Step> steps;
  auto reach = 1.f;
  for (int i = 0; i < 4; ++i)
    {
      steps.push_back ({ i * lap / 4, reach * d, reach * d });
      steps.push_back ({ i * lap / 4 + lap / 8, 0.f, 0.f });
      reach /= 2.f;
    }
  return fromSteps ("Echo", lengthBeats, radius, steps, heightMap);
}

// Dilation and contraction on a turn: the circle breathes four times round.
std::unique_ptr<Pattern>
PatternGenerator::createPulse (index_t lengthBeats, float radius,
                               HeightMap const &heightMap)
{
  return fromCurve (
      "Pulse", lengthBeats, radius,
      [] (float t) {
        auto const r = 0.7f + 0.3f * std::cos (4.f * turn * t);
        return std::pair{ r * std::cos (turn * t), r * std::sin (turn * t) };
      },
      heightMap);
}

// Four cusps, sharp at the tips and slow between them: a star that turns
// its corners hard -- tense rather than round.
std::unique_ptr<Pattern>
PatternGenerator::createAstroid (index_t lengthBeats, float radius,
                                 HeightMap const &heightMap)
{
  return fromCurve (
      "Astroid", lengthBeats, radius,
      [] (float t) {
        auto const c = std::cos (turn * t), s = std::sin (turn * t);
        return std::pair{ c * c * c, s * s * s };
      },
      heightMap);
}

// A trefoil: three leaves drawn in one line, flowing, never stopping.
std::unique_ptr<Pattern>
PatternGenerator::createTrefoil (index_t lengthBeats, float radius,
                                 HeightMap const &heightMap)
{
  return fromCurve (
      "Trefoil", lengthBeats, radius,
      [] (float t) {
        auto const a = turn * t;
        return std::pair{ (std::sin (a) + 2.f * std::sin (2.f * a)) / 3.f,
                          (std::cos (a) - 2.f * std::cos (2.f * a)) / 3.f };
      },
      heightMap);
}

// A slow wander that still comes home. Polar, so it cannot leave the disc:
// the distance breathes on three whole multiples of the lap and the bearing
// sways about one steady turn, so it reads as organic and closes without a
// seam.
std::unique_ptr<Pattern>
PatternGenerator::createDrift (index_t lengthBeats, float radius,
                               HeightMap const &heightMap)
{
  return fromCurve (
      "Drift", lengthBeats, radius,
      [] (float t) {
        auto const a = turn * t;
        auto const r = 0.6f + 0.25f * std::sin (3.f * a + 1.f)
                       + 0.15f * std::sin (5.f * a + 2.f);
        auto const bearing = a + 0.35f * std::sin (2.f * a + 0.7f);
        return std::pair{ r * std::cos (bearing), r * std::sin (bearing) };
      },
      heightMap);
}

}
