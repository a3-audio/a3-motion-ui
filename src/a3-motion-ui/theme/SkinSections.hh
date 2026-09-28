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

#include <vector>

namespace a3
{

/** The parts of the picture the skin editor is split into, top to bottom in
 *  the order the eye meets them on the device.
 *
 *  Asked for on 2026-09-28: *"wir müssen stark parameter reduzieren und
 *  sinnvoll in kategorien trennen (Sphere, Background, speakertops,
 *  speakerbass, blob, trajectory)"*. The old editor listed every one of the
 *  skin's ~110 values under twenty headings named after the shader's own
 *  structure; these six are named after what a performer sees.
 *
 *  `Interface` is the one addition, and it is not an effect: the channel
 *  colours, the accent and the two type sizes belong to none of the six, and
 *  since 2026-08-28 the skin editor is the only place a font size or the pot
 *  size can be set on the device. Hiding them would have taken that back. It
 *  has no switch — a font cannot be turned off. */
enum class SkinSection
{
  Sphere,
  Background,
  SpeakerTops,
  SpeakerBass,
  Blob,
  Trajectory,
  Interface,
};

/** One value an effect's switch writes over while it is off. */
struct SkinOverride
{
  char const *path;
  double offValue;
};

/** A value the panel offers a bar for: where it lives, what it is called,
 *  and the travel of its bar.
 *
 *  `centre` is the value half way along the bar. Most of these are spread
 *  over a range whose useful end is small -- the line ships at 0.0018 of a
 *  range reaching 0.08 -- and a linear bar puts that at the first
 *  percent of its travel, where a finger cannot find it. JUCE's skew does
 *  the rest (`juce::NormalisableRange::setSkewForCentre`). */
struct SkinTunable
{
  char const *path;
  char const *label;
  double min;
  double max;
  double centre;
  bool isWholeNumber = false;
};

/** A thing a section draws that can be switched off on its own.
 *
 *  `amount` is the effect's own strength, the one value it is tuned by, and
 *  sits on the same row as its switch. `whileOff` are the values the switch
 *  writes over while it is off -- usually just the amount at zero, but the
 *  net is two fields that cross the rim, the braid is a radius and a weave,
 *  and the corona's "off" is a reach of one body radius rather than zero,
 *  because a reach of zero divides by it. */
struct SkinEffect
{
  char const *key;
  SkinTunable amount;
  std::vector<SkinOverride> whileOff;
};

/** A colour the panel offers a swatch for. Opens the picker. */
struct SkinColourRow
{
  char const *path;
  char const *label;
};

struct SkinSectionSpec
{
  SkinSection section;
  /** The section's key under `switches` in the skin file. */
  char const *key;
  char const *label;
  /** False for Interface only. */
  bool hasSwitch;
  std::vector<SkinEffect> effects;
  std::vector<SkinTunable> values;
  std::vector<SkinColourRow> colours;
};

/** Every section, in the order the panel shows them. */
std::vector<SkinSectionSpec> const &skinSections ();

SkinSectionSpec const &skinSectionSpec (SkinSection section);

/** Where a switch lives in the skin: `switches.<section>.on` for a whole
 *  section, `switches.<section>.<effect>` for one of its effects. Its own
 *  block rather than a flag beside each value, so a skin written before
 *  switches existed has nothing in it to misread, and a skin with every
 *  switch on can drop the block without changing. */
juce::String sectionSwitchPath (SkinSection section);
juce::String effectSwitchPath (SkinSection section, char const *effectKey);

/** A switch's state. **Missing means on**: every skin written before
 *  switches existed has to look exactly as it did. */
bool skinSwitchIsOn (juce::var const &skin, juce::String const &path);
void setSkinSwitch (juce::var &skin, juce::String const &path, bool on);

/** Whether an effect is drawn: its own switch and its section's. */
bool skinEffectIsOn (juce::var const &skin, SkinSection section,
                     char const *effectKey);

/** The skin the renderers are given: the file's own values, with every
 *  switched-off effect's values written over by what "off" means.
 *
 *  One place, before anything reads the skin, rather than a check in every
 *  renderer: the shader, the corona and the theme all read values that
 *  already mean "off" at zero, so a switch is only a way to put a zero there
 *  without losing the number the skin had. The file keeps the number; the
 *  switch decides whether it is used.
 *
 *  A skin with no `switches` block comes back untouched -- the same var, not
 *  a copy -- so everything written before this is drawn as it always was. */
juce::var withSkinSwitchesApplied (juce::var const &skin);

/** The range a bar travels, with its skew, for a tunable. The one source for
 *  the slider and for the steps beside it, so the two cannot disagree. */
juce::NormalisableRange<double> skinTunableRange (SkinTunable const &tunable);

/** `value` moved `steps` hundredths of the bar's travel, and held inside it.
 *
 *  Steps across the bar rather than in the value's own units: on a skewed
 *  bar a fixed step in value is a crawl at one end and a leap at the other,
 *  where a share of the travel is the same distance under the finger
 *  everywhere. A whole number moves by at least one. */
double stepSkinTunable (SkinTunable const &tunable, double value, int steps);

/** What a bar says its value is. Three significant digits, and a count
 *  without decimals. */
juce::String skinTunableText (SkinTunable const &tunable, double value);

/** How many steps the − and + keys divide a bar into. */
constexpr int skinTunableStepsAcross = 100;

}
