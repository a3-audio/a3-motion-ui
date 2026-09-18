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

#include <ShippedSkin.hh>

#include <a3-motion-ui/theme/SkinGroups.hh>

using namespace a3;

// A blob at standard elevation sits on the equator, and in the overhead view
// the equator *is* the silhouette -- so half of it lies over the lit ball and
// half over the dark background. Measured on 2026-09-18: the ground around it
// is about twice as bright inside as outside, and the step reads as the blob
// being cut in half. "auch sind die blobs am sphärenrand abgeschnitten. wie
// kann das sein?"
//
// The ball's light fades out towards its edge now instead of ending on a
// two-pixel band, which is what a sphere does anyway.

TEST (SphereLimb, TheSurfaceFadesOutBeforeTheEdge)
{
  auto const shader = juce::File (A3_UI_SOURCE_DIR)
                          .getChildFile ("components/SphereShader.cc")
                          .loadFileAsString ();

  juce::StringArray lines;
  lines.addLines (shader);

  juce::String blend;
  for (auto const &line : lines)
    if (line.contains ("col = mix (colOutFinal, colSurf"))
      blend = line;

  EXPECT_FALSE (blend.isEmpty ()) << "nothing blends the ball into its ground";
  EXPECT_TRUE (blend.contains ("limbMix"))
      << "the ball's light still ends on the silhouette's own two pixels, so "
         "anything sitting on the equator is cut in half by the ground behind "
         "it: "
      << blend;
}

TEST (SphereLimb, TheAlphaFadesWithIt)
{
  auto const shader = juce::File (A3_UI_SOURCE_DIR)
                          .getChildFile ("components/SphereShader.cc")
                          .loadFileAsString ();

  juce::StringArray lines;
  lines.addLines (shader);

  juce::String alpha;
  for (auto const &line : lines)
    if (line.contains ("alpha = mix (1.0, sphereAlpha"))
      alpha = line;

  EXPECT_FALSE (alpha.isEmpty ());
  EXPECT_TRUE (alpha.contains ("limbMix"))
      << "the colour fades and the transparency does not, which is a step of "
         "its own in the same place: "
      << alpha;
}

TEST (SphereLimb, ItIsASkinValueUnderTheSphere)
{
  EXPECT_EQ (skinGroupFor ("sphereLimb"), "Sphere");
}

TEST (SphereLimb, TheShippedSkinCarriesIt)
{
  auto const skin = shippedSkin ();
  EXPECT_TRUE (skin.hasProperty ("sphereLimb"))
      << "a value nobody can reach in the editor is a value nobody will tune";
}
