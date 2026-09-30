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

/** A font height that fits: at most `cap`, and never below 1.
 *
 *  Nine call sites wrote this pairing out with their own two numbers. The
 *  numbers stay theirs -- what a label may cost is a property of that label --
 *  but the arithmetic around them is one thing and now says so.
 *
 *  The floor is not decoration: juce throws on a font of zero, and a box can be
 *  empty for a frame while a layout settles. */
float fittedFontHeight (float wanted, float cap);

/** The height at which `text` in `font` fits `width` -- the font's own
 *  height while it fits, smaller once it would not. Never below 1.
 *
 *  For a label whose height is fitted to its box but whose width is not:
 *  juce's drawText cuts a word that is too wide to "...", which reads as
 *  nothing at all. */
float heightToFitWidth (juce::Font const &font, juce::String const &text,
                        float width);

}
