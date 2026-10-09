/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include "BodyLook.hh"

#include <a3-motion-ui/components/fpv/FlightScene.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

#include <cmath>

namespace a3
{

namespace
{
constexpr float hatchOfRadius = 0.3f;    // the gap between two hatch lines
constexpr float holdRingOfRadius = 1.2f; // the removal ring round the disc
constexpr float holdRingOfStroke = 3.f;  // it is heavier than an outline
constexpr float labelBoxOfFont = 1.2f;

void
paintRing (juce::Graphics &g, BodyPaint const &body, ThemeColour colour)
{
  if (body.ringRadius <= 0.f)
    return;
  g.setColour (toColour (colour, theme ().alphaGuide));
  g.drawEllipse (juce::Rectangle<float> (2.f * body.ringRadius,
                                         2.f * body.ringRadius)
                     .withCentre (body.centre),
                 body.stroke);
}

void
paintHatch (juce::Graphics &g, juce::Rectangle<float> disc, float stroke)
{
  juce::Path clip;
  clip.addEllipse (disc);
  juce::Graphics::ScopedSaveState state (g);
  g.reduceClipRegion (clip);

  auto const gap = disc.getWidth () / 2.f * hatchOfRadius;
  // A disc of no size has nothing to hatch, and a zero step never ends.
  if (!(gap > 0.f))
    return;
  auto const span = disc.getWidth ();
  for (auto offset = -span; offset <= span; offset += gap)
    g.drawLine (disc.getX () + offset, disc.getBottom (),
                disc.getX () + offset + span, disc.getY (), stroke);
}

void
paintHoldRing (juce::Graphics &g, BodyPaint const &body, float radius)
{
  auto const progress = juce::jlimit (0.f, 1.f, body.holdProgress);
  if (progress <= 0.f)
    return;
  auto const ring = radius * holdRingOfRadius;
  juce::Path arc;
  arc.addCentredArc (body.centre.x, body.centre.y, ring, ring, 0.f, 0.f,
                     progress * juce::MathConstants<float>::twoPi, true);
  g.setColour (toColour (theme ().danger));
  g.strokePath (arc, juce::PathStrokeType (body.stroke * holdRingOfStroke));
}
}

BodyRole
bodyRole (float mass)
{
  return mass < 0.f ? BodyRole::Repel : BodyRole::Attract;
}

ThemeColour
bodyColour (BodyRole role)
{
  return role == BodyRole::Repel ? theme ().danger : theme ().textPrimary;
}

float
bodyWeightScale (float mass, FlightTuning const &tuning)
{
  // Relative to a group, so lighter planets are drawn as before; a dead zone
  // is drawn a crowd's size whatever its push.
  auto const drawn = mass < 0.f ? tuning.crowdMass : mass;
  return std::sqrt (drawn / tuning.groupMass);
}

float
bodyRadius (float mass, float blobDiameter, FlightTuning const &tuning)
{
  return bodyRadiusOfBlob * bodyWeightScale (mass, tuning) * blobDiameter;
}

float
bodyPulseScale (float pulse)
{
  return 1.f + (pulse - 1.f) * bodyPulseOfGravity;
}

float
bodyHitRadius (float mass, float blobDiameter, float fingertip,
               FlightTuning const &tuning)
{
  return std::max (bodyRadius (mass, blobDiameter, tuning), fingertip / 2.f);
}

juce::String
bodyLabel (int id)
{
  return "G" + juce::String (id + 1);
}

juce::Rectangle<float>
bodyLabelBox (juce::Point<float> above, float fontHeight)
{
  // Wide enough for "G8" at any font, and a little taller than the font so
  // descenders do not reach into the head below.
  auto const height = fontHeight * labelBoxOfFont;
  return juce::Rectangle<float> (height * 3.f, height)
      .withCentre (above)
      .withY (above.y - height);
}

void
paintBody (juce::Graphics &g, BodyPaint const &body)
{
  auto const role = bodyRole (body.mass);
  auto const colour = bodyColour (role);
  auto const radius = body.radius * bodyPulseScale (body.pulse);
  auto const disc
      = juce::Rectangle<float> (2.f * radius, 2.f * radius).withCentre (body.centre);

  paintRing (g, body, colour);

  if (role == BodyRole::Repel)
    {
      g.setColour (toColour (colour, theme ().alphaTextStrong));
      paintHatch (g, disc, body.stroke);
    }
  else
    {
      g.setColour (toColour (colour, theme ().alphaFillEmphasis));
      g.fillEllipse (disc);
    }

  g.setColour (toColour (colour, theme ().alphaSecondary));
  g.drawEllipse (disc, body.stroke);

  g.setColour (toColour (theme ().textPrimary,
                         body.hidden ? ghostLabelAlpha : 1.f));
  g.setFont (juce::Font (juce::FontOptions (body.fontHeight)));
  if (body.labelAbove)
    g.drawText (body.label, bodyLabelBox (*body.labelAbove, body.fontHeight),
                juce::Justification::centredBottom, false);
  else
    g.drawText (body.label, disc, juce::Justification::centred, false);

  paintHoldRing (g, body, radius);
}

}
