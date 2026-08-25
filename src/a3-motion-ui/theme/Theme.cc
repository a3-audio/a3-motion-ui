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
  auto const base = [this, role] {
    switch (role)
      {
      case FontRole::Heading: return fontHeading;
      case FontRole::Label: return fontLabel;
      case FontRole::Value: return fontValue;
      case FontRole::Status: return fontStatus;
      }
    return fontLabel;
  }();

  return base * fontScale;
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

  theme.fontHeading = themeFloat (skin, "fontHeading", theme.fontHeading);
  theme.fontLabel = themeFloat (skin, "fontLabel", theme.fontLabel);
  theme.fontValue = themeFloat (skin, "fontValue", theme.fontValue);
  theme.fontStatus = themeFloat (skin, "fontStatus", theme.fontStatus);

  return theme;
}

}
