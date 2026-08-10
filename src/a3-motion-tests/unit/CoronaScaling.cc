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

#include <a3-motion-ui/components/CoronaScaling.hh>

using namespace a3;

namespace
{

// Measured on a live system via `oscdump 7772` while playing back material at
// a normal level (≈ -16 dBFS peak). The corona has to be visible here — this
// is what the effect looks like in actual use, not a worst case.
constexpr float realisticPeak = 0.160538f;
constexpr float realisticRms = 0.053968f;

TEST (CoronaScaling, CoronaExtendsBeyondBlobAtRealisticLevel)
{
  CoronaConfig cfg;

  auto const level = coronaVuLevel (realisticPeak, realisticRms, cfg.vuMax);
  auto const scale = coronaScaleFactor (level, cfg);

  // The solid blob disc is drawn on top of the corona, so anything at or
  // below 1.0 is completely hidden.
  EXPECT_TRUE (coronaExtendsBeyondBlob (scale))
      << "corona scale " << scale << " does not reach past the blob";
}

// Alpha and white blend follow the peak alone, so the peak leg of the curve is
// needed separately from the combined level.
TEST (CoronaScaling, PeakLevelSaturatesAtVuMax)
{
  EXPECT_FLOAT_EQ (coronaPeakLevel (0.25f, 0.25f), 1.f);
  EXPECT_FLOAT_EQ (coronaPeakLevel (0.5f, 0.25f), 1.f); // clamped
  EXPECT_LT (coronaPeakLevel (0.05f, 0.25f), 1.f);
}

TEST (CoronaScaling, ConfigOverridesReplaceDefaults)
{
  auto const parsed = juce::JSON::parse (R"({"corona": {"sizeMin": 1.3}})");

  auto const cfg = loadCoronaConfig (parsed);

  EXPECT_FLOAT_EQ (cfg.sizeMin, 1.3f);
  EXPECT_FLOAT_EQ (cfg.sizeMax, CoronaConfig{}.sizeMax); // untouched key
}

// The bug this guards against was never in the code — the shipped config.json
// overrode sizeMin/sizeMax with values that put the corona entirely underneath
// the blob. Tuning those numbers must not silently switch the effect off again.
TEST (CoronaScaling, ShippedConfigKeepsCoronaVisibleAtRealisticLevel)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ()) << "no config.json at " << A3_CONFIG_JSON_PATH;

  auto const cfg = loadCoronaConfig (juce::JSON::parse (file.loadFileAsString ()));
  auto const level = coronaVuLevel (realisticPeak, realisticRms, cfg.vuMax);
  auto const scale = coronaScaleFactor (level, cfg);

  EXPECT_TRUE (coronaExtendsBeyondBlob (scale))
      << "config.json yields corona scale " << scale
      << " at a normal playback level — the corona stays hidden under the blob";
}

}
