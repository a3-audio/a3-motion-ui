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

#include <a3-motion-ui/theme/Theme.hh>

namespace a3
{

/** A role's colour, ready to draw with. Roles are opaque; transparency is a
 *  state (disabled, inactive) and belongs to the alpha the caller applies. */
juce::Colour toColour (ThemeColour const &colour);
juce::Colour toColour (ThemeColour const &colour, float alpha);

/** Shorthands for the two colours drawn all over the 2D components. Functions,
 *  not constants: they have to follow a skin change, and a `const juce::Colour`
 *  is fixed at static-init time — before any skin has been read. */
namespace Colours
{
juce::Colour background ();

/** The ground a row of cells sits on. Derived from the background rather than
 *  being its own role: a skin that darkens the background should carry the
 *  status bar with it, and two tokens would have to be kept in step by hand. */
juce::Colour statusBar ();
}

}
