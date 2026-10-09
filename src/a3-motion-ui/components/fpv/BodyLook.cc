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
  auto const radius = body.radius * bodyPulseScale (body.pulse);
  auto const disc
      = juce::Rectangle<float> (2.f * radius, 2.f * radius).withCentre (body.centre);

  g.setColour (toColour (theme ().textPrimary, labelAlpha (body.hidden)));
  g.setFont (juce::Font (juce::FontOptions (body.fontHeight)));
  g.drawText (body.label, disc, juce::Justification::centred, false);
}

}
