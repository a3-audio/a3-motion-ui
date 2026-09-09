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

#include "BarButton.hh"

#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
// The bar's own three washes, brought across with the drawing rather than
// guessed at again — the same move BarKnob.cc made with its two.
constexpr float cardWash = 0.08f;
constexpr float highlightWash = 0.18f;
constexpr float trackWash = 0.18f;

/** What is written on the face. Grey either way: what says whose button this
 *  is, is the ground behind it, and a word in the channel's colour beside one
 *  in grey reads as a difference in the setting rather than in which section
 *  a finger last touched. Selected means no dimming at all, which the
 *  alpha-less overload already says. */
juce::Colour
faceTextColour (bool isSelected)
{
  return isSelected ? toColour (theme ().textMuted)
                    : toColour (theme ().textMuted, theme ().alphaInactive);
}
}

void
paintBarButton (juce::Graphics &g, juce::Rectangle<int> bounds,
                ControlMetrics metrics, juce::Colour channelColour,
                juce::String const &label, juce::String const &caption,
                bool isActive, bool isSelected, juce::Colour valueColour)
{
  if (bounds.isEmpty ())
    return;

  // An active button lights in the channel's colour, except where nothing
  // belongs to a channel — the global section — and it lights grey.
  g.setColour (isActive
                   ? (isSelected
                          ? channelColour.withAlpha (highlightWash * 2.f)
                          : toColour (theme ().textPrimary,
                                      highlightWash * 2.f))
                   : toColour (theme ().textPrimary, cardWash));
  g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);

  g.setColour (toColour (theme ().textPrimary, trackWash));
  g.drawRoundedRectangle (bounds.toFloat (), theme ().radiusControl,
                          theme ().strokeThin);

  // Two lines, both inside the box: the caption on top, the value under it.
  // The caption used to sit below the button, which made a button a
  // different height from the box it looked like and left the name floating
  // between two of them.
  auto box = bounds.reduced (juce::roundToInt (theme ().paddingSmall),
                             juce::roundToInt (theme ().paddingTight));
  auto const captionArea
      = caption.isEmpty () ? juce::Rectangle<int>{}
                           : box.removeFromTop (box.getHeight () * 2 / 5);

  if (caption.isNotEmpty ())
    {
      g.setFont (juce::Font (
          juce::jmin (metrics.captionSize,
                      static_cast<float> (captionArea.getHeight ()) * 0.95f),
          juce::Font::plain));
      g.setColour (faceTextColour (isSelected));
      g.drawFittedText (caption, captionArea, juce::Justification::centred, 1);
    }

  auto const valueSize
      = juce::jmin (metrics.valueSize,
                    static_cast<float> (box.getHeight ()) * 0.9f);
  g.setFont (juce::Font (valueSize, juce::Font::plain));
  // A value that has a colour of its own — the clock's mode — writes itself
  // in it. Everything else takes the bar's.
  g.setColour (valueColour.isTransparent () ? faceTextColour (isSelected)
                                            : valueColour);

  g.drawFittedText (label, box, juce::Justification::centred, 1);
}

}
