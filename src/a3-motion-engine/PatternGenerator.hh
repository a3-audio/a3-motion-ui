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

#pragma once

#include <a3-motion-engine/Pattern.hh>

namespace a3
{

class HeightMap;

class PatternGenerator
{
public:
  static std::unique_ptr<Pattern> createCircle (index_t lengthBeats,
                                                float radius, float degrees,
                                                HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createFigureOfEight (index_t lengthBeats, float radius,
                       HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createCornerStep (index_t lengthBeats, float radius,
                    HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createSpiral (index_t lengthBeats, float radius,
                HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createLissajous (index_t lengthBeats, float radius,
                   HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createRose (index_t lengthBeats, float radius,
              HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createZigzag (index_t lengthBeats, float radius,
                HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createEllipse (index_t lengthBeats, float radius,
                 HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createPendulum (index_t lengthBeats, float radius,
                  HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createTriangle (index_t lengthBeats, float radius,
                  HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createSquare (index_t lengthBeats, float radius,
                HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createStar (index_t lengthBeats, float radius,
              HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createBounce (index_t lengthBeats, float radius,
                HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createHelix (index_t lengthBeats, float radius,
               HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createOrbit (index_t lengthBeats, float radius,
               HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createCross (index_t lengthBeats, float radius,
               HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createWave (index_t lengthBeats, float radius,
              HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createHypo (index_t lengthBeats, float radius,
              HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createDiamond (index_t lengthBeats, float radius,
                 HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createClover (index_t lengthBeats, float radius,
                HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createInfinity (index_t lengthBeats, float radius,
                  HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createPetal (index_t lengthBeats, float radius,
               HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createArc (index_t lengthBeats, float radius,
             HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createHeart (index_t lengthBeats, float radius,
               HeightMap const &heightMap);

  static std::unique_ptr<Pattern>
  createRandom (index_t lengthBeats, float radius,
                HeightMap const &heightMap);

private:
};

}
