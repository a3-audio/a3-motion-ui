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

/** Contents of the skin that actually ships — the one `ui.skin` names, not a
 *  hardcoded path, so these tests keep checking the file the app will load
 *  even after someone switches skins. */
inline juce::var
shippedSkin ()
{
  auto const configFile = juce::File (A3_CONFIG_JSON_PATH);
  auto const config = juce::JSON::parse (configFile.loadFileAsString ());

  return loadActiveSkinVar (configFile, config);
}

}
