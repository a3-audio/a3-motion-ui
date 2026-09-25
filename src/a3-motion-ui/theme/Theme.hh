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
  /** A state worth noticing that is neither good news nor bad — the Pioneer
   *  clock, say. accent was blue in an older skin and is green in this one,
   *  which is why PIO stopped standing out from INT. */
  ThemeColour notice{ 70, 130, 250 };
  /** A momentary emphasis: the accent key, which is not a state you are in but
   *  something you do for as long as you hold it. Neither the green of
   *  "running" nor the red of "this writes over something". */
  ThemeColour highlight{ 255, 214, 10 };
  float alphaDisabled = 0.35f;
  float alphaInactive = 0.6f;

  /** How loud a thing is drawn, as a share of its role's colour.
   *
   *  Ten rungs of one emphasis ladder, ordered by loudness alone — not by
   *  what shape the drawing takes. They were derived from what the code
   *  already did, and at that point most fills did sit low and most text
   *  did sit high, which is where alphaFill, alphaOutline and alphaDisabled
   *  got their names: rungs 1, 2 and 4 of the ladder, named after the
   *  drawing form that happened to dominate them at the time. The
   *  migration that put every remaining literal on its nearest rung
   *  (`.claude/notes/2026-09-07-metrik-rollen-design.md`) broke that
   *  correlation on purpose wherever a site's actual value disagreed with
   *  its shape — the value won, not the name — so a fill at 0.15 now reads
   *  `alphaOutline` and a stroke at 0.35 reads `alphaDisabled`
   *  (`ClipSettingsComponent.cc`'s channel faces are one example). A
   *  skinner turning `alphaOutline` down to make hairlines fainter will
   *  therefore also fade every fill parked on that rung, and there is no
   *  way to move just one without re-auditing all of them. Renaming the
   *  three misleading rungs to say only "how loud", not "what shape", is
   *  open work and deliberately not done here — see the deviations file. */
  float alphaFill = 0.06f;
  float alphaOutline = 0.15f;
  /** A mark you should be able to find without it competing with what it
   *  marks — the ear-height ring in the elevation graphic is the one that
   *  asked for it. Louder than a hairline, quieter than an emphasised fill. */
  float alphaGuide = 0.22f;
  float alphaFillEmphasis = 0.28f;
  float alphaMuted = 0.5f;
  /** How much of what lies behind them the two panels let through.
   *
   *  `panelOpacity` is the clip settings bar over the sphere: it has always
   *  been 0.85, and the pads page depends on being a child of it rather than
   *  a sibling under it. The menu, the skin editor and the colour picker have
   *  two: `overlayScrim` dims everything around their panel, `overlayOpacity`
   *  is the panel itself.
   *
   *  They were one value at 0.55, which drew the sphere's trajectory straight
   *  through the rows you are reading -- and making that one value opaque took
   *  the whole sphere away with it, because it fills the component and not the
   *  card. Two roles, so the rows can be solid while the ball stays visible
   *  around them and "Sphere in Menu" keeps its meaning.
   *
   *  Here rather than compiled in, because a decision about how something
   *  looks belongs to the skin -- and because the same 0.55 stood in three
   *  files, which is how it comes to mean three different things. */
  float panelOpacity = 0.85f;
  float overlayScrim = 0.55f;
  float overlayOpacity = 1.f;
  /** The main menu's own panel, over the scrim. See-through (0) by default:
   *  the maintainer wanted the sphere back behind the menu (2026-09-25),
   *  while the skin editor and the colour picker keep `overlayOpacity` --
   *  the picker has no scrim, and at 0 it would vanish. */
  float menuPanelOpacity = 0.f;
  /** Legible, but not the thing you are looking at: captions, resting
   *  states, the rows you did not select. Seven sites sat between 0.70 and
   *  0.75 and meant this; they are one rung now. */
  float alphaSecondary = 0.7f;
  float alphaTextStrong = 0.85f;
  /** The one thing you have hold of right now — brighter than anything
   *  merely legible. Drawn while the camera ball is being dragged. */
  float alphaActive = 0.95f;

  // Channels
  ThemeColour channel[numThemeChannels]
      = { { 216, 17, 89 }, { 69, 78, 158 }, { 247, 208, 2 }, { 33, 131, 128 } };

  // Sphere and shader
  ThemeColour sphereSurface{ 10, 10, 14 };      // was vec3 (0.04, 0.04, 0.055)
  ThemeColour sphereRim{ 128, 140, 166 };       // was vec3 (0.5, 0.55, 0.65)
  ThemeColour sphereEnvironment{ 5, 6, 8 };     // was vec3 (0.02, 0.025, 0.03)
  ThemeColour boltCore{ 255, 255, 255 };
  ThemeColour backgroundGlow{ 70, 130, 250 };
  ThemeColour speakerLight{ 70, 130, 250 };
  ThemeColour energy{ 150, 220, 255 };

  /** What a blob wears while an action script has it.
   *
   *  Neon rather than white: a VU peak already blends the blob towards white,
   *  and a second signal borrowing the first one's colour says nothing. A
   *  skin value so it can be moved off a channel colour it happens to collide
   *  with -- on a skin whose first channel is already pink it is the flicker
   *  rather than the hue that carries the message. */
  ThemeColour blobAction{ 184, 61, 255 };

  /** How much of each of the blob's three effects there is: the flecks thrown
   *  off it, the lightning on a transient, and the wake behind it. One is
   *  what the device ships with, zero is off, two is as far as it goes.
   *
   *  Zero has to mean off for all three, or "psychonautic" is a look nobody
   *  can decline. */
  float blobSparkle = 1.f;
  float blobBolt = 1.f;
  float blobTrail = 1.f;

  // Sizes, as a share of the component's shorter side
  float sphereScale = 0.62f;
  float blobScale = 0.05f;

  /** How thick the played trajectory is drawn on the sphere, in the same
   *  normalised units as the sphere itself.
   *
   *  A constant of 0.04 in MotionComponent until 2026-09-13, which made "draw
   *  it thinner" a rebuild rather than a knob. Halved as it moved, on the
   *  maintainer's call -- and the line has to give room back now that the blob
   *  is getting a trail. */
  float trajectoryThickness = 0.0018f;

  /** What the trajectory burns with.
   *
   *  The line itself is a vector stroke -- the one thing that can be a crisp
   *  line thinner than a pixel. Everything that *glows* is a field in the
   *  fragment shader, found through a rasterised map of where the line is
   *  (SphereShader::setLineTexture), because JUCE's 2D context has no
   *  additive blend at all and a stroked "plasma" is a stack of translucent
   *  ribbons that looks like one.
   *
   *  This replaced ten values that described a braid of three hairlines
   *  inside a coil of bolts, all of it built from strands offset along the
   *  line's normal. An offset copy of a curve folds where its curvature times
   *  the offset passes one -- and at the pole, where every azimuth meets, it
   *  fans out into straight spokes across the middle of the sphere. A level
   *  set of a field cannot do that, and that is what the filaments are now.
   *
   *  One is what the device ships with, zero is off, two is as far as it
   *  goes. */
  /** The braid: three hairlines twisted into a cord.
   *
   *  `radius` is how far a strand stands off the axis, `turns` how many
   *  windings there are over the whole figure, `spin` how fast the cord turns
   *  along its own length (signed), `strands` how many there are.
   *
   *  A radius of zero or a single strand gives back a plain line, exactly.
   *
   *  This was built once, taken out, and put back. What made it safe the
   *  second time is foldGuard() in PlasmaSheath.hh: an offset copy of a curve
   *  folds where the curvature times the offset passes one, and at the pole --
   *  where every azimuth of a figure meets at a point -- that fold drew
   *  straight grey spokes across the middle of the sphere. The strands are
   *  pulled back onto the axis before it arrives now, so the cord closes to a
   *  single thread through a cusp. */
  /** How far a strand of the cord stands off its axis. Vectors, because a
   *  hairline is a vector: a stroke can be thinner than a pixel and a field
   *  read from a map two and a half screen pixels a texel cannot. */
  float braidRadius = 0.0048f;
  /** How hard the same twist runs through the *light* around the cord. The
   *  glow is the shader's, so it cannot show three strands -- what it can show
   *  is what a twisted cord does to the light, cresting once per strand per
   *  winding. Zero leaves the glow smooth. */
  float braidWeave = 1.6f;
  float braidTurns = 60.f;
  float braidSpin = 0.10f;
  float braidStrands = 3.f;

  float lineGlow = 1.f;
  float lineFilament = 1.f;
  float lineBolt = 1.f;
  /** How hard the line runs towards white where that channel's blob is. It is
   *  the wire being energised where the sound on it actually is. */
  float lineHeat = 1.f;
  /** How far a pad is dimmed from its channel's colour for what the slot is
   *  doing. Subtractions from full, so a bigger number is a darker pad; see
   *  theme/PadStatusColours.hh for which state wears which. */
  float padShadeEmpty = 0.85f;
  float padShadeIdle = 0.3f;
  float padShadeBlink = 0.6f;

  float strokeThin = 1.f;
  /** Between the two: half a pixel from either, so it is a doubling rather
   *  than something that could snap. The response curve in the clip bar and
   *  the caret in the file list are drawn with it. */
  float strokeMedium = 1.5f;
  float strokeThick = 2.f;

  // Corner radii, in pixels. Named after where the rounded rectangle is
  // drawn, not after how round it is: the same 3px is a chip in the clip bar
  // and a chip in the action card, and both should follow one skin value.
  float radiusTick = 2.f;
  float radiusControl = 3.f;
  float radiusRow = 5.f;
  float radiusCard = 8.f;
  float radiusPanel = 10.f;

  // Insets, in pixels. A doubling scale: the hair is the gap between two
  // cells that must not touch, and every step above it is twice the last.
  float paddingHair = 1.f;
  float paddingTight = 2.f;
  float paddingSmall = 4.f;
  float padding = 8.f;

  // Font sizes, absolute and straight out of the skin. They used to be base
  // sizes with a percentage from the menu on top — two sources for one size,
  // where switching skin moved one and left the other.
  float fontHeader = 18.f;
  float fontBody = 15.f;

  /** Knob and toggle size in the clip settings bar, relative to the built-in
   *  size. Part of the look, so it lives with the rest of it. */
  float potSize = 1.f;

  /** How far a finger has to travel before a control steps once. Here and
   *  not a compile-time constant because what feels right is decided at
   *  the panel — the same reason potSize and the font sizes live here. */
  int touchDragPixelsPerStep = 12;

  /** How tall the clip settings bar is, as a multiple of what its contents
   *  ask for. 1.0 is what they ask for; below that the bar is squeezed and
   *  gives the sphere the room back, above it gets more air. A skin value
   *  like potSize, so it is dialled on the device rather than compiled in. */
  float clipSettingsHeightScale = 1.f;

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

/** Every role loadTheme reads, with the built-in value it would fall back to.
 *
 *  The skin editor derives its list from the file it loaded, so a key a file
 *  does not name is a key nobody can reach. Merging this in gives the editor
 *  the full vocabulary whatever the file happens to carry.
 *
 *  Excluded: `speakerLight`, `energy`, `blob` (all of it, including
 *  `scale`), `channels`, and `backgroundGlow`. Every one of these is a
 *  compound object in the skin files, not a plain colour or a plain number,
 *  and every shipped skin already states it in full. `backgroundGlow` in
 *  particular still has a Theme field and a loadTheme() line, but
 *  MotionComponent reads the group straight off the skin var with its own
 *  defaults rather than through that field -- restating only its colour half
 *  here would put a default in the editor that disagrees with the one the
 *  effect actually uses. */
juce::var themeDefaultsVar ();

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

/** The name of the skin that edits to `edited` are written to.
 *
 *  The shipped default is never written. It is what "reset" restores from and
 *  what a device that has been dialled into a corner is brought back to, and
 *  neither works if it can be overwritten. Editing it therefore branches off:
 *  the changes land in a skin of their own, which then becomes the active one.
 *
 *  Every other name is its own. */
juce::String skinNameToWriteTo (juce::String const &edited);

/** `skin` with the groups under the names they have now.
 *
 *  The names grew with the code and stopped saying what the things are: the
 *  corona is the blob's halo, and the "sphere glow" is the field behind
 *  everything rather than something the sphere does. blobScale sat at the top
 *  level while the rest of the blob had a group of its own.
 *
 *  Applied on every load rather than once over the files, because a skin can
 *  arrive from anywhere -- an older device, a copy somebody kept. Running it
 *  on an already-current skin changes nothing, and where a file carries both
 *  spellings the current one wins: that file was half-edited, and the new name
 *  is the one somebody meant. */
juce::var migrateSkinNames (juce::var const &skin);

/** The skin that must never be written, and the one a branch lands in. */
constexpr char const *protectedSkinName = "default";
constexpr char const *branchedSkinName = "custom";

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
