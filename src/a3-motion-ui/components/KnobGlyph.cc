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
// Half the groove between two arrows: short enough to clear them, so the
// shapes never overlap and cancel out under the fill rule.
constexpr float grooveShare = 0.1f;
// The folder: its body, its tab, its corners.
constexpr float folderHalfWidth = 0.42f;
constexpr float folderTop = -0.22f;
constexpr float folderBottom = 0.3f;
constexpr float folderTabWidth = 0.36f;
constexpr float folderTabHeight = 0.1f;
constexpr float folderCorner = 0.05f;
// The list: three lines, the middle one marked by an arrow at its left.
constexpr float listLineSpacing = 0.28f;
constexpr float listLineLeft = -0.12f;
constexpr float listLineRight = 0.45f;
constexpr float listMarkerSize = 0.13f;

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

/** Two arrows apart with a short groove between them, along `angle` (0 =
 *  up and down). */
void
addArrowPair (juce::Path &filled, juce::Path &lines, juce::Point<float> c,
              float s, float angle)
{
  auto const tri = s * triangleShare;
  auto const along = juce::Point<float> (0.f, -1.f).rotatedAboutOrigin (angle);
  auto const reach = s * halfExtent - tri;
  addArrow (filled, c + along * reach, tri, angle);
  addArrow (filled, c - along * reach, tri,
            angle + juce::MathConstants<float>::pi);
  addLine (lines, c - along * (s * grooveShare), c + along * (s * grooveShare));
}
}

juce::Path
knobGlyphPath (KnobGlyph glyph, juce::Rectangle<float> box)
{
  auto const s = juce::jmin (box.getWidth (), box.getHeight ());
  auto const c = box.getCentre ();

  juce::Path filled;
  juce::Path lines;
  switch (glyph)
    {
    case KnobGlyph::None: return {};
    case KnobGlyph::UpDown: addArrowPair (filled, lines, c, s, 0.f); break;
    case KnobGlyph::LeftRight:
      addArrowPair (filled, lines, c, s, juce::MathConstants<float>::halfPi);
      break;
    case KnobGlyph::Folder:
      {
        auto const left = c.x - s * folderHalfWidth;
        filled.addRoundedRectangle (left, c.y + s * folderTop,
                                    2.f * s * folderHalfWidth,
                                    s * (folderBottom - folderTop),
                                    s * folderCorner);
        filled.addRoundedRectangle (left, c.y + s * (folderTop - folderTabHeight),
                                    s * folderTabWidth,
                                    s * folderTabHeight * 2.f,
                                    s * folderCorner);
        break;
      }
    case KnobGlyph::File:
      for (int line = -1; line <= 1; ++line)
        {
          auto const y = c.y + static_cast<float> (line) * s * listLineSpacing;
          addLine (lines, { c.x + s * listLineLeft, y },
                   { c.x + s * listLineRight, y });
        }
      addArrow (filled, { c.x - s * halfExtent + s * listMarkerSize, c.y },
                s * listMarkerSize, juce::MathConstants<float>::halfPi);
      break;
    }

  juce::Path outlined;
  juce::PathStrokeType (s * lineShare, juce::PathStrokeType::curved,
                        juce::PathStrokeType::rounded)
      .createStrokedPath (outlined, lines);
  filled.addPath (outlined);
  return filled;
}

}
