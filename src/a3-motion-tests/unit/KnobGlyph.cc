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
  for (auto glyph : { KnobGlyph::UpDown, KnobGlyph::LeftRight,
                      KnobGlyph::Folder, KnobGlyph::File })
    {
      auto const path = knobGlyphPath (glyph, box);
      EXPECT_FALSE (path.isEmpty ()) << static_cast<int> (glyph);
      EXPECT_TRUE (box.contains (path.getBounds ()))
          << static_cast<int> (glyph);
    }
}

// Up-and-down stands, left-and-right lies: the shape says the direction.
TEST (KnobGlyph, TheArrowsPointTheirWay)
{
  auto const upright = knobGlyphPath (KnobGlyph::UpDown, box).getBounds ();
  auto const lying = knobGlyphPath (KnobGlyph::LeftRight, box).getBounds ();
  EXPECT_GT (upright.getHeight (), 2.f * upright.getWidth ());
  EXPECT_GT (lying.getWidth (), 2.f * lying.getHeight ());
}

// A folder and a list are told apart at a glance: a folder is a solid shape
// wider than it is tall, a list a stack of lines.
TEST (KnobGlyph, AFolderIsNotAList)
{
  auto const folder = knobGlyphPath (KnobGlyph::Folder, box);
  auto const list = knobGlyphPath (KnobGlyph::File, box);
  EXPECT_GT (folder.getBounds ().getWidth (),
             folder.getBounds ().getHeight ());
  // The folder is filled in its middle, the list has a gap between lines.
  auto const centre = box.getCentre ();
  EXPECT_TRUE (folder.contains (centre));
  EXPECT_FALSE (list.contains (centre.translated (0.f, box.getHeight () * 0.12f)));
}

// It scales with its box: no pixel sizes.
TEST (KnobGlyph, ItScalesWithItsBox)
{
  for (auto glyph : { KnobGlyph::UpDown, KnobGlyph::Folder, KnobGlyph::File })
    {
      auto const small = knobGlyphPath (glyph, box).getBounds ();
      auto const big
          = knobGlyphPath (glyph, box.withSizeKeepingCentre (80.f, 80.f))
                .getBounds ();
      EXPECT_GT (small.getHeight (), 0.f);
      EXPECT_NEAR (big.getHeight (), 2.f * small.getHeight (), 0.5f)
          << static_cast<int> (glyph);
    }
}
