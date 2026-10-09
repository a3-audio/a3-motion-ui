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
constexpr float holdRingOfRadius = 1.2f; // the removal ring round the disc
constexpr float holdRingOfStroke = 3.f;  // it is heavier than an outline

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

juce::String
bodyLabel (int id)
{
  return "G" + juce::String (id + 1);
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

  // The mark itself -- a group's fill and outline, a dead zone's hatch -- is
  // the sphere shader's, painted on the dance floor so it lies there.
  g.setColour (toColour (theme ().textPrimary, labelAlpha (body.hidden)));
  g.setFont (juce::Font (juce::FontOptions (body.fontHeight)));
  g.drawText (body.label, disc, juce::Justification::centred, false);

  paintHoldRing (g, body, radius);
}

}
