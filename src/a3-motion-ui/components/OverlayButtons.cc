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


#include "OverlayButtons.hh"

#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
constexpr float faceWash = 0.10f;
constexpr float edgeWash = 0.20f;
}

OverlayButtons::OverlayButtons ()
{
  // Not for itself, but for its children: the gap between the two buttons
  // belongs to whatever is underneath.
  setInterceptsMouseClicks (false, true);

  _backTouch = std::make_unique<TouchControl> ();
  _backTouch->onTap = [this] (int, int) {
    if (onBack)
      onBack ();
  };
  addAndMakeVisible (*_backTouch);

  _closeTouch = std::make_unique<TouchControl> ();
  _closeTouch->onTap = [this] (int, int) {
    if (onClose)
      onClose ();
  };
  addAndMakeVisible (*_closeTouch);
}

int
OverlayButtons::preferredHeight ()
{
  // A finger, not a glyph: these sit over a busy page and are pressed
  // without looking.
  return juce::jmax (
      40, static_cast<int> (theme ().fontSize (FontRole::Header) * 2.2f));
}

void
OverlayButtons::resized ()
{
  auto bounds = getLocalBounds ();
  auto const size = bounds.getHeight ();
  auto const gap = juce::jmax (2, size / 8);

  _closeArea = bounds.removeFromRight (size);
  bounds.removeFromRight (gap);
  _backArea = bounds.removeFromRight (size);

  _backTouch->setBounds (_backArea);
  _closeTouch->setBounds (_closeArea);
}

void
OverlayButtons::paintGlyph (juce::Graphics &g, juce::Rectangle<int> area,
                            bool isClose)
{
  g.setColour (toColour (theme ().textPrimary, faceWash));
  g.fillRoundedRectangle (area.toFloat (), 5.f);
  g.setColour (toColour (theme ().textPrimary, edgeWash));
  g.drawRoundedRectangle (area.toFloat (), 5.f, 1.f);

  auto const centre = area.toFloat ().getCentre ();
  auto const r = static_cast<float> (area.getHeight ()) * 0.22f;
  auto const stroke = juce::jmax (2.f, r * 0.3f);

  // Drawn rather than typed: a glyph would follow the body font and come out
  // a different weight in every skin.
  g.setColour (toColour (theme ().textPrimary));

  if (isClose)
    {
      g.drawLine (centre.x - r, centre.y - r, centre.x + r, centre.y + r,
                  stroke);
      g.drawLine (centre.x - r, centre.y + r, centre.x + r, centre.y - r,
                  stroke);
      return;
    }

  juce::Path chevron;
  chevron.startNewSubPath (centre.x + r * 0.5f, centre.y - r);
  chevron.lineTo (centre.x - r * 0.5f, centre.y);
  chevron.lineTo (centre.x + r * 0.5f, centre.y + r);

  g.strokePath (chevron, juce::PathStrokeType (
                             stroke, juce::PathStrokeType::curved,
                             juce::PathStrokeType::rounded));
}

void
OverlayButtons::paint (juce::Graphics &g)
{
  paintGlyph (g, _backArea, false);
  paintGlyph (g, _closeArea, true);
}

}
