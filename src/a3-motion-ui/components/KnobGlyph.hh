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

#pragma once

#include <juce_graphics/juce_graphics.h>

namespace a3
{

/** The signs beside the KEYS knobs (2026-10-16): what an encoder does, drawn
 *  rather than written, because a word does not fit beside a 5 mm knob. */
enum class KnobGlyph
{
  None,
  /** A text caret, an I-beam, with a small triangle either side. */
  Cursor,
  /** A scroll bar standing up: a groove with a triangle at each end. */
  ScrollVertical,
  /** The same lying down. */
  ScrollHorizontal,
};

/** The glyph as one path to fill, inside `box` and centred in it. The
 *  triangles are the keyboard's own arrow; the strokes are already outlined,
 *  so the caller fills the whole thing in one colour. Empty for None. */
juce::Path knobGlyphPath (KnobGlyph glyph, juce::Rectangle<float> box);

}
