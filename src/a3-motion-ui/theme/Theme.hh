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

/** Which of the two sizes on this screen a piece of text is.
 *
 *  Header is the status bar and a section's title; Body is every setting
 *  and every value under one. Two and no more: four roles said the same
 *  thing in a way nobody could set from the menu, and left the status bar
 *  larger than the headings it sits above. The skin gives each a base
 *  size, and each has its own factor in the menu. */
enum class FontRole
{
  Header,
  Body,
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
  /** The band the status bar sits on, raised above the sphere behind it.
   *  Its value is what juce's stock window background happened to be, which
   *  is where this band's colour came from until a skin could reach it. */
  ThemeColour surfaceRaised{ 50, 62, 68 };
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

  // Font sizes, absolute and straight out of the skin. They used to be base
  // sizes with a percentage from the menu on top — two sources for one size,
  // where switching skin moved one and left the other.
  float fontHeader = 18.f;
  float fontBody = 15.f;

  /** Knob and toggle size in the clip settings bar, relative to the built-in
   *  size. Part of the look, so it lives with the rest of it. */
  float potSize = 1.f;

  float fontSize (FontRole role) const;

  /** What this skin's size is, relative to the built-in one. Derived, never
   *  set: it is how the few places that scale JUCE's own fonts (LookAndFeel,
   *  PadRowDisplay) follow the skin without becoming a second source. */
  float scaleFor (FontRole role) const;
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

/** Set only one menu factor, leaving the skin's own values — and the other
 *  factor — untouched. Skin and settings arrive from different files at

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

/** The skins that exist beside the given config directory, by name and
 *  sorted, so a menu built from this reads the same on every machine.
 *
 *  A missing or empty folder still offers "default" — that is what
 *  skinFile() falls back to, and a menu with no entries would be worse
 *  than one entry that always works. */
juce::StringArray availableSkins (juce::File const &configDir);

/** Which skin the config file names, "default" when it names none. */
juce::String activeSkinName (juce::File const &configFile);

/** Point the config file at another skin, and nothing else.
 *
 *  Only the value is rewritten — config.json is hand-maintained, and its
 *  ordering, spacing and comments are how a person reads it; a parse and
 *  re-serialise would reformat the whole file to say one word. Returns
 *  false, and leaves the file untouched, when there is no `ui.skin` entry
 *  to rewrite: guessing where one should go would write a file the next
 *  hand edit fights with. */
bool writeActiveSkin (juce::File const &configFile, juce::String const &name);

/** Whether a name may be a skin's.
 *
 *  Lowercase letters, digits and dashes. A name is a file name and a key in
 *  config.json, so what can be dialled in on an encoder has to be narrower
 *  than what might be dialled in by accident. */
bool isUsableSkinName (juce::String const &name);

/** The name a copy of `name` should take: the same name with the next free
 *  number. A name that already ends in one counts on rather than nesting —
 *  the number is a counter, not part of the name. */
juce::String nextFreeSkinName (juce::File const &configDir,
                               juce::String const &name);

/** Move a skin to another name, taking config.json with it when it is the
 *  one running. Refuses an unusable name and refuses to write over another
 *  skin; nothing is lost in a refused attempt. */
bool renameSkin (juce::File const &configDir, juce::String const &from,
                 juce::String const &to);

/** Remove a skin. If it is the one running, another takes over — the config
 *  must not be left pointing at nothing. The last skin cannot be deleted:
 *  an empty folder is a device with no look at all, and getting back out of
 *  that needs a file manager. */
bool deleteSkin (juce::File const &configDir, juce::String const &name);

}
