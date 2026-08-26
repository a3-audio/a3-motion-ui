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

#include "SkinParameters.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{

bool
isNumber (juce::var const &value)
{
  return value.isDouble () || value.isInt () || value.isInt64 ();
}

void
collect (juce::var const &value, juce::String const &prefix,
         std::vector<SkinParameter> &into)
{
  if (auto const *array = value.getArray ())
    {
      for (int i = 0; i < array->size (); ++i)
        collect ((*array)[i], prefix + juce::String (i) + ".", into);
      return;
    }

  if (auto const *object = value.getDynamicObject ())
    {
      for (auto const &property : object->getProperties ())
        collect (property.value, prefix + property.name.toString () + ".",
                 into);
      return;
    }

  if (!isNumber (value))
    return;

  // The trailing separator the recursion carries is not part of a name.
  auto const path = prefix.dropLastCharacters (1);
  into.push_back ({ path, !value.isDouble () });
}

/** The var at `path`'s parent, and the last step of the path. */
juce::var *
locate (juce::var &skin, juce::String const &path, juce::String &leaf)
{
  juce::StringArray steps;
  steps.addTokens (path, ".", "");
  if (steps.isEmpty ())
    return nullptr;

  leaf = steps[steps.size () - 1];

  auto *current = &skin;
  for (int i = 0; i < steps.size () - 1; ++i)
    {
      if (auto *array = current->getArray ())
        {
          auto const index = steps[i].getIntValue ();
          if (index < 0 || index >= array->size ())
            return nullptr;
          current = &array->getReference (index);
          continue;
        }

      auto *object = current->getDynamicObject ();
      if (object == nullptr || !object->hasProperty (steps[i]))
        return nullptr;
      current = object->getProperties ().getVarPointer (steps[i]);
      if (current == nullptr)
        return nullptr;
    }

  return current;
}

}

std::vector<SkinParameter>
skinParameters (juce::var const &skin)
{
  std::vector<SkinParameter> found;
  collect (skin, {}, found);

  std::sort (found.begin (), found.end (),
             [] (auto const &a, auto const &b) { return a.path < b.path; });

  return found;
}

double
skinValue (juce::var const &skin, juce::String const &path)
{
  auto copy = skin; // locate needs a non-const handle; nothing is written
  juce::String leaf;
  auto *parent = locate (copy, path, leaf);
  if (parent == nullptr)
    return 0.0;

  if (auto *array = parent->getArray ())
    {
      auto const index = leaf.getIntValue ();
      return index >= 0 && index < array->size ()
                 ? static_cast<double> ((*array)[index])
                 : 0.0;
    }

  auto *object = parent->getDynamicObject ();
  return object != nullptr && object->hasProperty (leaf)
             ? static_cast<double> (object->getProperty (leaf))
             : 0.0;
}

void
setSkinValue (juce::var &skin, juce::String const &path, double value,
              bool asWholeNumber)
{
  juce::String leaf;
  auto *parent = locate (skin, path, leaf);
  if (parent == nullptr)
    return;

  auto const stored
      = asWholeNumber
            ? juce::var (static_cast<int> (std::lround (value)))
            : juce::var (value);

  if (auto *array = parent->getArray ())
    {
      auto const index = leaf.getIntValue ();
      if (index >= 0 && index < array->size ())
        array->getReference (index) = stored;
      return;
    }

  if (auto *object = parent->getDynamicObject ())
    object->setProperty (leaf, stored);
}

double
skinValueStep (double value, bool isWholeNumber)
{
  if (isWholeNumber)
    return 1.0;

  constexpr double share = 0.1;
  constexpr double smallest = 0.01;

  return std::max (smallest, std::abs (value) * share);
}

double
stepSkinValue (double value, int detents, bool isWholeNumber,
               bool isColourChannel)
{
  auto const moved
      = value + detents * skinValueStep (value, isWholeNumber);

  if (isColourChannel)
    return juce::jlimit (0.0, 255.0, moved);

  return moved;
}

bool
isColourChannelPath (juce::String const &path)
{
  auto const leaf = path.fromLastOccurrenceOf (".", false, false);
  auto const name = leaf.isNotEmpty () ? leaf : path;

  return name == "r" || name == "g" || name == "b";
}

}
