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

/** Colour a function button's LED lights in when it carries a function.
 *
 *  The hardware takes a colour per LED — `writeSetLed` always did — but every
 *  function button was sent plain white, so nothing on the panel said which key
 *  did what without reading the legend.
 *
 *  `name` is the button's key in config.json's `buttonLeds` ("record", "tap",
 *  "menu", "shift"). An unconfigured button stays white rather than going dark,
 *  because a button that lights up wrongly is still usable and one that does
 *  not light up at all looks broken. */
struct LedColour
{
  int r = 255, g = 255, b = 255;

  bool
  operator== (LedColour const &other) const
  {
    return r == other.r && g == other.g && b == other.b;
  }
};

/** White, which is what every function button used to light in. */
constexpr LedColour ledColourUnassigned{ 255, 255, 255 };

LedColour buttonLedColour (juce::var const &buttonLedsConfig,
                           juce::String const &name);

/** What a function button lights in while nobody is touching it.
 *
 *  A key that does something should say so at rest; the LEDs used to light
 *  only under a finger, which tells you what you already know. White unless
 *  the config's `idle` entry says otherwise. */
LedColour buttonLedIdleColour (juce::var const &buttonLedsConfig);

/** The same colour, as an LED has to say it.
 *
 *  A screen colour is read against the surface around it: `accent` at
 *  rgb(144, 238, 144) is plainly a light green next to a dark bar. An LED has
 *  no surround — it *is* the light — so the same value arrives as white with a
 *  tint, and the maintainer reported exactly that: "the green of the hardware
 *  keys does not look green, it looks white."
 *
 *  So the hue is kept and the saturation is brought up to a floor. Brightness
 *  is left alone, because how bright a key is means something (see TAP on a
 *  beat) and this must not say it a second way. A colour with no hue at all is
 *  returned untouched: a key meant to be white should be white. */
juce::Colour ledColour (juce::Colour colour);

/** Below this an LED reads as white with a tint rather than as a colour. */
constexpr float ledMinSaturation = 0.8f;

/** An LED colour as juce states one. */
juce::Colour toColour (LedColour const &colour);

}
