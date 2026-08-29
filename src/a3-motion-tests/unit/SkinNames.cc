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

#include <a3-motion-ui/theme/Theme.hh>

using namespace a3;

namespace
{

juce::var
parse (juce::String const &text)
{
  return juce::JSON::parse (text);
}

// The names grew with the code and stopped saying what the things are: the
// corona is the blob's halo, and the "sphere glow" is the field behind
// everything rather than anything the sphere does. A skin written before the
// rename still has to load, and it has to load the same values.
TEST (SkinNames, TheOldNamesAreCarriedOver)
{
  auto const old = parse (R"({
    "corona": { "vuMax": 0.3, "alphaMax": 0.7 },
    "sphereGlow": { "intensity": 2.5 },
    "blobScale": 0.05,
    "energy": { "intensity": 1.0 }
  })");

  auto const now = migrateSkinNames (old);

  EXPECT_TRUE (now.hasProperty ("blob"));
  EXPECT_TRUE (now.hasProperty ("background"));
  EXPECT_FALSE (now.hasProperty ("corona"));
  EXPECT_FALSE (now.hasProperty ("sphereGlow"));

  EXPECT_FLOAT_EQ ((float)now["blob"]["vuMax"], 0.3f);
  EXPECT_FLOAT_EQ ((float)now["blob"]["alphaMax"], 0.7f);
  EXPECT_FLOAT_EQ ((float)now["background"]["intensity"], 2.5f);
}

// blobScale sat at the top level while everything else about the blob was in a
// group of its own. It moves in with them.
TEST (SkinNames, TheBlobSizeMovesInWithTheRestOfTheBlob)
{
  auto const now = migrateSkinNames (parse (R"({"blobScale": 0.07})"));

  EXPECT_FLOAT_EQ ((float)now["blob"]["scale"], 0.07f);
  EXPECT_FALSE (now.hasProperty ("blobScale"));
}

// Everything else is left exactly as it was -- a migration that touches what
// it was not asked to touch is a migration nobody can trust.
TEST (SkinNames, WhatWasNotRenamedIsUntouched)
{
  auto const now = migrateSkinNames (parse (R"({
    "energy": { "intensity": 1.5 },
    "speakerLight": { "beamIntensity": 1.3 },
    "recordingUnderlay": { "opacity": 0.28 },
    "sphereScale": 0.62,
    "fontHeader": 1.0
  })"));

  EXPECT_FLOAT_EQ ((float)now["energy"]["intensity"], 1.5f);
  EXPECT_FLOAT_EQ ((float)now["speakerLight"]["beamIntensity"], 1.3f);
  EXPECT_FLOAT_EQ ((float)now["recordingUnderlay"]["opacity"], 0.28f);
  EXPECT_FLOAT_EQ ((float)now["sphereScale"], 0.62f);
  EXPECT_FLOAT_EQ ((float)now["fontHeader"], 1.0f);
}

// Run twice it must give the same thing: the app migrates on every load, and a
// skin already carrying the new names goes through it too.
TEST (SkinNames, MigratingTwiceChangesNothing)
{
  auto const once = migrateSkinNames (parse (R"({
    "corona": { "vuMax": 0.3 }, "blobScale": 0.05
  })"));
  auto const twice = migrateSkinNames (once);

  EXPECT_FLOAT_EQ ((float)twice["blob"]["vuMax"], 0.3f);
  EXPECT_FLOAT_EQ ((float)twice["blob"]["scale"], 0.05f);
}

// A new name already present wins: a file that carries both is one somebody
// half-edited, and the current spelling is the one they meant.
TEST (SkinNames, TheNewNameWinsWhenBothArePresent)
{
  auto const now = migrateSkinNames (parse (R"({
    "corona": { "vuMax": 0.1 },
    "blob": { "vuMax": 0.9 }
  })"));

  EXPECT_FLOAT_EQ ((float)now["blob"]["vuMax"], 0.9f);
}

}
