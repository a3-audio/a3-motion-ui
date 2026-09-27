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
/** A field's name tab, engraved: the ground sunk a shade darker, deepest
 *  under its top edge, a lit edge where the cut meets the field, and the words
 *  cut in with a faint light copy below them. */
constexpr float captionTabDeep = 0.55f;
constexpr float captionTabShallow = 0.3f;
constexpr float captionTabLitEdge = 0.14f;
constexpr float captionEngraveLight = 0.12f;

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
      g.setColour (Colours::barText (isSelected));
      g.drawFittedText (caption, captionArea, juce::Justification::centred, 1);
    }

  auto const valueSize
      = juce::jmin (metrics.valueSize,
                    static_cast<float> (box.getHeight ()) * 0.9f);
  g.setFont (juce::Font (valueSize, juce::Font::plain));
  // A value that has a colour of its own — the clock's mode — writes itself
  // in it. Everything else takes the bar's.
  g.setColour (valueColour.isTransparent () ? Colours::barText (isSelected)
                                            : valueColour);

  g.drawFittedText (label, box, juce::Justification::centred, 1);
}

void
paintFieldCaptionTab (juce::Graphics &g, juce::Rectangle<int> field,
                      juce::String const &text, float captionSize)
{
  if (text.isEmpty () || field.isEmpty ())
    return;

  juce::Font const font{ juce::FontOptions (captionSize) };
  auto const hair = theme ().strokeThin;
  // Inside the field's own outline, so the field keeps its edge all round.
  auto const tab
      = fieldCaptionPlate (field, captionSize,
                           juce::GlyphArrangement::getStringWidth (font, text))
            .toFloat ()
            .withTrimmedLeft (hair)
            .withTrimmedTop (hair);

  // Rounded where the field is (top left) and where the cut turns back into
  // the field (bottom right); square along the two edges it shares.
  auto const radius
      = juce::jmin (theme ().radiusControl, tab.getHeight () / 2.f);
  juce::Path shape;
  shape.addRoundedRectangle (tab.getX (), tab.getY (), tab.getWidth (),
                             tab.getHeight (), radius, radius, true, false,
                             false, true);

  // Sunk into the field: darkest under its top edge, as if that edge threw a
  // shadow into the cut.
  g.setGradientFill (juce::ColourGradient (
      toColour (theme ().background, captionTabDeep), tab.getTopLeft (),
      toColour (theme ().background, captionTabShallow), tab.getBottomLeft (),
      false));
  g.fillPath (shape);

  // The cut's far walls catch the light: a hairline down its right side and
  // along its foot, round the corner between them.
  auto const right = tab.getRight () - hair * 0.5f;
  auto const bottom = tab.getBottom () - hair * 0.5f;
  juce::Path litEdge;
  litEdge.startNewSubPath (right, tab.getY ());
  litEdge.lineTo (right, bottom - radius);
  litEdge.quadraticTo (right, bottom, right - radius, bottom);
  litEdge.lineTo (tab.getX (), bottom);
  g.setColour (toColour (theme ().textPrimary, captionTabLitEdge));
  g.strokePath (litEdge, juce::PathStrokeType (hair));

  // The words cut in: a faint light copy a hairline lower, then the words.
  g.setFont (font);
  auto const words = tab.toNearestInt ();
  g.setColour (toColour (theme ().textPrimary, captionEngraveLight));
  g.drawText (text, words.translated (0, juce::roundToInt (hair)),
              juce::Justification::centred, true);
  g.setColour (toColour (theme ().textMuted));
  g.drawText (text, words, juce::Justification::centred, true);
}

}
