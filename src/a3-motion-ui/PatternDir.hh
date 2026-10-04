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

/** Where the pattern library is, from config.json's "patternDir".
 *
 *  A relative path is taken from \p base -- the working directory, which is
 *  also where config/config.json is read from -- so the shipped config works
 *  wherever the checkout sits. It named /home/aaa/a3-system/a3-motion-ui/
 *  pattern, and in the a3-system umbrella the UI is a3-motion/ui: the app
 *  found no patterns and made an empty folder at the old place instead
 *  (a3nuc2, 2026-10-04). An absolute path is taken as it is. No key, or an
 *  empty one, is "pattern".
 */
juce::File patternDirectory (juce::var const &config, juce::File const &base);

} // namespace a3
