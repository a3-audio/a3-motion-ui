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

#include <JuceHeader.h>

namespace a3
{

/** Visual tuning for the VU-driven glow around each channel blob. Sizes are
 *  multiples of the blob diameter — the solid blob disc is drawn on top of the
 *  corona, so anything at or below 1.0 stays invisible. */
struct CoronaConfig
{
  float vuMax = 0.25f;
  float sizeMin = 1.1f;
  float sizeMax = 2.2f;
  float sizeGrabbed = 1.5f;
  float alphaMin = 0.f;
  float alphaMax = 0.75f;
  float whiteBlend = 0.5f;
  float attack = 0.02f; // seconds to reach peak
  float decay = 0.4f;   // seconds to fade out
};

/** The corona is drawn as two concentric layers; this is the outer one. */
constexpr float coronaOuterLayerScale = 1.15f;

/** Reads the "corona" object out of the parsed config.json. Keys that are
 *  absent keep their default. */
CoronaConfig loadCoronaConfig (juce::var const &config);

/** Maps raw VU peak/rms onto a perceptual 0..1 level. Peak dominates but is
 *  weighted down so short transients don't wash the corona out. */
float coronaVuLevel (float vuPeak, float vuRms, float vuMax);

/** The peak leg of that curve on its own — drives alpha and white blend. */
float coronaPeakLevel (float vuPeak, float vuMax);

/** Corona diameter as a multiple of the blob diameter. */
float coronaScaleFactor (float vuLevel, CoronaConfig const &config);

/** True when the corona actually reaches past the solid blob disc drawn over
 *  it — i.e. when it is visible at all. */
bool coronaExtendsBeyondBlob (float scaleFactor);

}
