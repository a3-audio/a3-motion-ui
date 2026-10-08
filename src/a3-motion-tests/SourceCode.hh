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

#include <string>

namespace a3
{

/** The code of a source text with its comments taken out and every space
 *  and line break squeezed away -- for the few wiring checks that read the
 *  main component's source, since it cannot be built in a test. A reformat
 *  cannot turn them red, and a call left only in a comment cannot keep them
 *  green. String and character literals are kept as they are. */
inline juce::String
codeOf (juce::String const &source)
{
  std::string code;
  auto const text = source.toStdString ();
  for (std::size_t i = 0; i < text.size (); ++i)
    {
      auto const c = text[i];
      auto const next = i + 1 < text.size () ? text[i + 1] : '\0';

      if (c == '/' && next == '/')
        {
          while (i < text.size () && text[i] != '\n')
            ++i;
          continue;
        }
      if (c == '/' && next == '*')
        {
          i += 2;
          while (i + 1 < text.size () && !(text[i] == '*' && text[i + 1] == '/'))
            ++i;
          ++i;
          continue;
        }
      if (c == '"' || c == '\'')
        {
          code += c;
          for (++i; i < text.size () && text[i] != c; ++i)
            {
              code += text[i];
              if (text[i] == '\\' && i + 1 < text.size ())
                code += text[++i];
            }
          code += c;
          continue;
        }
      if (!juce::CharacterFunctions::isWhitespace (c))
        code += c;
    }
  return juce::String (code);
}

/** codeOf a file of the UI's own source, relative to src/a3-motion-ui. */
inline juce::String
uiCode (char const *relativePath)
{
  return codeOf (juce::File (A3_UI_SOURCE_DIR)
                     .getChildFile (relativePath)
                     .loadFileAsString ());
}

}
