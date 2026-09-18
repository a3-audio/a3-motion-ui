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

// The one that actually cut them. The floor darkens what is behind it, and it
// is applied at the very end because everything else adds light -- so it was
// darkening the blobs too. Outside the silhouette that is the full darkening
// and inside only a third of it (uFloorThrough), which put a step across every
// blob at standard elevation: measured on the yellow one, 148 above the
// silhouette against 198 below it. A blob stands at ear height, above the
// floor, so its light is added after the floor rather than before.
TEST (SphereLimb, TheFloorDoesNotDarkenTheBlobs)
{
  auto const shader = juce::File (A3_UI_SOURCE_DIR)
                          .getChildFile ("components/SphereShader.cc")
                          .loadFileAsString ();

  juce::StringArray lines;
  lines.addLines (shader);

  auto blobsAdded = -1;
  auto floorDarkens = -1;
  for (int i = 0; i < lines.size (); ++i)
    {
      if (lines[i].contains ("col += blobs;"))
        blobsAdded = i;
      if (lines[i].contains ("col = mix (col, col * uFloorDark"))
        floorDarkens = i;
    }

  EXPECT_GT (blobsAdded, 0) << "nothing draws the blobs";
  EXPECT_GT (floorDarkens, 0) << "the floor darkens nothing";
  EXPECT_GT (blobsAdded, floorDarkens)
      << "the blobs are added before the floor darkens what is behind it, so "
         "the floor cuts every blob that sits on the silhouette";
}

