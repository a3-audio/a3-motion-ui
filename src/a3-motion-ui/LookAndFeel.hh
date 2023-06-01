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

namespace a3
{

struct Colours
{
  static juce::Colour const background;
  static juce::Colour const circle;
};

/*
 * Our custom A3 LookAndFeel class.
 *
 * A note on the enum values for our custom ColourIds. JUCE uses
 * per-Component globally unique ColourIds, so we use the same
 * mechanism for our own Components. JUCE IDs start at 0x1000000, A3
 * IDs start at 0x03000000.
 *
 * Formatting convention of IDs is as follows: 0x03000204 identifies
 * color 4 of the A3 widget with running Component number 2. Every
 * newly introduced Component class increases the component number by
 * one. The per-Component custom colors are then numbered 00-ff in the
 * last 2 hex digits, making for 255 possible custom colors per custom
 * Component type.
 */
class LookAndFeel_A3 : public juce::LookAndFeel_V4
{
public:
  LookAndFeel_A3 ();
};

}
