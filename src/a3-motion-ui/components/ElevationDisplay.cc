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

#include <a3-motion-ui/theme/Theme.hh>

#include <a3-motion-ui/components/LookAndFeel.hh>

namespace a3
{

namespace
{
// Structural opacities: the hairline between cells, the dark outline that
// keeps text readable on a channel-coloured ground, the frame around the row
// the encoder is on.
constexpr float cellBorderWash = 0.15f;
constexpr float outlineOpacity = 0.5f;
constexpr float highlightOpacity = 0.8f;
}

ElevationDisplay::ElevationDisplay () = default;

void
ElevationDisplay::resized ()
{
}

void
ElevationDisplay::paint (juce::Graphics &g)
{
  auto bounds = getLocalBounds ();

  // Grey background matching the StatusBar (same base as PadRowDisplay)
  g.setColour (Colours::statusBar ());
  g.fillRect (bounds);

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
  auto const colour = cell.colour;

  // Background: channel colour, intensity depends on state (like PadRowDisplay)
  auto bgAlpha = cell.cellSelected ? 0.85f
                 : cell.rowHighlighted ? 0.55f
                                       : 0.25f;
  g.setColour (colour.withAlpha (bgAlpha));
  g.fillRect (bounds);

  // Border between cells
  g.setColour (toColour (theme ().surface, cellBorderWash));
  g.drawVerticalLine (bounds.getRight () - 1, static_cast<float> (bounds.getY ()),
                      static_cast<float> (bounds.getBottom ()));

  // Content: "ELV" and coverage value
  // Use dark outline + channel colour text for readability on coloured bg
  auto boundsF = bounds.toFloat ().reduced (2.f);
  auto const font = juce::Font (h * 0.55f * theme ().scaleFor (FontRole::Body));
  g.setFont (font);

  auto const outlineColour = toColour (theme ().surface, outlineOpacity);

  // Label "ELV"
  auto labelArea = boundsF.removeFromLeft (boundsF.getWidth () * 0.4f);
  for (int dx = -1; dx <= 1; ++dx)
    for (int dy = -1; dy <= 1; ++dy)
      if (dx != 0 || dy != 0)
        {
          g.setColour (outlineColour);
          g.drawText ("ELV", labelArea.translated (
                          static_cast<float> (dx), static_cast<float> (dy)),
                      juce::Justification::centredLeft, false);
        }
  g.setColour (colour.brighter (0.2f));
  g.drawText ("ELV", labelArea, juce::Justification::centredLeft, false);

  // Coverage value as percentage (per-channel)
  auto const coveragePercent = static_cast<int> (std::round (cell.coverage * 100.f));
  auto valueStr = juce::String (coveragePercent) + "%";
  for (int dx = -1; dx <= 1; ++dx)
    for (int dy = -1; dy <= 1; ++dy)
      if (dx != 0 || dy != 0)
        {
          g.setColour (outlineColour);
          g.drawText (valueStr, boundsF.translated (
                          static_cast<float> (dx), static_cast<float> (dy)),
                      juce::Justification::centredRight, false);
        }
  g.setColour (colour.brighter (0.3f));
  g.drawText (valueStr, boundsF, juce::Justification::centredRight, false);

  // Draw a mini sphere icon showing coverage (small arc)
  auto iconArea = bounds.toFloat ().reduced (2.f);
  auto iconX = iconArea.getCentreX ();
  auto iconY = iconArea.getCentreY ();
  auto iconR = h * 0.25f;

  // Draw sphere outline
  g.setColour (outlineColour);
  g.drawEllipse (iconX - iconR - 0.5f, iconY - iconR - 0.5f,
                 iconR * 2.f + 1.f, iconR * 2.f + 1.f, 2.f);
  g.setColour (colour.withAlpha (0.5f));
  g.drawEllipse (iconX - iconR, iconY - iconR,
                 iconR * 2.f, iconR * 2.f, 1.f);

  // Fill arc to show coverage (from top)
  if (cell.coverage > 0.01f)
    {
      auto coverageAngle = cell.coverage * juce::MathConstants<float>::pi;
      auto arcPath = juce::Path ();
      arcPath.addCentredArc (iconX, iconY, iconR, iconR,
                             0.f,
                             -coverageAngle, coverageAngle,
                             true);
      arcPath.lineTo (iconX, iconY);
      arcPath.closeSubPath ();
      g.setColour (colour.withAlpha (0.4f));
      g.fillPath (arcPath);
    }

  // White border when row is highlighted (hovered by encoder)
  if (cell.rowHighlighted)
    {
      g.setColour (toColour (theme ().textPrimary, highlightOpacity));
      g.drawRect (bounds, 2);
    }
}

void
ElevationDisplay::setChannelColour (int channel, juce::Colour colour)
{
  if (channel >= 0 && channel < numChannels)
    _cells[static_cast<size_t> (channel)].colour = colour;
}

}
