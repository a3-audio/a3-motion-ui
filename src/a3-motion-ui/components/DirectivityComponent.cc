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

#include "DirectivityComponent.hh"

namespace a3
{

DirectivityComponent::DirectivityComponent () : _width (90.f), _order (1) {}

void
DirectivityComponent::paint (juce::Graphics &g)
{
  auto bounds = getLocalBounds ();
  jassert (bounds.getWidth () == bounds.getHeight ());

  auto const widthArc = bounds.getWidth () * 0.1f;
  auto const radiusArc = (bounds.getWidth () - widthArc) / 2.f;
  auto const center = bounds.getCentre ();

  auto pieAngleRad = juce::degreesToRadians (360.f);
  if (_order > 0)
    {
      pieAngleRad /= 2.f * _order;
    }

  juce::Logger::writeToLog ("widthArc: " + juce::String (widthArc));
  juce::Logger::writeToLog ("radiusArc: " + juce::String (radiusArc));
  juce::Logger::writeToLog ("center: " + center.toString ());

  auto strokeType
      = juce::PathStrokeType (widthArc, //
                              juce::PathStrokeType::JointStyle::mitered,
                              juce::PathStrokeType::EndCapStyle::butt);

  auto path = juce::Path ();
  auto angleStart = juce::degreesToRadians (-_width / 2.f) - pieAngleRad / 2.f;
  auto angleEnd = juce::degreesToRadians (-_width / 2.f) + pieAngleRad / 2.f;
  path.addCentredArc (center.getX (), center.getY (), radiusArc, radiusArc,
                      0.f, angleStart, angleEnd, true);
  g.setColour (juce::Colours::white.withAlpha (0.5f));
  g.strokePath (path, strokeType);

  path.clear ();
  angleStart = juce::degreesToRadians (_width / 2.f) - pieAngleRad / 2.f;
  angleEnd = juce::degreesToRadians (_width / 2.f) + pieAngleRad / 2.f;
  path.addCentredArc (center.getX (), center.getY (), radiusArc, radiusArc,
                      0.f, angleStart, angleEnd, true);
  g.setColour (juce::Colours::red.withAlpha (0.5f));
  g.strokePath (path, strokeType);
}

void
DirectivityComponent::setWidth (float width)
{
  _width = width;
}

void
DirectivityComponent::setOrder (int order)
{
  _order = order;
}

}
