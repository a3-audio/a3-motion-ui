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

#include <a3-motion-ui/components/ChannelUIState.hh>
#include <a3-motion-ui/components/LookAndFeel.hh>

namespace a3
{

DirectivityComponent::DirectivityComponent (ChannelUIState const &uiState)
    : _uiState (uiState), _width (180.f), _order (1)
{
}

void
DirectivityComponent::paint (juce::Graphics &g)
{
  auto bounds = getLocalBounds ().toFloat ();
  // jassert (bounds.getWidth () == bounds.getHeight ());

  auto const widthArcDirectivity = bounds.getWidth () * 0.05f;
  auto const radiusArcDirectivity
      = (bounds.getWidth () - widthArcDirectivity) / 2.f;

  auto const widthArcGap = bounds.getWidth () * 0.02f;

  auto const radiusInnerCircle
      = bounds.getWidth () / 2.f - widthArcDirectivity - widthArcGap;

  auto const widthArcProgress = bounds.getWidth () * 0.1f;
  auto const radiusArcProgress = radiusInnerCircle - widthArcProgress / 2.f;

  auto const center = bounds.getCentre ();

  auto strokeType
      = juce::PathStrokeType (widthArcProgress, //
                              juce::PathStrokeType::JointStyle::mitered,
                              juce::PathStrokeType::EndCapStyle::butt);

  g.setColour (_uiState.colour.withLightness (0.3));
  g.fillEllipse (bounds.withSizeKeepingCentre (2.f * radiusInnerCircle,
                                               2.f * radiusInnerCircle));

  auto path = juce::Path ();
  auto angleStart = 0.f;
  auto angleEnd = juce::degreesToRadians (360.f) * _uiState.progress;

  path.addCentredArc (center.getX (), center.getY (), radiusArcProgress,
                      radiusArcProgress, 0.f, angleStart, angleEnd, true);
  g.setColour (_uiState.colour);
  g.strokePath (path, strokeType);
  path.clear ();

  auto pieAngleRad = juce::degreesToRadians (360.f);
  if (_order > 0)
    {
      pieAngleRad /= 2.f * _order;
    }

  angleStart = juce::degreesToRadians (-_width / 2.f) - pieAngleRad / 2.f;
  angleEnd = juce::degreesToRadians (-_width / 2.f) + pieAngleRad / 2.f;

  path = juce::Path ();
  path.addCentredArc (center.getX (), center.getY (), radiusArcDirectivity,
                      radiusArcDirectivity, 0.f, angleStart, angleEnd, true);

  strokeType.setStrokeThickness (widthArcDirectivity);
  g.setColour (juce::Colours::white.withAlpha (0.5f));
  g.strokePath (path, strokeType);

  path.clear ();
  angleStart = juce::degreesToRadians (_width / 2.f) - pieAngleRad / 2.f;
  angleEnd = juce::degreesToRadians (_width / 2.f) + pieAngleRad / 2.f;
  path.addCentredArc (center.getX (), center.getY (), radiusArcDirectivity,
                      radiusArcDirectivity, 0.f, angleStart, angleEnd, true);
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
