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

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/components/KnobGlyph.hh>

using namespace a3;

namespace
{
juce::Rectangle<float> const box{ 10.f, 20.f, 40.f, 40.f };
}

TEST (KnobGlyph, NoneIsEmpty)
{
  EXPECT_TRUE (knobGlyphPath (KnobGlyph::None, box).isEmpty ());
}

// Each sign is drawn, and stays inside the box it was given.
TEST (KnobGlyph, EachGlyphIsDrawnInsideItsBox)
{
  for (auto glyph : { KnobGlyph::Cursor, KnobGlyph::ScrollVertical,
                      KnobGlyph::ScrollHorizontal })
    {
      auto const path = knobGlyphPath (glyph, box);
      EXPECT_FALSE (path.isEmpty ()) << static_cast<int> (glyph);
      EXPECT_TRUE (box.contains (path.getBounds ()))
          << static_cast<int> (glyph);
    }
}

// Up-and-down stands, left-and-right lies: the shape says the direction.
TEST (KnobGlyph, TheScrollSignsPointTheirWay)
{
  auto const upright = knobGlyphPath (KnobGlyph::ScrollVertical, box).getBounds ();
  auto const lying
      = knobGlyphPath (KnobGlyph::ScrollHorizontal, box).getBounds ();
  EXPECT_GT (upright.getHeight (), 2.f * upright.getWidth ());
  EXPECT_GT (lying.getWidth (), 2.f * lying.getHeight ());
}

// The cursor reaches out both sides: its triangles point left and right.
TEST (KnobGlyph, TheCursorIsCentredAndReachesBothSides)
{
  auto const bounds = knobGlyphPath (KnobGlyph::Cursor, box).getBounds ();
  EXPECT_NEAR (bounds.getCentreX (), box.getCentreX (), 0.5f);
  EXPECT_NEAR (bounds.getCentreY (), box.getCentreY (), 0.5f);
  EXPECT_GT (bounds.getWidth (), 0.6f * box.getWidth ());
}

// It scales with its box: no pixel sizes.
TEST (KnobGlyph, ItScalesWithItsBox)
{
  auto const small = knobGlyphPath (KnobGlyph::ScrollVertical, box).getBounds ();
  auto const big = knobGlyphPath (KnobGlyph::ScrollVertical,
                                  box.withSizeKeepingCentre (80.f, 80.f))
                       .getBounds ();
  EXPECT_NEAR (big.getHeight (), 2.f * small.getHeight (), 0.5f);
}
