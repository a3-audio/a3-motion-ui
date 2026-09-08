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

#include <a3-motion-ui/theme/SkinGroups.hh>
#include <a3-motion-ui/theme/Theme.hh>

#include <a3-motion-ui/components/SpeakerLightScaling.hh>

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

/** Whether this object carries all three colour channels as numbers. */
bool
holdsAColour (juce::DynamicObject const &object)
{
  for (auto const *channel : { "r", "g", "b" })
    if (!object.hasProperty (channel)
        || !isNumber (object.getProperty (channel)))
      return false;

  return true;
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
      auto const isColour = holdsAColour (*object);
      if (isColour)
        {
          // One row for the three channels; the picker behind it is what
          // three rows of 0..255 were a poor stand-in for.
          auto parameter = SkinParameter{ prefix.dropLastCharacters (1) };
          parameter.isColour = true;
          into.push_back (parameter);
        }

      for (auto const &property : object->getProperties ())
        {
          auto const name = property.name.toString ();
          if (isColour && (name == "r" || name == "g" || name == "b"))
            continue; // part of the colour above

          collect (property.value, prefix + name + ".", into);
        }
      return;
    }

  // The trailing separator the recursion carries is not part of a name.
  auto const path = prefix.dropLastCharacters (1);

  if (isNumber (value))
    into.push_back ({ path, !value.isDouble (), false });
  else if (value.isString ())
    into.push_back ({ path, false, true });
}

/** The var at `path`'s parent, and the last step of the path.
 *
 *  `create`, used only by the write path, adds a missing object step instead
 *  of failing there: a role merged in from the theme's defaults (see
 *  `themeDefaultsVar()`) has no parent object in the file at all until its
 *  first edit, and the read side must not invent one just by being asked.
 *  Never creates an array element either way — an index the file does not
 *  have is a bug to see, not a gap to paper over. */
juce::var *
locate (juce::var &skin, juce::String const &path, juce::String &leaf,
       bool create = false)
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
      if (object == nullptr)
        return nullptr;

      if (!object->hasProperty (steps[i]))
        {
          if (!create)
            return nullptr;
          object->setProperty (steps[i],
                               juce::var (new juce::DynamicObject ()));
        }

      current = object->getProperties ().getVarPointer (steps[i]);
      if (current == nullptr)
        return nullptr;
    }

  return current;
}

}

std::vector<SkinParameter>
skinParameters (juce::var const &skin, bool includeThemeDefaults)
{
  // The file first, the theme's defaults behind it: a value the file states
  // is the file's, a role it leaves out is still offered rather than hidden.
  //
  // withKeysReplaced replaces exactly the keys it is given, so the file's own
  // top-level names are what it is given -- that is the list of "the file has
  // an opinion here". A caller editing a slice of config.json rather than an
  // actual skin (the Network page, Button LEDs) opts out: those keys are not
  // theme roles, and the merge would bury the page's own few fields under
  // every colour and metric the theme owns.
  juce::var complete = skin;

  // Every top-level key the merge filled in rather than the file: not just
  // which rows to offer, but which of them can carry the theme's own default
  // below -- see the loop after collect(). A key the file states, even
  // partially (an "accent" with only "r"), is not one of these: the file had
  // an opinion, so nothing here is fabricated for it.
  juce::StringArray defaultedTopLevelKeys;
  juce::var defaults;

  if (includeThemeDefaults)
    {
      juce::StringArray stated;
      if (auto const *object = skin.getDynamicObject ())
        for (auto const &property : object->getProperties ())
          stated.add (property.name.toString ());

      defaults = themeDefaultsVar ();
      complete = withKeysReplaced (defaults, skin, stated);

      if (auto const *defaultsObject = defaults.getDynamicObject ())
        for (auto const &property : defaultsObject->getProperties ())
          {
            auto const name = property.name.toString ();
            if (!stated.contains (name))
              defaultedTopLevelKeys.add (name);
          }
    }

  std::vector<SkinParameter> found;
  collect (complete, {}, found);

  for (auto &parameter : found)
    {
      parameter.group = skinGroupFor (parameter.path);

      // Carry the value this path falls back to while the file does not
      // state it, so the editor can show and step from the value the app is
      // actually drawing with, rather than the zero an absent key would
      // otherwise read as. themeDefaultsVar() only ever nests one level deep
      // (a colour's r, g and b), so the top-level segment of a defaulted
      // leaf's path is always the key that decides this, for a plain number
      // and for a colour's own row alike.
      auto const topLevel
          = parameter.path.upToFirstOccurrenceOf (".", false, false);
      if (defaultedTopLevelKeys.contains (topLevel))
        {
          parameter.hasDefault = true;
          parameter.defaultValue
              = defaults.getProperty (juce::Identifier (topLevel), {});
        }
    }

  std::sort (found.begin (), found.end (), [] (auto const &a, auto const &b) {
    auto const ga = skinGroupOrder (a.group);
    auto const gb = skinGroupOrder (b.group);
    if (ga != gb)
      return ga < gb;
    return a.path < b.path;
  });

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

bool
skinHasValue (juce::var const &skin, juce::String const &path)
{
  auto copy = skin; // locate needs a non-const handle; nothing is written
  juce::String leaf;
  auto *parent = locate (copy, path, leaf);
  if (parent == nullptr)
    return false;

  if (auto *array = parent->getArray ())
    {
      auto const index = leaf.getIntValue ();
      return index >= 0 && index < array->size ();
    }

  auto *object = parent->getDynamicObject ();
  return object != nullptr && object->hasProperty (leaf);
}

void
setSkinValue (juce::var &skin, juce::String const &path, double value,
              bool asWholeNumber)
{
  juce::String leaf;
  auto *parent = locate (skin, path, leaf, true); // create a missing parent
  if (parent == nullptr)
    return;

  // Rounded on the way in: stepping compounds, and a double carries every
  // rounding of the way, so an encoder run would otherwise leave something
  // like 2.98150695788495 in a file people still read and diff.
  //
  // Two places. Four was still more than anybody would type, and nothing is
  // lost: the smallest number any shipped skin carries is 0.01, and
  // skinValueStep never returns less than that — so no value can round back
  // onto itself and become impossible to turn.
  constexpr double places = 100.0;

  auto const stored
      = asWholeNumber
            ? juce::var (static_cast<int> (std::lround (value)))
            : juce::var (std::round (value * places) / places);

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

juce::String
skinText (juce::var const &skin, juce::String const &path)
{
  auto copy = skin;
  juce::String leaf;
  auto *parent = locate (copy, path, leaf);
  if (parent == nullptr)
    return {};

  if (auto *array = parent->getArray ())
    {
      auto const index = leaf.getIntValue ();
      return index >= 0 && index < array->size () ? (*array)[index].toString ()
                                                  : juce::String{};
    }

  auto *object = parent->getDynamicObject ();
  return object != nullptr && object->hasProperty (leaf)
             ? object->getProperty (leaf).toString ()
             : juce::String{};
}

void
setSkinText (juce::var &skin, juce::String const &path,
             juce::String const &text)
{
  juce::String leaf;
  auto *parent = locate (skin, path, leaf);
  if (parent == nullptr)
    return;

  if (auto *array = parent->getArray ())
    {
      auto const index = leaf.getIntValue ();
      if (index >= 0 && index < array->size ())
        array->getReference (index) = text;
      return;
    }

  if (auto *object = parent->getDynamicObject ())
    object->setProperty (leaf, text);
}

bool
isColourChannelPath (juce::String const &path)
{
  auto const leaf = path.fromLastOccurrenceOf (".", false, false);
  auto const name = leaf.isNotEmpty () ? leaf : path;

  return name == "r" || name == "g" || name == "b";
}

juce::var
withKeysReplaced (juce::var const &document, juce::var const &edited,
                  juce::StringArray const &keys)
{
  auto *source = document.getDynamicObject ();
  if (source == nullptr)
    return document;

  // A copy, not the original: the caller may still be holding the document it
  // handed in, and a config page that is only being previewed must not have
  // already changed what is on disk.
  auto *merged = new juce::DynamicObject ();
  for (auto const &property : source->getProperties ())
    merged->setProperty (property.name, property.value);

  for (auto const &key : keys)
    {
      auto const identifier = juce::Identifier (key);
      if (edited.hasProperty (identifier))
        merged->setProperty (identifier, edited[identifier]);
    }

  return juce::var (merged);
}

double
clampSkinValue (juce::var const &skin, juce::String const &path, double value)
{
  // Derived from each value's own default, so the ranges keep their
  // proportions if a default ever moves: roughly half up to about 1.75x.
  if (path == "fontBody")
    return juce::jlimit (7.8, 26.0, value);

  if (path == "fontHeader")
    return juce::jlimit (9.4, 31.2, value);

  if (path == "potSize")
    return juce::jlimit (0.5, 2.0, value);

  if (path == "sphereScale")
    {
      // Not a fixed ceiling: the sphere may grow until the speaker icons run
      // off the screen, and that point moves with speakerRadius. Walking down
      // from the request keeps the answer honest even if the geometry changes,
      // without this function having to restate it.
      auto const speakerRadius = static_cast<float> (
          skinValue (skin, "speakerLight.speakerRadius"));
      if (speakerRadius <= 0.f)
        return juce::jmax (0.3, value);

      auto capped = juce::jmax (0.3, value);
      while (capped > 0.3
             && !speakerIconsFitOnScreen (static_cast<float> (capped),
                                          speakerRadius))
        capped -= 0.01;

      return capped;
    }

  // Derived from each role's own default, like the font sizes above: roughly
  // half up to about 1.75x. A table rather than a chain of ifs because the
  // roles are a scale — reading them in one block is how a wrong ceiling
  // shows itself.
  struct Range
  {
    char const *path;
    double min, max;
  };

  static constexpr Range metricRanges[] = {
    { "radiusTick", 1.0, 3.5 },   { "radiusControl", 1.5, 5.25 },
    { "radiusRow", 2.5, 8.75 },   { "radiusCard", 4.0, 14.0 },
    { "radiusPanel", 5.0, 17.5 }, { "paddingHair", 0.5, 1.75 },
    { "paddingTight", 1.0, 3.5 },  { "paddingSmall", 2.0, 7.0 },
    { "padding", 4.0, 14.0 },   { "strokeThin", 0.5, 1.75 },
    { "strokeMedium", 0.75, 2.6 }, { "strokeThick", 1.0, 3.5 },
  };

  for (auto const &range : metricRanges)
    if (path == range.path)
      return juce::jlimit (range.min, range.max, value);

  // An alpha outside 0..1 is not a dimmer setting, it is a value juce will
  // clamp silently later — better to say so at the knob.
  for (auto const *alpha :
       { "alphaFill", "alphaOutline", "alphaGuide", "alphaFillEmphasis",
         "alphaMuted", "alphaSecondary", "alphaTextStrong", "alphaActive",
         "alphaDisabled", "alphaInactive" })
    if (path == alpha)
      return juce::jlimit (0.0, 1.0, value);

  return value;
}

}
