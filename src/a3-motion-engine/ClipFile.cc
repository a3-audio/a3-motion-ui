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

#include "ClipFile.hh"

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/Playhead.hh>

namespace a3
{

namespace
{
/** Read a value, or leave the default standing. A clip file may name only
 *  what it wants to say — that is what lets a new setting be added without
 *  invalidating every file already written. */
int
readInt (juce::var const &object, char const *key, int fallback)
{
  auto const value = object.getProperty (key, juce::var ());
  return value.isVoid () ? fallback : static_cast<int> (value);
}

float
readFloat (juce::var const &object, char const *key, float fallback)
{
  auto const value = object.getProperty (key, juce::var ());
  return value.isVoid () ? fallback : static_cast<float> (value);
}

bool
readBool (juce::var const &object, char const *key, bool fallback)
{
  auto const value = object.getProperty (key, juce::var ());
  return value.isVoid () ? fallback : static_cast<bool> (value);
}

juce::String
readString (juce::var const &object, char const *key,
            juce::String const &fallback)
{
  auto const value = object.getProperty (key, juce::var ());
  return value.isVoid () ? fallback : value.toString ();
}
}

bool
ClipFile::save (Clip const &clip, juce::File const &file)
{
  auto *object = new juce::DynamicObject ();

  object->setProperty ("name", juce::String (clip.name));
  object->setProperty ("svg", juce::String (clip.svg));

  if (!clip.aka.empty ())
    {
      juce::Array<juce::var> aka;
      for (auto const &name : clip.aka)
        aka.add (juce::String (name));
      object->setProperty ("aka", aka);
    }

  auto const &s = clip.settings;
  object->setProperty ("speed", s.speedLog2);
  object->setProperty ("rotate", s.rotate);

  object->setProperty ("reach", s.reach);
  object->setProperty ("clipTop", s.clipTop);
  object->setProperty ("clipBottom", s.clipBottom);
  object->setProperty ("mirrorSouth", s.mirrorSouth);
  object->setProperty ("flat", s.flat);
  object->setProperty ("flatElevation", s.flatElevation);

  object->setProperty ("spin", s.spin);
  object->setProperty ("swell", s.reachLfo);
  object->setProperty ("envAttack", s.envelopeAttack);
  object->setProperty ("envDecay", s.envelopeDecay);
  object->setProperty ("envMax", s.envelopeMax);
  object->setProperty ("actMode", actModeToName (s.actMode));

  object->setProperty ("direction", playDirectionToName (s.direction));
  object->setProperty ("endAction", endActionToName (s.endAction));

  object->setProperty ("fade", s.fadeSixteenths);

  if (!file.getParentDirectory ().createDirectory ())
    return false;

  return file.replaceWithText (juce::JSON::toString (juce::var (object)));
}

std::optional<Clip>
ClipFile::load (juce::File const &file)
{
  if (!file.existsAsFile ())
    return {};

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  if (!parsed.isObject ())
    return {};

  // A clip that names no shape plays nothing. Better to say the file is not a
  // clip than to hand back one that points nowhere.
  auto const svg = readString (parsed, "svg", {});
  if (svg.isEmpty ())
    return {};

  Clip clip;
  clip.name = readString (parsed, "name", file.getFileNameWithoutExtension ())
                  .toStdString ();
  clip.svg = svg.toStdString ();

  if (auto const *aka = parsed.getProperty ("aka", {}).getArray ())
    for (auto const &name : *aka)
      clip.aka.push_back (name.toString ().toStdString ());

  auto &s = clip.settings;
  ClipSettings const defaults;

  s.speedLog2 = readInt (parsed, "speed", defaults.speedLog2);
  s.rotate = readFloat (parsed, "rotate", defaults.rotate);

  s.reach = readFloat (parsed, "reach", defaults.reach);
  s.clipTop = readFloat (parsed, "clipTop", defaults.clipTop);
  s.clipBottom = readFloat (parsed, "clipBottom", defaults.clipBottom);
  s.mirrorSouth = readBool (parsed, "mirrorSouth", defaults.mirrorSouth);
  s.flat = readBool (parsed, "flat", defaults.flat);
  s.flatElevation
      = readFloat (parsed, "flatElevation", defaults.flatElevation);

  s.spin = readInt (parsed, "spin", defaults.spin);
  s.reachLfo = readInt (parsed, "swell", defaults.reachLfo);
  s.envelopeAttack = readInt (parsed, "envAttack", defaults.envelopeAttack);
  s.envelopeDecay = readInt (parsed, "envDecay", defaults.envelopeDecay);
  s.envelopeMax = readFloat (parsed, "envMax", defaults.envelopeMax);
  s.actMode = actModeFromName (
      readString (parsed, "actMode", actModeToName (defaults.actMode)));

  s.direction = playDirectionFromName (readString (
      parsed, "direction", playDirectionToName (defaults.direction)));
  s.endAction = endActionFromName (
      readString (parsed, "endAction", endActionToName (defaults.endAction)));

  s.fadeSixteenths = readInt (parsed, "fade", defaults.fadeSixteenths);

  return clip;
}

bool
clipHasDrifted (Pattern const &pattern, juce::File const &clipFile)
{
  auto const clip = ClipFile::load (clipFile);
  if (!clip)
    return false;

  return clipSettingsFrom (pattern) != clip->settings;
}

}
