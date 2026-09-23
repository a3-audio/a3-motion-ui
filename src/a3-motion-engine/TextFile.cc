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

#include "TextFile.hh"

namespace a3
{

namespace
{

/** A copy of `json` with every object's keys in order, recursively. */
juce::var
sorted (juce::var const &json)
{
  if (auto *array = json.getArray ())
    {
      juce::Array<juce::var> out;
      for (auto const &item : *array)
        out.add (sorted (item));
      return out;
    }

  auto *object = json.getDynamicObject ();
  if (object == nullptr)
    return json;

  juce::StringArray names;
  for (auto const &property : object->getProperties ())
    names.add (property.name.toString ());
  names.sort (false);

  auto *out = new juce::DynamicObject ();
  for (auto const &name : names)
    out->setProperty (name, sorted (object->getProperty (name)));

  return juce::var (out);
}

}


bool
writeTextFile (juce::File const &file, juce::String const &text)
{
  // The three arguments before it are JUCE's own defaults, spelled out
  // because the fourth is the point: without it this writes "\r\n".
  return file.replaceWithText (text, false, false, "\n");
}

bool
writeJsonFile (juce::File const &file, juce::var const &json)
{
  return writeTextFile (file, juce::JSON::toString (sorted (json)) + "\n");
}

juce::var
shortFloat (float value)
{
  // The shortest decimal that still reads back as this float. Nine digits
  // always suffice for a float32, and most values need three.
  for (int digits = 1; digits < 9; ++digits)
    {
      auto const text = juce::String (static_cast<double> (value), digits, false);
      if (static_cast<float> (text.getDoubleValue ()) == value)
        return text.getDoubleValue ();
    }

  return static_cast<double> (value);
}

}
