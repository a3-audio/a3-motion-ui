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

#include <juce_core/juce_core.h>

namespace a3
{

/** Split SVG path data into its commands and numbers.
 *
 *  Splitting on whitespace alone is not enough, and the same bug was written
 *  twice because of it: a closed subpath ends in Z and the next one begins
 *  with M, and written straight after each other they come out as one token,
 *  "ZM", which is neither command. Both readers dropped it, and with it the
 *  second subpath's starting point -- the two runs were joined into one and a
 *  straight line drawn across the gap between them.
 *
 *  Every command letter is separated from whatever it runs into first. None of
 *  them can appear inside a number: an exponent's 'e' is not a command. */
inline juce::StringArray
svgPathTokens (juce::String const &pathData)
{
  juce::String separated;
  for (auto const character : pathData)
    {
      if (juce::String ("MmLlCcQqZzHhVvAaSsTt").containsChar (character))
        separated << ' ';
      separated << character;
    }

  auto tokens = juce::StringArray::fromTokens (separated, " ,\t\n\r", "");
  tokens.removeEmptyStrings ();
  return tokens;
}

}
