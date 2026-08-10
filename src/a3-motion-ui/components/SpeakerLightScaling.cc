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

#include "SpeakerLightScaling.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

float
speakerLightLevel (float vuPeak, float vuRms, float vuMax, float curve)
{
  auto const level = std::max (vuRms, vuPeak * 0.8f);

  return std::pow (std::clamp (level / vuMax, 0.f, 1.f), curve);
}

}
