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
  /** Up and down: two arrows apart, one above the other. */
  UpDown,
  /** The same lying down. */
  LeftRight,
  /** A folder with its tab: which of FILES' folders. */
  Folder,
  /** A list of lines with one marked: which file in the list. */
  File,
};

/** The glyph as one path to fill, inside `box` and centred in it. The
 *  triangles are the keyboard's own arrow; strokes come already outlined, so
 *  the caller fills the whole thing in one colour. Empty for None. */
juce::Path knobGlyphPath (KnobGlyph glyph, juce::Rectangle<float> box);

}
