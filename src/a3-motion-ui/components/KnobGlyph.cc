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

#include "KnobGlyph.hh"

namespace a3
{

namespace
{
/** Shares of the glyph's box -- the sign follows the box, nothing else. */
constexpr float halfExtent = 0.48f;   // how far from the centre it reaches
constexpr float triangleShare = 0.2f; // the arrows' size
constexpr float lineShare = 0.09f;    // the strokes' width
// Half the scroll groove: short enough to clear the arrows, so the two
// shapes never overlap and cancel out under the fill rule.
constexpr float grooveShare = 0.1f;
constexpr float caretShare = 0.4f;    // half the I-beam's height
constexpr float serifShare = 0.1f;    // half its serifs' width
constexpr float caretArrowShare = 0.85f; // the caret's arrows, of a scroll arrow

/** The keyboard's arrow: an isosceles triangle with its tip `size` from
 *  `centre` along `angle` (0 = up, clockwise). */
void
addArrow (juce::Path &path, juce::Point<float> centre, float size, float angle)
{
  juce::Path arrow;
  arrow.addTriangle (0.f, -size, -size * 0.9f, size * 0.55f, size * 0.9f,
                     size * 0.55f);
  path.addPath (arrow, juce::AffineTransform::rotation (angle).translated (
                           centre));
}

void
addLine (juce::Path &path, juce::Point<float> from, juce::Point<float> to)
{
  path.startNewSubPath (from);
  path.lineTo (to);
}
}

juce::Path
knobGlyphPath (KnobGlyph glyph, juce::Rectangle<float> box)
{
  auto const s = juce::jmin (box.getWidth (), box.getHeight ());
  auto const c = box.getCentre ();
  auto const reach = s * halfExtent;
  auto const tri = s * triangleShare;
  auto const halfPi = juce::MathConstants<float>::halfPi;

  juce::Path filled;
  juce::Path lines;
  switch (glyph)
    {
    case KnobGlyph::None: return {};
    case KnobGlyph::ScrollVertical:
      addArrow (filled, { c.x, c.y - reach + tri }, tri, 0.f);
      addArrow (filled, { c.x, c.y + reach - tri }, tri, 2.f * halfPi);
      addLine (lines, { c.x, c.y - s * grooveShare },
               { c.x, c.y + s * grooveShare });
      break;
    case KnobGlyph::ScrollHorizontal:
      addArrow (filled, { c.x - reach + tri, c.y }, tri, -halfPi);
      addArrow (filled, { c.x + reach - tri, c.y }, tri, halfPi);
      addLine (lines, { c.x - s * grooveShare, c.y },
               { c.x + s * grooveShare, c.y });
      break;
    case KnobGlyph::Cursor:
      {
        auto const h = s * caretShare;
        auto const serif = s * serifShare;
        addLine (lines, { c.x, c.y - h }, { c.x, c.y + h });
        addLine (lines, { c.x - serif, c.y - h }, { c.x + serif, c.y - h });
        addLine (lines, { c.x - serif, c.y + h }, { c.x + serif, c.y + h });
        auto const t = tri * caretArrowShare;
        addArrow (filled, { c.x - reach + t, c.y }, t, -halfPi);
        addArrow (filled, { c.x + reach - t, c.y }, t, halfPi);
        break;
      }
    }

  juce::Path outlined;
  juce::PathStrokeType (s * lineShare, juce::PathStrokeType::curved,
                        juce::PathStrokeType::rounded)
      .createStrokedPath (outlined, lines);
  filled.addPath (outlined);
  return filled;
}

}
