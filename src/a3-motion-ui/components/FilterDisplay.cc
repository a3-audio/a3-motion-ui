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

#include "FilterDisplay.hh"

#include <a3-motion-ui/components/LookAndFeel.hh>

#include <cmath>

namespace a3
{

FilterDisplay::FilterDisplay ()
{
}

void
FilterDisplay::resized ()
{
}

void
FilterDisplay::paint (juce::Graphics &g)
{
  auto bounds = getLocalBounds ();

  // Grey background matching the StatusBar
  g.setColour (Colours::statusBar ());
  g.fillRect (bounds);

  // Divide into 4 equal channel sections
  auto const sectionWidth = bounds.getWidth () / numChannels;

  for (int ch = 0; ch < numChannels; ++ch)
    {
      auto sectionBounds
          = bounds.removeFromLeft (ch < numChannels - 1 ? sectionWidth
                                                        : bounds.getWidth ());
      paintChannel (g, sectionBounds, ch);

      // Draw thin separator line between sections
      if (ch < numChannels - 1)
        {
          g.setColour (Colours::background ());
          g.drawVerticalLine (sectionBounds.getRight (),
                              static_cast<float> (sectionBounds.getY ()),
                              static_cast<float> (sectionBounds.getBottom ()));
        }
    }
}

std::pair<float, float>
FilterDisplay::computeCutoffs (float sweep, float q)
{
  // Sweep controls the center position of the filter.
  // Q controls both the width between LP and HP cutoffs AND the steepness.
  //   Low Q = wide gap, gentle slopes (almost fullrange)
  //   High Q = narrow gap, steep slopes (narrow bandpass)
  //
  // Cutoffs extend beyond [0,1] to avoid visual "snap" at edges.

  float const overshoot = 0.35f;  // extend beyond visible range for smooth edges
  float const minPos = -overshoot;
  float const maxPos = 1.f + overshoot;

  // Center position: sweep 0→1 maps linearly across full range
  float center = minPos + sweep * (maxPos - minPos);

  // Half-width between center and each cutoff
  // At Q=0: very wide (fullrange-like), cutoffs far apart
  // At Q=1: very narrow, cutoffs close together
  float const maxHalfWidth = 0.7f;   // wide at Q=0
  float const minHalfWidth = 0.08f;  // narrow at Q=1
  float halfWidth = maxHalfWidth - q * (maxHalfWidth - minHalfWidth);

  // Create symmetric bell around center
  float lpCutoff = center + halfWidth;
  float hpCutoff = center - halfWidth;

  return { lpCutoff, hpCutoff };
}

float
FilterDisplay::butterworthResponse (float x, float cutoff, bool isHighpass,
                                    float steepness)
{
  // Butterworth-style sigmoid rolloff
  // steepness controls how steep the transition is (higher = steeper)
  float dist;
  if (isHighpass)
    dist = (cutoff - x) * steepness;  // positive when x < cutoff (attenuated)
  else
    dist = (x - cutoff) * steepness;  // positive when x > cutoff (attenuated)

  if (dist < -10.f)
    return 1.f;
  if (dist > 10.f)
    return 0.f;

  // Smooth sigmoid: 1 / (1 + exp(dist))^2 for steeper rolloff
  float sigmoid = 1.f / (1.f + std::exp (dist));
  return sigmoid * sigmoid;  // squared for ~4th order Butterworth character
}

void
FilterDisplay::paintChannel (juce::Graphics &g,
                             juce::Rectangle<int> bounds, int channel)
{
  auto const sweep = _channels[channel].sweep.load ();
  auto const q = _channels[channel].q.load ();
  auto const colour = _channels[channel].colour;

  auto [lpCutoff, hpCutoff] = computeCutoffs (sweep, q);

  auto const leftX = static_cast<float> (bounds.getX ());
  auto const rightX = static_cast<float> (bounds.getRight ());
  auto const top = static_cast<float> (bounds.getY ());
  auto const bottom = static_cast<float> (bounds.getBottom ());
  // Use full height, only minimal margin for stroke visibility
  auto const drawBottom = bottom - 1.f;
  auto const drawTop = top + 1.f;
  auto const drawHeight = drawBottom - drawTop;

  // Steepness factor for rolloff (higher = steeper filter)
  // Q controls steepness: low Q = gentle slopes, high Q = steep slopes
  auto const steepness = 8.f + q * 25.f;

  // Compute raw response for all points
  auto constexpr numPoints = 80;
  float responses[numPoints + 1];
  float maxResponse = 0.f;

  for (int i = 0; i <= numPoints; ++i)
    {
      auto const normX = static_cast<float> (i) / numPoints;
      float response = 1.f;

      // Apply LP filter if cutoff is within or near visible range
      if (lpCutoff < 1.5f)
        response *= butterworthResponse (normX, lpCutoff, false, steepness);
      // Apply HP filter if cutoff is within or near visible range
      if (hpCutoff > -0.5f)
        response *= butterworthResponse (normX, hpCutoff, true, steepness);

      responses[i] = response;
      if (response > maxResponse)
        maxResponse = response;
    }

  // Normalize so the passband always reaches full height
  if (maxResponse > 0.001f)
    {
      auto const invMax = 1.f / maxResponse;
      for (int i = 0; i <= numPoints; ++i)
        responses[i] *= invMax;
    }

  // Build the frequency response path
  juce::Path curvePath;
  curvePath.startNewSubPath (leftX, drawBottom);

  for (int i = 0; i <= numPoints; ++i)
    {
      auto const normX = static_cast<float> (i) / numPoints;
      auto const x = leftX + normX * (rightX - leftX);
      auto const y = drawBottom - responses[i] * drawHeight;
      curvePath.lineTo (x, y);
    }

  curvePath.lineTo (rightX, drawBottom);
  curvePath.closeSubPath ();

  // Fill with semi-transparent channel colour
  g.setColour (colour.withAlpha (0.2f));
  g.fillPath (curvePath);

  // Stroke the top edge of the curve
  juce::Path strokePath;
  for (int i = 0; i <= numPoints; ++i)
    {
      auto const normX = static_cast<float> (i) / numPoints;
      auto const x = leftX + normX * (rightX - leftX);
      auto const y = drawBottom - responses[i] * drawHeight;
      if (i == 0)
        strokePath.startNewSubPath (x, y);
      else
        strokePath.lineTo (x, y);
    }
  g.setColour (colour.withAlpha (0.7f));
  g.strokePath (strokePath, juce::PathStrokeType (1.5f));

  // Thin baseline
  g.setColour (colour.withAlpha (0.1f));
  g.drawHorizontalLine (static_cast<int> (drawBottom), leftX, rightX);
}

void
FilterDisplay::setSweep (int channel, float normSweep)
{
  jassert (channel >= 0 && channel < numChannels);
  _channels[channel].sweep = juce::jlimit (0.f, 1.f, normSweep);
  repaint ();
}

void
FilterDisplay::setQ (int channel, float normQ)
{
  jassert (channel >= 0 && channel < numChannels);
  _channels[channel].q = juce::jlimit (0.f, 1.f, normQ);
  repaint ();
}

void
FilterDisplay::setChannelColour (int channel, juce::Colour colour)
{
  jassert (channel >= 0 && channel < numChannels);
  _channels[channel].colour = colour;
}

}
