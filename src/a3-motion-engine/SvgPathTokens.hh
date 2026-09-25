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

#include <cstring>

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
 *  Every command letter is a token of its own, apart from whatever it runs
 *  into on either side. None of them can appear inside a number: an
 *  exponent's 'e' is not a command.
 *
 *  One pass over the text. A long recorded take is a quarter of a megabyte of
 *  path data, and this used to build a String for every character and then
 *  take the empty tokens out one at a time, shifting the whole array each
 *  time -- a second and a half per take, on the message thread, every time its
 *  pad icon was redrawn. The touch screen answered twenty seconds late
 *  (2026-09-25). */
inline juce::StringArray
svgPathTokens (juce::String const &pathData)
{
  auto const text = pathData.toStdString ();
  auto const isCommand = [] (char c) {
    return std::strchr ("MmLlCcQqZzHhVvAaSsTt", c) != nullptr && c != '\0';
  };
  auto const isSeparator = [] (char c) {
    return c == ' ' || c == ',' || c == '\t' || c == '\n' || c == '\r';
  };

  juce::StringArray tokens;
  tokens.ensureStorageAllocated (static_cast<int> (text.size () / 4));

  std::size_t start = 0;
  auto const flush = [&] (std::size_t end) {
    if (end > start)
      tokens.add (juce::String (text.data () + start, end - start));
  };

  for (std::size_t i = 0; i < text.size (); ++i)
    {
      auto const c = text[i];
      if (isSeparator (c))
        {
          flush (i);
          start = i + 1;
        }
      else if (isCommand (c))
        {
          flush (i);
          tokens.add (juce::String::charToString (static_cast<juce::juce_wchar> (c)));
          start = i + 1;
        }
    }
  flush (text.size ());

  return tokens;
}
}
