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

#include "FunctionKeyColours.hh"

#include <a3-motion-ui/theme/ThemeColours.hh>
#include <a3-motion-ui/theme/TransportLook.hh>

namespace a3
{

juce::Colour
recModeColour (int recMode)
{
  // Read as "how much of what is already there will this pass destroy".
  // Touch mends a corner and leaves the rest; Latch holds on after the finger
  // goes; Write clears the whole pass whether you touched it or not. That is
  // a scale, so it is said on the scale the rest of the device already uses.
  switch (recMode)
    {
    case 1:
      return toColour (theme ().warning);
    case 2:
      return toColour (theme ().danger);
    default:
      return toColour (theme ().accent);
    }
}

juce::Colour
functionKeyColour (FunctionKey key, FunctionKeyLook const &look)
{
  switch (key)
    {
    case FunctionKey::ClockMode:
      // Always coloured: which clock is running is never "nothing".
      return Colours::clockMode (look.clockMode);

    case FunctionKey::RecMode:
      return recModeColour (look.recMode);

    case FunctionKey::Record:
      // Red, always. Recording writes over what you cannot get back, and the
      // key that does it should say so before it is pressed as well as after
      // — it was orange until it ran, which made the standing state a colour
      // you had to know the meaning of. Running is the same red, brighter, so
      // the difference is loudness rather than a second thing to learn.
      //
      // Taken from the transport rule rather than restated: the panel key and
      // the bar's little circle are the same function, and a function has one
      // colour.
      return look.recording
                 ? transportColour (TransportKey::Record).brighter (0.45f)
                 : transportColour (TransportKey::Record);

    case FunctionKey::Shift:
      return look.shiftHeld ? toColour (theme ().accent) : juce::Colour{};

    case FunctionKey::Tap:
      return (look.tapPressed || look.tapBeat) ? toColour (theme ().accent)
                                               : juce::Colour{};

    case FunctionKey::Menu:
      // Blue -- notice, the colour this device already uses for "worth seeing,
      // neither good news nor bad". The word says what it does; the colour
      // says where the key belongs, which is what you are looking for when you
      // are scanning the strip rather than reading it.
      //
      // Lit while the menu is open, like Shift while it is held: both are
      // states you are *in*, and a key you are inside of that looked the same
      // as one you are not is how you end up pressing it twice.
      return toColour (theme ().notice);
    }

  return {};
}

}
