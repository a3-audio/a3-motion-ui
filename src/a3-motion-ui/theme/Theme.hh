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

namespace a3
{

/** A colour as a skin file states it.
 *
 *  Not a juce::Colour: this header is compiled into the test runner, which
 *  links the engine alone and has no juce_graphics. The conversion happens
 *  where the drawing does. */
struct ThemeColour
{
  int r = 255, g = 255, b = 255;
};

constexpr int numThemeChannels = 4;

/** Which font a piece of text is. The skin gives each a base size; the Font
 *  Size setting in the menu scales all of them together. */
enum class FontRole
{
  Heading,
  Label,
  Value,
  Status,
};

/** Everything about how the device looks, in one place.
 *
 *  The values below are the defaults, taken from what the code and config
 *  carried before there was a theme — LookAndFeel.cc, SphereShader.cc and
 *  config.json. A skin file overlays them role by role; anything it leaves out
 *  keeps the value here, so a missing or half-written file still gives a usable
 *  picture rather than a black one. */
struct Theme
{
  // Surfaces
  ThemeColour surface{ 0, 0, 0 };
  ThemeColour surfaceRaised{ 20, 23, 27 };
  ThemeColour background{ 41, 47, 54 }; // was 0xff292f36 in LookAndFeel.cc

  // Text
  ThemeColour textPrimary{ 255, 255, 255 };
  ThemeColour textMuted{ 211, 211, 211 };
  ThemeColour textOnAccent{ 0, 0, 0 };

  // State
  ThemeColour accent{ 144, 238, 144 }; // was Colours::lightgreen
  ThemeColour warning{ 255, 165, 0 };
  ThemeColour danger{ 255, 0, 0 };
  float alphaDisabled = 0.35f;
  float alphaInactive = 0.6f;

  // Channels
  ThemeColour channel[numThemeChannels]
      = { { 216, 17, 89 }, { 69, 78, 158 }, { 247, 208, 2 }, { 33, 131, 128 } };

  // Sphere and shader
  ThemeColour sphereSurface{ 10, 10, 14 };      // was vec3 (0.04, 0.04, 0.055)
  ThemeColour sphereRim{ 128, 140, 166 };       // was vec3 (0.5, 0.55, 0.65)
  ThemeColour sphereEnvironment{ 5, 6, 8 };     // was vec3 (0.02, 0.025, 0.03)
  ThemeColour boltCore{ 255, 255, 255 };
  ThemeColour sphereGlow{ 70, 130, 250 };
  ThemeColour speakerLight{ 70, 130, 250 };
  ThemeColour energy{ 150, 220, 255 };

  // Sizes, as a share of the component's shorter side
  float sphereScale = 0.62f;
  float blobScale = 0.05f;
  float strokeThin = 1.f;
  float strokeThick = 2.f;

  // Font base sizes, before the menu's factor
  float fontHeading = 22.f;
  float fontLabel = 15.f;
  float fontValue = 18.f;
  float fontStatus = 16.f;

  /** Set by the Font Size setting in the menu. Multiplies every base size, so
   *  no component can be forgotten when it changes. */
  float fontScale = 1.f;

  float fontSize (FontRole role) const;
};

/** One colour out of a skin, falling back when it is absent or incomplete.
 *
 *  All three channels or none: a missing "b" would read as 0 and quietly
 *  darken the role, which looks like a rendering fault rather than a gap in
 *  the file. */
ThemeColour themeColour (juce::var const &skin, juce::String const &name,
                         ThemeColour fallback);

/** A theme built from a skin file's contents. Pass a void var for the
 *  built-in defaults. */
Theme loadTheme (juce::var const &skin);

/** Where a named skin lives: `<configDir>/skins/<name>.json`. An empty name
 *  gives the default skin rather than a path that cannot exist. */
juce::File skinFile (juce::File const &configDir, juce::String const &name);

/** The factors the Font Size menu offers, and the one a saved index means.
 *
 *  Lives with the theme rather than with the menu: the menu is one of two
 *  callers, and a saved index has to be turned into a factor at startup, before
 *  any menu exists. An index out of range gives 1 — it arrives from a file that
 *  can be older than this table, or hand-edited. */
constexpr int numFontScales = 5;
float fontScaleForIndex (int index);

/** Set only the menu's factor, leaving the skin's own values untouched. The
 *  two arrive from different files at different times, and neither may throw
 *  the other away. */
void setFontScale (float scale);

/** The theme in force. Written only while loading, on the message thread; the
 *  GL thread copies what it needs per frame, as it already does with the
 *  config. Follows the `extern juce::var userConfig` pattern this repo
 *  already uses rather than introducing a second convention. */
Theme const &theme ();
void setTheme (Theme newTheme);

/** Contents of the skin `ui.skin` names, beside the given config file. A
 *  missing file gives a void var, which loadTheme turns into the built-in
 *  defaults. */
juce::var loadActiveSkinVar (juce::File const &configFile,
                             juce::var const &config);

}
