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


#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/theme/SkinGroups.hh>

using namespace a3;

// The values that make a skin come first and sit together. Sorted by their
// spelling, `background` and `surface` were forty rows apart with the speaker
// light's thirty-four in between.
TEST (SkinGroups, WhatDesignsASkinComesBeforeWhatTunesAShader)
{
  auto const design = { "surface", "textPrimary", "accent", "channels.0.r",
                        "sphereRim", "fontBody" };
  auto const tuning = { "speakerLight.boltEscape", "energy.netLacunarity",
                        "blob.whiteBlend", "recordingUnderlay.lineThickness" };

  for (auto const *a : design)
    for (auto const *b : tuning)
      EXPECT_LT (skinGroupOrder (skinGroupFor (a)),
                 skinGroupOrder (skinGroupFor (b)))
          << a << " should come before " << b;
}

// The three that describe one surface belong under one heading, and so on.
// This is the whole point: what is read together is edited together.
TEST (SkinGroups, ThingsThatBelongTogetherShareAHeading)
{
  auto const same = {
    std::vector<char const *>{ "surface", "surfaceRaised", "background" },
    std::vector<char const *>{ "textPrimary", "textMuted", "textOnAccent" },
    std::vector<char const *>{ "accent", "warning", "danger", "notice" },
    std::vector<char const *>{ "sphereSurface", "sphereRim", "sphereScale",
                               "boltCore", "backgroundGlow" },
    std::vector<char const *>{ "fontHeader", "fontBody", "potSize",
                               "clipSettingsHeightScale" },
    std::vector<char const *>{ "speakerLight.boltWidth",
                               "speakerLight.boltCount",
                               "speakerLight.boltEscape" },
    std::vector<char const *>{ "energy.netScale", "energy.netGain",
                               "energy.netOctaves" },
  };

  for (auto const &group : same)
    {
      auto const heading = skinGroupFor (group.front ());
      EXPECT_NE (heading, skinUngroupedHeading ()) << group.front ();
      for (auto const *path : group)
        EXPECT_EQ (skinGroupFor (path), heading) << path;
    }
}

// The speaker light is thirty-four values and was one heading. Split, because
// a heading that covers a third of the list is not a heading.
TEST (SkinGroups, TheSpeakerLightIsMoreThanOneHeading)
{
  std::set<juce::String> headings;
  for (auto const *path :
       { "speakerLight.r", "speakerLight.boltWidth", "speakerLight.wander",
         "speakerLight.apertureAngle" })
    headings.insert (skinGroupFor (path));

  EXPECT_GE (headings.size (), 3u);
}

// A key nobody placed still appears. The parameter list is derived from the
// file so a new key needs no registering; a grouping that dropped what it did
// not recognise would take that back, and the value would be unreachable with
// nothing to say so.
TEST (SkinGroups, AnUnknownKeyLandsSomewhereRatherThanNowhere)
{
  auto const heading = skinGroupFor ("somethingNobodyHasWrittenYet");

  EXPECT_EQ (heading, skinUngroupedHeading ());
  EXPECT_FALSE (heading.isEmpty ());

  // And last, so it does not interrupt what is ordered.
  for (auto const *known : { "surface", "speakerLight.boltEscape" })
    EXPECT_LT (skinGroupOrder (skinGroupFor (known)),
               skinGroupOrder (heading));
}

// The same editor shows the Network page, whose keys are none of the above and
// grouped themselves perfectly well by their own path. They keep doing that:
// a grouping that tidied one page by flattening another would be a trade, not
// an improvement.
TEST (SkinGroups, PathsFromAnotherPageKeepTheirOwnHeadings)
{
  EXPECT_EQ (skinGroupFor ("oscAddresses.out.channelAzimuth"),
             "oscAddresses.out");
  EXPECT_EQ (skinGroupFor ("oscAddresses.in.vuPrefix"), "oscAddresses.in");
  EXPECT_EQ (skinGroupFor ("oscSender.host"), "oscSender");

  // Together and after everything a skin has, so neither page interrupts the
  // other if they ever appear in one list.
  EXPECT_EQ (skinGroupOrder (skinGroupFor ("oscSender.host")),
             skinGroupOrder (skinGroupFor ("oscReceiver.port")));
  EXPECT_LT (skinGroupOrder (skinGroupFor ("blob.whiteBlend")),
             skinGroupOrder (skinGroupFor ("oscSender.host")));
}
