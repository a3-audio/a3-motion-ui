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

#include "SkinGroups.hh"

#include <array>

namespace a3
{

namespace
{
/** A group, and the paths that fall into it.
 *
 *  The order of this table is the order of the list. What a skin is made of
 *  comes first — you can design one without ever reaching the second half —
 *  and the effects follow, each split small enough that a heading still means
 *  something. The speaker light alone is thirty-four values; under one
 *  heading it *was* a third of the list.
 */
struct Group
{
  char const *heading;
  /** Exact paths, and prefixes marked with a trailing `*`. First match wins,
   *  so a specific path can be pulled out of a block a prefix would claim —
   *  which is how the light's colour sits with the other colours rather than
   *  among its own physics.
   *
   *  The star is not decoration. Matching every pattern as a prefix had
   *  `background` swallowing `backgroundGlow`, which is a different thing in
   *  a different part of the picture; saying which patterns are prefixes is
   *  the difference between a rule and a coincidence of spelling. */
  std::array<char const *, 14> paths;
};

constexpr Group groups[] = {
  { "Surfaces", { "surface", "surfaceRaised", "background" } },
  { "Text", { "textPrimary", "textMuted", "textOnAccent" } },
  { "States", { "accent", "warning", "danger", "notice", "alphaDisabled",
                "alphaInactive" } },
  { "Channels", { "channels.*" } },
  { "Sphere", { "sphereSurface", "sphereRim", "sphereEnvironment",
                "sphereScale", "boltCore", "backgroundGlow" } },
  { "Type and size", { "fontHeader", "fontBody", "potSize",
                       "clipSettingsHeightScale", "strokeThin",
                       "strokeThick" } },
  { "Touch", { "touchDragPixelsPerStep" } },

  // The colours of the two effects sit here, with the other colours, rather
  // than among the numbers that shape them: choosing a colour is the same
  // job wherever the value happens to live in the file.
  { "Effect colours", { "speakerLight.r", "speakerLight.g", "speakerLight.b",
                        "energy.r", "energy.g", "energy.b" } },

  { "Blobs", { "blob.*" } },
  { "Recording underlay", { "recordingUnderlay.*" } },

  { "Speaker light: beam", { "speakerLight.beamIntensity",
                             "speakerLight.speakerRadius",
                             "speakerLight.apertureAngle",
                             "speakerLight.wrapAngle",
                             "speakerLight.edgeSoftness", "speakerLight.cover",
                             "speakerLight.fray", "speakerLight.bleed",
                             "speakerLight.root", "speakerLight.levelFloor" } },
  { "Speaker light: response", { "speakerLight.vuMax", "speakerLight.curve",
                                 "speakerLight.attack",
                                 "speakerLight.decay" } },
  { "Speaker light: wander", { "speakerLight.wander*" } },
  { "Speaker light: bolts", { "speakerLight.bolt*" } },

  { "Energy: response", { "energy.vuMax", "energy.curve", "energy.attack",
                          "energy.decay", "energy.intensity" } },
  { "Energy: net", { "energy.net*" } },

  // Anything left. The parameter list is derived from the file so a new key
  // needs no registering; a grouping that dropped what it did not recognise
  // would take that back, and the value would be unreachable with nothing on
  // screen to say so.
  { "Other", {} },
};

constexpr int numGroups = static_cast<int> (sizeof (groups) / sizeof (*groups));

bool
matches (juce::String const &path, char const *pattern)
{
  if (pattern == nullptr)
    return false;

  juce::String const p (pattern);

  if (p.endsWithChar ('*'))
    return path.startsWith (p.dropLastCharacters (1));

  return path == p;
}
}

juce::String
skinGroupFor (juce::String const &path)
{
  // Up the path, not just at it. A colour is one row whose path names the
  // object — `accent` — but a half-written one comes through as `accent.r`,
  // and the value under a name is the same kind of thing as the name. Trying
  // each ancestor means a group is stated once, for the object, and holds for
  // whatever hangs off it.
  for (auto candidate = path; candidate.isNotEmpty ();)
    {
      for (auto const &group : groups)
        for (auto const *pattern : group.paths)
          if (matches (candidate, pattern))
            return group.heading;

      auto const dot = candidate.lastIndexOfChar ('.');
      if (dot <= 0)
        break;
      candidate = candidate.substring (0, dot);
    }

  // Nothing here claimed it. Fall back to what the editor did before there
  // were groups at all: the path without its last segment. That is not a
  // leftover — the same editor shows the Network page, whose keys are
  // `oscAddresses.out.*` and `oscSender.*` and which grouped itself perfectly
  // well that way. Sweeping them all under one heading would have been a
  // grouping that made one page tidier and another worse.
  auto const dot = path.lastIndexOfChar ('.');
  if (dot > 0)
    return path.substring (0, dot);

  return skinUngroupedHeading ();
}

int
skinGroupOrder (juce::String const &group)
{
  for (int i = 0; i < numGroups; ++i)
    if (group == groups[i].heading)
      return i;

  // Everything unclaimed shares one rank and sorts by path within it, which
  // keeps a block like `oscAddresses.out` together and in the order it has
  // always been in.
  return numGroups;
}

juce::String
skinUngroupedHeading ()
{
  return groups[numGroups - 1].heading;
}

}
