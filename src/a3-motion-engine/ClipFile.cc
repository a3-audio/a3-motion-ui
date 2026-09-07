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
  if (!clip.svg.empty ())
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
  object->setProperty ("sqzX", s.squeezeX);
  object->setProperty ("sqzY", s.squeezeY);
  object->setProperty ("strX", s.squeezeXLfo);
  object->setProperty ("strY", s.squeezeYLfo);

  object->setProperty ("reach", s.reach);
  object->setProperty ("clipTop", s.clipTop);
  object->setProperty ("clipBottom", s.clipBottom);
  object->setProperty ("elevationBase", s.elevationBase);
  object->setProperty ("mirrorSouth", s.mirrorSouth);
  object->setProperty ("flat", s.flat);
  object->setProperty ("flatElevation", s.flatElevation);

  object->setProperty ("spin", s.spin);
  object->setProperty ("swell", s.reachLfo);
  object->setProperty ("sway", s.elevationLfo);
  object->setProperty ("envAttack", s.envelopeAttack);
  object->setProperty ("envDecay", s.envelopeDecay);
  object->setProperty ("envMax", s.envelopeMax);
  object->setProperty ("actMode", actModeToName (s.actMode));

  object->setProperty ("direction", playDirectionToName (s.direction));
  object->setProperty ("endAction", endActionToName (s.endAction));

  object->setProperty ("freqAttack", s.freqAttack);
  object->setProperty ("freqDecay", s.freqDecay);
  object->setProperty ("freqMax", s.freqMax);
  object->setProperty ("qAttack", s.qAttack);
  object->setProperty ("qDecay", s.qDecay);
  object->setProperty ("qMax", s.qMax);
  object->setProperty ("fadeReach", s.fadeReach);
  object->setProperty ("bridgeBias", s.bridgeBias);

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

  // A clip may name no shape: that is a settings preset, and applying it
  // leaves the slot's shape alone. What a file must have is a name -- without
  // one there is nothing to call it in a list.
  auto const svg = readString (parsed, "svg", {});
  if (readString (parsed, "name", {}).isEmpty ()
      && file.getFileNameWithoutExtension ().isEmpty ())
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
  s.squeezeX = readFloat (parsed, "sqzX", defaults.squeezeX);
  s.squeezeY = readFloat (parsed, "sqzY", defaults.squeezeY);
  s.squeezeXLfo = readInt (parsed, "strX", defaults.squeezeXLfo);
  s.squeezeYLfo = readInt (parsed, "strY", defaults.squeezeYLfo);

  s.reach = readFloat (parsed, "reach", defaults.reach);
  s.clipTop = readFloat (parsed, "clipTop", defaults.clipTop);
  s.clipBottom = readFloat (parsed, "clipBottom", defaults.clipBottom);
  s.mirrorSouth = readBool (parsed, "mirrorSouth", defaults.mirrorSouth);

  // Where the middle of the trajectory sits. A clip written before it says
  // only mirrorSouth, and what that meant was "the middle sits at the south
  // pole" -- which is a base of 1. Migrated rather than defaulted, or every
  // southern take would quietly come back northern.
  s.elevationBase = readFloat (parsed, "elevationBase",
                               s.mirrorSouth ? 1.f : defaults.elevationBase);
  s.flat = readBool (parsed, "flat", defaults.flat);
  s.flatElevation
      = readFloat (parsed, "flatElevation", defaults.flatElevation);

  s.spin = readInt (parsed, "spin", defaults.spin);
  s.reachLfo = readInt (parsed, "swell", defaults.reachLfo);
  s.elevationLfo = readInt (parsed, "sway", defaults.elevationLfo);
  s.envelopeAttack = readInt (parsed, "envAttack", defaults.envelopeAttack);
  s.envelopeDecay = readInt (parsed, "envDecay", defaults.envelopeDecay);
  s.envelopeMax = readFloat (parsed, "envMax", defaults.envelopeMax);
  s.actMode = actModeFromName (
      readString (parsed, "actMode", actModeToName (defaults.actMode)));

  s.direction = playDirectionFromName (readString (
      parsed, "direction", playDirectionToName (defaults.direction)));
  s.endAction = endActionFromName (
      readString (parsed, "endAction", endActionToName (defaults.endAction)));

  // The cutoff's envelope was written as "filter*" while there was only one
  // of them, so those names are what a clip saved before the split carries.
  // Read as the cutoff's, which is what they were: the resonance had no
  // envelope of its own to lose.
  s.freqAttack = readInt (parsed, "freqAttack",
                          readInt (parsed, "filterAttack",
                                   defaults.freqAttack));
  s.freqDecay = readInt (parsed, "freqDecay",
                         readInt (parsed, "filterDecay", defaults.freqDecay));
  s.freqMax = readFloat (parsed, "freqMax",
                         readFloat (parsed, "filterMax", defaults.freqMax));
  s.qAttack = readInt (parsed, "qAttack", defaults.qAttack);
  s.qDecay = readInt (parsed, "qDecay", defaults.qDecay);
  s.qMax = readFloat (parsed, "qMax", defaults.qMax);
  s.fadeReach = readFloat (parsed, "fadeReach", defaults.fadeReach);
  s.bridgeBias = readInt (parsed, "bridgeBias", defaults.bridgeBias);

  return clip;
}

bool
nameCharacterIsAllowed (juce::juce_wchar character)
{
  if (character < ' ' || character == 127)
    return false;

  return !juce::String::charToString (character)
              .containsAnyOf ("/\\:*?\"<>|");
}

juce::String
freeClipName (juce::File const &clipDir, juce::String const &base,
              juce::String const &extension)
{
  if (!clipDir.getChildFile (base + extension).existsAsFile ())
    return base;

  // Counting rather than a timestamp or a random tail: "Wave 2" is a name
  // somebody can say out loud and find again in a list.
  for (int n = 2; n < 1000; ++n)
    {
      auto const candidate = base + " " + juce::String (n);
      if (!clipDir.getChildFile (candidate + extension).existsAsFile ())
        return candidate;
    }

  return base + " " + juce::String (juce::Time::currentTimeMillis ());
}

bool
saveClipSettings (Pattern const &pattern, juce::File const &clipFile)
{
  auto clip = ClipFile::load (clipFile);
  if (!clip)
    return false;

  // Only the settings. The name, the shape and the former names are the
  // clip's identity, not its values -- see the header.
  clip->settings = clipSettingsFrom (pattern);

  return ClipFile::save (*clip, clipFile);
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
