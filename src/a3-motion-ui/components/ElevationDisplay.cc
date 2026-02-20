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

#include "ElevationDisplay.hh"

#include <a3-motion-ui/components/LookAndFeel.hh>

namespace a3
{

ElevationDisplay::ElevationDisplay () = default;

void
ElevationDisplay::resized ()
{
}

void
ElevationDisplay::paint (juce::Graphics &g)
{
  auto bounds = getLocalBounds ();
  auto const cellWidth = bounds.getWidth () / numChannels;

  for (int ch = 0; ch < numChannels; ++ch)
    {
      auto cellBounds = bounds.removeFromLeft (cellWidth);
      paintCell (g, cellBounds, ch);
    }
}

void
ElevationDisplay::paintCell (juce::Graphics &g,
                              juce::Rectangle<int> bounds,
                              int channel)
{
  auto const &cell = _cells[static_cast<size_t> (channel)];
  auto const h = static_cast<float> (bounds.getHeight ());

  // Background
  if (cell.cellSelected)
    g.setColour (cell.colour.withAlpha (0.3f));
  else if (cell.rowHighlighted)
    g.setColour (juce::Colour (0xff3a3f47));
  else
    g.setColour (juce::Colour (0xff292f36));
  g.fillRect (bounds);

  // Border between cells
  g.setColour (juce::Colour (0xff1a1d22));
  g.drawVerticalLine (bounds.getRight () - 1, static_cast<float> (bounds.getY ()),
                      static_cast<float> (bounds.getBottom ()));

  // Content: "ELV" and coverage value
  auto boundsF = bounds.toFloat ().reduced (2.f);
  auto const font = juce::Font (h * 0.55f);
  g.setFont (font);

  // Label
  g.setColour (cell.colour.withAlpha (0.6f));
  g.drawText ("ELV", boundsF.removeFromLeft (boundsF.getWidth () * 0.4f),
              juce::Justification::centredLeft, false);

  // Coverage value as percentage (per-channel)
  auto const coveragePercent = static_cast<int> (std::round (cell.coverage * 100.f));
  auto valueStr = juce::String (coveragePercent) + "%";
  g.setColour (cell.colour);
  g.drawText (valueStr, boundsF, juce::Justification::centredRight, false);

  // Draw a mini sphere icon showing coverage (small arc)
  // The arc fills from top to bottom proportional to coverage
  auto iconArea = bounds.toFloat ().reduced (2.f);
  auto iconX = iconArea.getCentreX ();
  auto iconY = iconArea.getCentreY ();
  auto iconR = h * 0.25f;

  // Draw sphere outline
  g.setColour (cell.colour.withAlpha (0.3f));
  g.drawEllipse (iconX - iconR, iconY - iconR,
                 iconR * 2.f, iconR * 2.f, 1.f);

  // Fill arc to show coverage (from top)
  if (cell.coverage > 0.01f)
    {
      auto coverageAngle = cell.coverage * juce::MathConstants<float>::pi;
      auto arcPath = juce::Path ();
      // Arc from top, sweeping down by coverage angle
      arcPath.addCentredArc (iconX, iconY, iconR, iconR,
                             0.f,
                             -coverageAngle, coverageAngle,
                             true);
      arcPath.lineTo (iconX, iconY);
      arcPath.closeSubPath ();
      g.setColour (cell.colour.withAlpha (0.4f));
      g.fillPath (arcPath);
    }
}

void
ElevationDisplay::setCoverage (int channel, float coverage)
{
  if (channel >= 0 && channel < numChannels)
    {
      _cells[static_cast<size_t> (channel)].coverage
          = std::clamp (coverage, 0.05f, 1.0f);
      repaint ();
    }
}

float
ElevationDisplay::getCoverage (int channel) const
{
  if (channel >= 0 && channel < numChannels)
    return _cells[static_cast<size_t> (channel)].coverage;
  return 0.5f;
}

void
ElevationDisplay::setChannelColour (int channel, juce::Colour colour)
{
  if (channel >= 0 && channel < numChannels)
    _cells[static_cast<size_t> (channel)].colour = colour;
}

void
ElevationDisplay::setRowHighlighted (int channel, bool highlighted)
{
  if (channel >= 0 && channel < numChannels)
    {
      _cells[static_cast<size_t> (channel)].rowHighlighted = highlighted;
      repaint ();
    }
}

void
ElevationDisplay::setCellSelected (int channel, bool selected)
{
  if (channel >= 0 && channel < numChannels)
    {
      _cells[static_cast<size_t> (channel)].cellSelected = selected;
      repaint ();
    }
}

}
