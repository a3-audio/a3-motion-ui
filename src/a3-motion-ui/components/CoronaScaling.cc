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

#include "CoronaScaling.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{

// Perceptual curve: x^0.6 approximated as sqrt(x) * (0.4 + 0.6x), which is
// cheap enough to run per blob per frame on the RPi4.
float
perceptual (float normalized)
{
  return std::sqrt (normalized) * (0.4f + 0.6f * normalized);
}

}

CoronaConfig
loadCoronaConfig (juce::var const &config)
{
  auto const &corona = config["blob"];

  auto const value = [&corona] (char const *key, float fallback) {
    return corona.hasProperty (key) ? static_cast<float> (corona[key])
                                    : fallback;
  };

  CoronaConfig cfg;
  cfg.vuMax = value ("vuMax", cfg.vuMax);
  cfg.sizeMin = value ("sizeMin", cfg.sizeMin);
  cfg.sizeMax = value ("sizeMax", cfg.sizeMax);
  cfg.sizeGrabbed = value ("sizeGrabbed", cfg.sizeGrabbed);
  cfg.alphaMin = value ("alphaMin", cfg.alphaMin);
  cfg.alphaMax = value ("alphaMax", cfg.alphaMax);
  cfg.whiteBlend = value ("whiteBlend", cfg.whiteBlend);
  cfg.attack = value ("attack", cfg.attack);
  cfg.decay = value ("decay", cfg.decay);

  return cfg;
}

float
coronaVuLevel (float vuPeak, float vuRms, float vuMax)
{
  auto const rmsNorm = std::clamp (vuRms / vuMax, 0.f, 1.f);

  return std::max (perceptual (rmsNorm), coronaPeakLevel (vuPeak, vuMax) * 0.8f);
}

float
coronaPeakLevel (float vuPeak, float vuMax)
{
  return perceptual (std::clamp (vuPeak / vuMax, 0.f, 1.f));
}

float
coronaScaleFactor (float vuLevel, CoronaConfig const &config)
{
  return config.sizeMin + vuLevel * (config.sizeMax - config.sizeMin);
}

bool
coronaExtendsBeyondBlob (float scaleFactor)
{
  return scaleFactor * coronaOuterLayerScale > 1.f;
}

}
