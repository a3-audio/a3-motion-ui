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

#include "Theme.hh"

#include <regex>

namespace a3
{

namespace
{

ThemeColour
colourFromEntry (juce::var const &entry, ThemeColour fallback)
{
  if (!entry.hasProperty ("r") || !entry.hasProperty ("g")
      || !entry.hasProperty ("b"))
    return fallback;

  auto const channel = [&entry] (char const *key) {
    return juce::jlimit (0, 255,
                         static_cast<int> (entry[juce::Identifier (key)]));
  };

  return { channel ("r"), channel ("g"), channel ("b") };
}

float
themeFloat (juce::var const &skin, char const *name, float fallback)
{
  auto const key = juce::Identifier (name);

  return skin.hasProperty (key) ? static_cast<float> (skin[key]) : fallback;
}

}

float
Theme::fontSize (FontRole role) const
{
  auto const base = role == FontRole::Header ? fontHeader : fontBody;

  return base * scaleFor (role);
}

float
Theme::scaleFor (FontRole role) const
{
  return role == FontRole::Header ? headerScale : bodyScale;
}

ThemeColour
themeColour (juce::var const &skin, juce::String const &name,
             ThemeColour fallback)
{
  auto const key = juce::Identifier (name);
  if (!skin.hasProperty (key))
    return fallback;

  return colourFromEntry (skin[key], fallback);
}

juce::File
skinFile (juce::File const &configDir, juce::String const &name)
{
  auto const chosen = name.isNotEmpty () ? name : juce::String ("default");

  return configDir.getChildFile ("skins").getChildFile (chosen + ".json");
}

namespace
{
Theme &
mutableTheme ()
{
  static Theme instance;
  return instance;
}
}

float
fontScaleForIndex (int index)
{
  constexpr float scales[numFontScales] = { 0.75f, 1.f, 1.25f, 1.5f, 1.75f };

  if (index < 0 || index >= numFontScales)
    return 1.f;

  return scales[index];
}

namespace
{
float
usableScale (float scale)
{
  return juce::jlimit (0.25f, 4.f, scale);
}
}

void
setHeaderScale (float scale)
{
  mutableTheme ().headerScale = usableScale (scale);
}

void
setBodyScale (float scale)
{
  mutableTheme ().bodyScale = usableScale (scale);
}

Theme const &
theme ()
{
  return mutableTheme ();
}

void
setTheme (Theme newTheme)
{
  mutableTheme () = newTheme;
}

juce::var
loadActiveSkinVar (juce::File const &configFile, juce::var const &config)
{
  auto const file = skinFile (configFile.getParentDirectory (),
                              config["ui"]["skin"].toString ());

  if (!file.existsAsFile ())
    return {};

  return juce::JSON::parse (file.loadFileAsString ());
}

juce::StringArray
availableSkins (juce::File const &configDir)
{
  juce::StringArray names;

  auto const folder = configDir.getChildFile ("skins");
  if (folder.isDirectory ())
    for (auto const &entry : juce::RangedDirectoryIterator (
             folder, false, "*.json", juce::File::findFiles))
      names.add (entry.getFile ().getFileNameWithoutExtension ());

  names.sort (true);

  if (names.isEmpty ())
    names.add ("default");

  return names;
}

juce::String
activeSkinName (juce::File const &configFile)
{
  auto const config = juce::JSON::parse (configFile.loadFileAsString ());
  auto const named = config["ui"]["skin"].toString ();

  return named.isNotEmpty () ? named : juce::String ("default");
}

bool
writeActiveSkin (juce::File const &configFile, juce::String const &name)
{
  auto const text = configFile.loadFileAsString ();

  // The entry as it is written, whatever spacing surrounds the colon.
  std::regex const entry (R"(("skin"\s*:\s*")[^"]*("))");
  auto const before = text.toStdString ();
  std::string after
      = std::regex_replace (before, entry, "$1" + name.toStdString () + "$2");

  if (after == before && activeSkinName (configFile) != name)
    return false; // no entry to rewrite

  if (after == before)
    return true; // already this skin

  // Explicit "\n": replaceWithText writes CRLF by default, which would
  // rewrite every line in the file to change one word.
  return configFile.replaceWithText (juce::String (after), false, false, "\n");
}

bool
isUsableSkinName (juce::String const &name)
{
  if (name.isEmpty ())
    return false;

  return name.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789-");
}

juce::String
nextFreeSkinName (juce::File const &configDir, juce::String const &name)
{
  // A trailing "-<number>" is a counter this function put there, so a copy of
  // "neutral-2" is "neutral-3" rather than "neutral-2-2".
  auto stem = name;
  auto const tail = name.fromLastOccurrenceOf ("-", false, false);
  if (tail.isNotEmpty () && tail.containsOnly ("0123456789"))
    stem = name.upToLastOccurrenceOf ("-", false, false);

  auto const existing = availableSkins (configDir);
  for (int number = 2; number < 1000; ++number)
    {
      auto const candidate = stem + "-" + juce::String (number);
      if (!existing.contains (candidate))
        return candidate;
    }

  return stem;
}

bool
renameSkin (juce::File const &configDir, juce::String const &from,
            juce::String const &to)
{
  if (!isUsableSkinName (to) || from == to)
    return false;

  auto const source = skinFile (configDir, from);
  auto const target = skinFile (configDir, to);

  if (!source.existsAsFile () || target.existsAsFile ())
    return false;

  if (!source.moveFileTo (target))
    return false;

  auto const configFile = configDir.getChildFile ("config.json");
  if (activeSkinName (configFile) == from)
    writeActiveSkin (configFile, to);

  return true;
}

bool
deleteSkin (juce::File const &configDir, juce::String const &name)
{
  auto const remaining = availableSkins (configDir);
  if (remaining.size () < 2 || !remaining.contains (name))
    return false;

  auto const file = skinFile (configDir, name);
  if (!file.existsAsFile () || !file.deleteFile ())
    return false;

  auto const configFile = configDir.getChildFile ("config.json");
  if (activeSkinName (configFile) == name)
    {
      auto const left = availableSkins (configDir);
      writeActiveSkin (configFile, left[0]);
    }

  return true;
}

Theme
loadTheme (juce::var const &skin)
{
  Theme theme; // defaults

  auto const colour = [&skin] (char const *name, ThemeColour &target) {
    target = themeColour (skin, name, target);
  };

  colour ("surface", theme.surface);
  colour ("surfaceRaised", theme.surfaceRaised);
  colour ("background", theme.background);

  colour ("textPrimary", theme.textPrimary);
  colour ("textMuted", theme.textMuted);
  colour ("textOnAccent", theme.textOnAccent);

  colour ("accent", theme.accent);
  colour ("warning", theme.warning);
  colour ("danger", theme.danger);

  colour ("sphereSurface", theme.sphereSurface);
  colour ("sphereRim", theme.sphereRim);
  colour ("sphereEnvironment", theme.sphereEnvironment);
  colour ("boltCore", theme.boltCore);
  colour ("sphereGlow", theme.sphereGlow);
  colour ("speakerLight", theme.speakerLight);
  colour ("energy", theme.energy);

  // Channels come as an array, and their order is what ties a colour to a
  // channel — an entry out of place renames every channel after it.
  if (auto const *channels = skin["channels"].getArray ())
    {
      auto const count
          = juce::jmin (channels->size (), numThemeChannels);
      for (int i = 0; i < count; ++i)
        theme.channel[i]
            = colourFromEntry ((*channels)[i], theme.channel[i]);
    }

  theme.alphaDisabled = themeFloat (skin, "alphaDisabled", theme.alphaDisabled);
  theme.alphaInactive = themeFloat (skin, "alphaInactive", theme.alphaInactive);

  theme.sphereScale = themeFloat (skin, "sphereScale", theme.sphereScale);
  theme.blobScale = themeFloat (skin, "blobScale", theme.blobScale);
  theme.strokeThin = themeFloat (skin, "strokeThin", theme.strokeThin);
  theme.strokeThick = themeFloat (skin, "strokeThick", theme.strokeThick);

  theme.fontHeader = themeFloat (skin, "fontHeader", theme.fontHeader);
  theme.fontBody = themeFloat (skin, "fontBody", theme.fontBody);

  return theme;
}

}
