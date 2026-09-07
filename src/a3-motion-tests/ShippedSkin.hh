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

/** Contents of the skin that ships.
 *
 *  This used to load whatever `ui.skin` named, so that the guards below kept
 *  checking the file the app would actually load. That was right while the
 *  named skin *was* the shipped one. It is not any more: the default is
 *  protected now and editing it branches into a skin of the user's own, so
 *  following the name meant these tests measured somebody's working file and
 *  failed the suite the moment they dialled a value outside a guard band —
 *  which is a thing they are allowed to do.
 *
 *  The default is the right reference precisely because it cannot be written:
 *  what it holds is what every device starts from and what Reset returns to. */
inline juce::var
shippedSkin ()
{
  auto const configFile = juce::File (A3_CONFIG_JSON_PATH);
  auto const skin = skinFile (configFile.getParentDirectory (),
                              protectedSkinName);

  return juce::JSON::parse (skin.loadFileAsString ());
}

}
