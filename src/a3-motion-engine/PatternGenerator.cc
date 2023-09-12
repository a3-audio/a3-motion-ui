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

}
