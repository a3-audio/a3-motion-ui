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

#include <JuceHeader.h>

#include <vector>

namespace a3
{

/** One editable number in a skin file, named by where it sits.
 *
 *  Derived from the file rather than from a hand-written list of roles: a
 *  key added to a skin shows up in the editor without anyone remembering to
 *  register it, and a key removed stops being offered. */
struct SkinParameter
{
  juce::String path;          //< "accent.r", "channels.2.b", "sphereGlow.netGain"
  bool isWholeNumber = false; //< how the file writes it, and how it steps
};

/** Every number in the skin, by path, sorted so the list does not reshuffle
 *  between sessions. Anything that is not a number is left out — an encoder
 *  has no way to turn into a string. */
std::vector<SkinParameter> skinParameters (juce::var const &skin);

/** The number at `path`, or 0 when there is none. */
double skinValue (juce::var const &skin, juce::String const &path);

/** Put `value` at `path`, leaving its neighbours alone.
 *
 *  `asWholeNumber` decides how it is stored, and the caller is the one that
 *  knows: a value the file writes as a float has to stay one even when it
 *  lands exactly on an integer, or the next session reads it as whole and
 *  steps it in ones instead of hundredths. */
void setSkinValue (juce::var &skin, juce::String const &path, double value,
                   bool asWholeNumber = false);

/** How far one encoder detent moves a value of this size.
 *
 *  One encoder has to cover a colour channel counted in 255ths and a wrap
 *  angle counted in degrees, so the step is a share of the value rather
 *  than a constant — with a floor, or a value sitting at zero could never
 *  be raised again. */
double skinValueStep (double value, bool isWholeNumber);

/** `value` moved by `detents`, stepped and clamped. Colour channels stop at
 *  0 and 255: past that is not a brighter colour, it is a broken file. */
double stepSkinValue (double value, int detents, bool isWholeNumber,
                      bool isColourChannel = false);

/** Whether a path names one of a colour's three channels. */
bool isColourChannelPath (juce::String const &path);

}
