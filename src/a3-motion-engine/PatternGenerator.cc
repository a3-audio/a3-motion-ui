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

#include <a3-motion-engine/elevation/HeightMap.hh>

namespace a3
{

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

      auto constexpr offsetX = 0.05; // avoid azimuth singularity
      auto y = radius * std::sin (phase * 2.f * pi<float> ());
      auto x = radius * std::sin (phase * 4.f * pi<float> ()) + offsetX;

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
      auto const x = radius * std::sin (3.f * angle + pi<float> () / 2.f);
      auto const y = radius * std::sin (2.f * angle);

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

std::unique_ptr<Pattern>
PatternGenerator::createRose (index_t lengthBeats, float radius,
                              HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Rose");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const theta = t * 2.f * pi<float> ();
      auto const rad = radius * std::cos (3.f * theta);
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
PatternGenerator::createHypo (index_t lengthBeats, float radius,
                              HeightMap const &heightMap)
{
  std::unique_ptr<Pattern> pattern = std::make_unique<Pattern> ();
  pattern->setName ("Hypo");

  auto const numTicks
      = lengthBeats
        * static_cast<std::size_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (numTicks);
  pattern->setStatus (Pattern::Status::Idle);

  auto constexpr R = 5.f, rv = 3.f, d = 5.f;
  auto const scale = radius / (R - rv + d);
  for (auto tick = 0u; tick < numTicks; ++tick)
    {
      auto const t = float (tick) / numTicks;
      auto const angle = t * 2.f * pi<float> ();
      auto const x = scale * ((R - rv) * std::cos (angle)
                               + d * std::cos ((R - rv) / rv * angle));
      auto const y = scale * ((R - rv) * std::sin (angle)
                               - d * std::sin ((R - rv) / rv * angle));

      auto position = Pos::fromCartesian (x, y, 0);
      position.setZ (heightMap.computeHeight (position));
      pattern->setTick (tick, position);
    }

  return pattern;
}

}
