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

#include "SkinSections.hh"

#include <a3-motion-ui/theme/SkinParameters.hh>

#include <cmath>

namespace a3
{

namespace
{
/** An effect whose "off" is its own amount at zero -- most of them. */
SkinEffect
zeroedWhenOff (char const *key, SkinTunable amount)
{
  return { key, amount, { { amount.path, 0.0 } } };
}

/** The table. What each effect is, and which value turns it off, was read
 *  off the renderers (SphereShader.cc, MotionComponent::applyVisualConfig,
 *  CoronaScaling): every amount here already means "none" at zero in the
 *  shader, which is what makes a switch possible without touching them.
 *
 *  Ranges hold every value the shipped skins and the maintainer's own carry
 *  (a test reads the shipped ones), with room either side; `centre` puts
 *  what ships near the middle of a bar. */
std::vector<SkinSectionSpec>
buildSections ()
{
  std::vector<SkinSectionSpec> sections;

  sections.push_back (
      { SkinSection::Sphere,
        "sphere",
        "Sphere",
        true,
        {
            // Compiled in at 0.08 until the switches: the one effect of the
            // ball that had no value to turn it down with.
            zeroedWhenOff ("grid", { "sphereGrid", "Grid", 0.0, 3.0, 1.0 }),
            zeroedWhenOff ("edge", { "sphereLimb", "Edge", 0.0, 0.5, 0.18 }),
            zeroedWhenOff ("energy",
                           { "energy.intensity", "Energy", 0.0, 2.0, 0.5 }),
            // Inside the rim and outside it, riding the beams: one set of
            // filaments crossing the edge, so one switch.
            { "net",
              { "energy.netIntensity", "Net", 0.0, 4.0, 1.0 },
              { { "energy.netIntensity", 0.0 },
                { "energy.netBeamIntensity", 0.0 } } },
        },
        { { "sphereScale", "Size", 0.3, 1.0, 0.65 } },
        { { "sphereSurface", "Surface" }, { "sphereRim", "Rim" } } });

  sections.push_back (
      { SkinSection::Background,
        "background",
        "Background",
        true,
        {
            zeroedWhenOff ("glow", { "backgroundGlow.intensity", "Glow", 0.0,
                                     4.0, 1.0 }),
            zeroedWhenOff ("floor", { "speakerLight.floorLevel", "Floor", 0.0,
                                      2.0, 1.0 }),
            zeroedWhenOff ("floorBeams", { "speakerLight.floorBeams",
                                           "Floor beams", 0.0, 3.0, 1.0 }),
        },
        {},
        { { "background", "Ground" }, { "backgroundGlow", "Glow colour" } } });

  sections.push_back (
      { SkinSection::SpeakerTops,
        "speakerTops",
        "Speaker tops",
        true,
        {
            zeroedWhenOff ("beams", { "speakerLight.beamIntensity", "Beams",
                                      0.0, 10.0, 1.5 }),
            // The white core the band's bolts run with. Its colour is the
            // top-level `boltCore`; this is how bright it runs.
            zeroedWhenOff ("bolts", { "speakerLight.boltCore", "Bolts", 0.0,
                                      3.0, 1.0 }),
            zeroedWhenOff ("horns", { "speakerLight.topGlow", "Horn glow",
                                      0.0, 8.0, 2.0 }),
            // The whole cabinet, every face -- the subs' as well. Here
            // because it is what CLEAN relies on to show a top is playing.
            zeroedWhenOff ("cabinets", { "speakerLight.boxGlow",
                                         "Cabinet glow", 0.0, 2.0, 0.5 }),
        },
        { { "speakerLight.boltCount", "Bolt count", 1.0, 12.0, 6.0, true } },
        { { "speakerLight", "Light" }, { "boltCore", "Bolt core" } } });

  sections.push_back (
      { SkinSection::SpeakerBass,
        "speakerBass",
        "Speaker bass",
        true,
        {
            zeroedWhenOff ("ballLightning", { "speakerLight.ballLevel",
                                              "Ball lightning", 0.0, 3.0,
                                              1.0 }),
            zeroedWhenOff ("ports", { "speakerLight.subGlow", "Port glow", 0.0,
                                      5.0, 1.0 }),
        },
        { { "speakerLight.ballCount", "Balls", 0.0, 8.0, 3.0, true },
          { "speakerLight.ballSize", "Ball size", 0.01, 0.15, 0.05 } },
        {} });

  sections.push_back (
      { SkinSection::Blob,
        "blob",
        "Blob",
        true,
        {
            // A reach in body radii, and the shader divides by it: off is
            // one radius, a corona that ends where the body does.
            { "corona",
              { "blob.sizeMax", "Corona", 1.0, 8.0, 3.0 },
              { { "blob.sizeMin", 1.0 }, { "blob.sizeMax", 1.0 } } },
            zeroedWhenOff ("sparks", { "blobSparkle", "Sparks", 0.0, 2.0,
                                       1.0 }),
            zeroedWhenOff ("bolts", { "blobBolt", "Bolts", 0.0, 2.0, 1.0 }),
            zeroedWhenOff ("trail", { "blobTrail", "Trail", 0.0, 2.0, 1.0 }),
        },
        { { "blob.scale", "Size", 0.02, 0.12, 0.05 } },
        { { "blobAction", "Action" } } });

  sections.push_back (
      { SkinSection::Trajectory,
        "trajectory",
        "Trajectory",
        true,
        {
            // A radius of zero gives back a plain line, exactly (Theme.hh);
            // the weave is the same twist in the light around it.
            { "braid",
              { "braidRadius", "Braid", 0.0, 0.04, 0.008 },
              { { "braidRadius", 0.0 }, { "braidWeave", 0.0 } } },
            zeroedWhenOff ("glow", { "lineGlow", "Glow", 0.0, 2.0, 1.0 }),
            zeroedWhenOff ("filaments", { "lineFilament", "Filaments", 0.0,
                                          2.0, 1.0 }),
            zeroedWhenOff ("bolts", { "lineBolt", "Bolts", 0.0, 2.0, 1.0 }),
            zeroedWhenOff ("heat", { "lineHeat", "Heat", 0.0, 2.0, 1.0 }),
        },
        { { "trajectoryThickness", "Thickness", 0.001, 0.08, 0.006 } },
        {} });

  // Not a part of the picture: what the rest of the screen is set in. The
  // ranges are clampSkinValue's, so a bar cannot offer what the file would
  // refuse. Channel colours and the accent are deliberately not here -- a
  // channel's colour is its identity across every skin, not a look.
  sections.push_back (
      { SkinSection::Interface,
        "interface",
        "Text and size",
        false,
        {},
        { { "fontHeader", "Header text", 9.4, 31.2, 20.3 },
          { "fontBody", "Body text", 7.8, 26.0, 16.9 },
          { "potSize", "Pot size", 0.5, 2.0, 1.25 },
          { "buttonSize", "Button size", 0.5, 2.0, 1.25 },
          { "clipSettingsHeightScale", "Bar height", 0.5, 2.0, 1.25 } },
        {} });

  return sections;
}

constexpr char const *switchesKey = "switches";
constexpr char const *sectionOnKey = "on";

/** The object at `name` under `parent`, created when missing. */
juce::DynamicObject *
childObject (juce::var &parent, juce::Identifier const &name)
{
  auto *object = parent.getDynamicObject ();
  if (object == nullptr)
    return nullptr;

  if (!object->getProperty (name).isObject ())
    object->setProperty (name, juce::var (new juce::DynamicObject ()));

  return object->getProperty (name).getDynamicObject ();
}
}

std::vector<SkinSectionSpec> const &
skinSections ()
{
  static std::vector<SkinSectionSpec> const sections = buildSections ();
  return sections;
}

SkinSectionSpec const &
skinSectionSpec (SkinSection section)
{
  for (auto const &spec : skinSections ())
    if (spec.section == section)
      return spec;

  jassertfalse;
  return skinSections ().front ();
}

juce::String
sectionSwitchPath (SkinSection section)
{
  return juce::String (switchesKey) + "." + skinSectionSpec (section).key + "."
         + sectionOnKey;
}

juce::String
effectSwitchPath (SkinSection section, char const *effectKey)
{
  return juce::String (switchesKey) + "." + skinSectionSpec (section).key + "."
         + effectKey;
}

bool
skinSwitchIsOn (juce::var const &skin, juce::String const &path)
{
  auto value = skin;
  for (auto const &segment : juce::StringArray::fromTokens (path, ".", ""))
    {
      if (!value.isObject ())
        return true;
      value = value[juce::Identifier (segment)];
    }

  // Missing is on: a skin written before switches existed looks as it did.
  if (value.isVoid () || value.isUndefined ())
    return true;

  return static_cast<bool> (value);
}

void
setSkinSwitch (juce::var &skin, juce::String const &path, bool on)
{
  auto const segments = juce::StringArray::fromTokens (path, ".", "");
  if (segments.isEmpty () || !skin.isObject ())
    return;

  if (!on)
    {
      auto parent = skin;
      for (int i = 0; i < segments.size () - 1; ++i)
        {
          auto *child = childObject (parent, juce::Identifier (segments[i]));
          if (child == nullptr)
            return;
          parent = juce::var (child);
        }

      parent.getDynamicObject ()->setProperty (
          juce::Identifier (segments.strings.getLast ()), false);
      return;
    }

  // On is what a missing switch means: remove it, and any block it leaves
  // empty, so a skin switched off and on again is the file it was.
  juce::Array<juce::var> chain{ skin };
  for (int i = 0; i < segments.size () - 1; ++i)
    {
      auto const next = chain.getLast ()[juce::Identifier (segments[i])];
      if (!next.isObject ())
        return;
      chain.add (next);
    }

  for (int i = segments.size () - 1; i >= 0; --i)
    {
      auto *object = chain[i].getDynamicObject ();
      object->removeProperty (juce::Identifier (segments[i]));
      if (i > 0 && !object->getProperties ().isEmpty ())
        break;
    }
}

bool
skinEffectIsOn (juce::var const &skin, SkinSection section,
                char const *effectKey)
{
  return skinSwitchIsOn (skin, sectionSwitchPath (section))
         && skinSwitchIsOn (skin, effectSwitchPath (section, effectKey));
}

juce::var
withSkinSwitchesApplied (juce::var const &skin)
{
  if (!skin.isObject () || !skin.hasProperty (switchesKey))
    return skin;

  auto applied = skin.clone ();

  for (auto const &spec : skinSections ())
    for (auto const &effect : spec.effects)
      {
        if (skinEffectIsOn (skin, spec.section, effect.key))
          continue;

        for (auto const &override_ : effect.whileOff)
          setSkinValue (applied, override_.path, override_.offValue);
      }

  return applied;
}

juce::NormalisableRange<double>
skinTunableRange (SkinTunable const &tunable)
{
  juce::NormalisableRange<double> range (tunable.min, tunable.max);
  if (tunable.isWholeNumber)
    range.interval = 1.0;

  // setSkewForCentre asserts a centre strictly inside the range; a linear
  // bar is what a centre at the midpoint means anyway.
  auto const midpoint = (tunable.min + tunable.max) / 2.0;
  if (tunable.centre > tunable.min && tunable.centre < tunable.max
      && std::abs (tunable.centre - midpoint) > 1e-12)
    range.setSkewForCentre (tunable.centre);

  return range;
}

double
stepSkinTunable (SkinTunable const &tunable, double value, int steps)
{
  auto const range = skinTunableRange (tunable);

  if (tunable.isWholeNumber)
    {
      auto const across = range.convertTo0to1 (juce::jlimit (
                              tunable.min, tunable.max, value))
                          + static_cast<double> (steps)
                                / skinTunableStepsAcross;
      auto moved = std::round (range.convertFrom0to1 (
          juce::jlimit (0.0, 1.0, across)));

      // A hundredth of a short count is less than one: at least one, in
      // the direction asked.
      if (steps > 0)
        moved = juce::jmax (moved, std::round (value) + 1.0);
      if (steps < 0)
        moved = juce::jmin (moved, std::round (value) - 1.0);

      return juce::jlimit (tunable.min, tunable.max, moved);
    }

  auto const across = range.convertTo0to1 (
                          juce::jlimit (tunable.min, tunable.max, value))
                      + static_cast<double> (steps) / skinTunableStepsAcross;

  return range.convertFrom0to1 (juce::jlimit (0.0, 1.0, across));
}

juce::String
skinTunableText (SkinTunable const &tunable, double value)
{
  if (tunable.isWholeNumber)
    return juce::String (juce::roundToInt (value));

  if (juce::approximatelyEqual (value, 0.0))
    return "0";

  // Three significant digits: enough to tell two settings apart by eye, and
  // the same width whether the value is a gain or a thickness of 0.0018.
  auto const magnitude
      = static_cast<int> (std::floor (std::log10 (std::abs (value))));
  return juce::String (value, juce::jlimit (0, 4, 2 - magnitude));
}

}
