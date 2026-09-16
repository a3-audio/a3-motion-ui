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
  // How far the corona reaches at silence and at full level, in blob radii.
  //
  // These described a ring drawn around a 2D disc until 2026-09-16; they are
  // the shader's corona reach now, in the same unit. The ramp they replaced
  // was written into the shader as 1.9 + 2.4 * level, so the floor keeps that
  // and the ceiling goes above it — the reach is the one thing about a blob
  // that says "level" from across a booth, and it was reported as too subtle
  // to read.
  float sizeMin = 1.9f;
  float sizeMax = 5.0f;
  float sizeGrabbed = 1.5f;
  float alphaMin = 0.f;
  float alphaMax = 0.75f;
  float whiteBlend = 0.5f;
  float attack = 0.02f; // seconds to reach peak
  float decay = 0.4f;   // seconds to fade out
};

/** The corona is drawn as two concentric layers; this is the outer one. */
constexpr float coronaOuterLayerScale = 1.15f;

/** Reads the "blob" object out of the parsed skin. Keys that are
 *  absent keep their default. */
CoronaConfig loadCoronaConfig (juce::var const &config);

/** Maps raw VU peak/rms onto a perceptual 0..1 level. Peak dominates but is
 *  weighted down so short transients don't wash the corona out. */
/** How big a blob is drawn, as a factor on its base size.
 *
 *  Two sizes and deliberately no third: held, and not held. There used to be a
 *  "last touched" size as well (1.1x), which meant a blob never came back to
 *  the size it started at — you could read off the ball which one you had
 *  touched last, long after it stopped mattering.
 *
 *  `sizeGrabbed` is what held is worth. Note that this is the *drawn* size
 *  only: what a finger has to hit is getActiveDistanceInPixel(), a separate
 *  and deliberately larger radius, so making the mark smaller does not make
 *  the blob harder to catch. Those two shared one number until 2026-09-16 and
 *  a blob under a finger tripled. */
float blobDrawScale (bool grabbed, CoronaConfig const &config);

/** How wide to draw one of the blob's filaments, never thinner than the
 *  screen can carry.
 *
 *  Everything the blob throws off is measured in blob radii, so it all shrinks
 *  with the blob — and the blob is small. At the shipped scale its bolt core
 *  is 0.96 screen pixels wide before `taper` narrows it further along the arm,
 *  so most of a bolt was thinner than a pixel. boltAt() is width/(d+width):
 *  below a pixel the value between two sample points swings from nearly one to
 *  nearly nothing, and the arm comes apart into flecks instead of reading as a
 *  line. That is what "die blitze am blob sehen kaputt aus" was looking at,
 *  and shrinking the held blob from 3x to 1.5x is what pushed the held one
 *  under as well.
 *
 *  `pixelInUv` is 1/sphereRadiusInPixels — the shader knows it as the same
 *  quantity aaWidth is built from. Mirrors blobFilamentWidth() in
 *  SphereShader.cc. */
float blobFilamentWidth (float wanted, float pixelInUv);

float coronaVuLevel (float vuPeak, float vuRms, float vuMax);

/** The peak leg of that curve on its own — drives alpha and white blend. */
float coronaPeakLevel (float vuPeak, float vuMax);

/** Corona diameter as a multiple of the blob diameter. */
float coronaScaleFactor (float vuLevel, CoronaConfig const &config);

/** True when the corona actually reaches past the solid blob disc drawn over
 *  it — i.e. when it is visible at all. */
bool coronaExtendsBeyondBlob (float scaleFactor);

}
