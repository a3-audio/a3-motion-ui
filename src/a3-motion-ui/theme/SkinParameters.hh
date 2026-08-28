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
  juce::String path;          //< "accent.r", "channels.2.b", "oscSender.host"
  bool isWholeNumber = false; //< how the file writes it, and how it steps
  bool isText = false;        //< typed on the keyboard rather than turned
  /** An object carrying r, g and b: one row, and a picker behind it. The
   *  path names the object, so its channels are path + ".r" and so on. */
  bool isColour = false;
};

/** Every editable leaf, by path, sorted so the list does not reshuffle
 *  between sessions. Numbers and text; anything else is structure, and
 *  there is no control on this panel that could edit structure. */
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

/** The text at `path`, and how to put it back. */
juce::String skinText (juce::var const &skin, juce::String const &path);
void setSkinText (juce::var &skin, juce::String const &path,
                  juce::String const &text);

/** `value` held inside whatever range its path has one.
 *
 *  The values that used to be menu rows — the two font sizes, the pot size,
 *  the sphere's scale — offered named steps there and could not fall out of
 *  their range. In the editor every value is a free number, so the guard rails
 *  had to come along; they were lost in the move, not dropped on purpose.
 *
 *  Takes the whole skin because one of the ranges is not a number but a
 *  consequence: the sphere may grow until the speaker icons would be clipped,
 *  and where that is depends on speakerRadius, which is itself a skin value.
 *
 *  A path with no range is returned untouched — a tuning number nobody has
 *  reasoned about must not silently acquire one. */
double clampSkinValue (juce::var const &skin, juce::String const &path,
                       double value);

/** Whether a path names one of a colour's three channels. */
bool isColourChannelPath (juce::String const &path);

/** `document` with `keys` replaced by whatever `edited` holds for them.
 *
 *  A config page edits a slice of config.json — the keys it was opened with —
 *  and that slice has to go two places: to disk when the page is left, and
 *  into the running configuration straight away, so a colour being picked is
 *  visible while it is being picked. One function for both, so they cannot
 *  drift apart. Keys `edited` does not carry are left as they were. */
juce::var withKeysReplaced (juce::var const &document, juce::var const &edited,
                            juce::StringArray const &keys);

}
