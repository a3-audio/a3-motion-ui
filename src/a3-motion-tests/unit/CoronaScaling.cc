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
  auto const parsed = juce::JSON::parse (R"({"blob": {"sizeMin": 1.3}})");

  auto const cfg = loadCoronaConfig (parsed);

  EXPECT_FLOAT_EQ (cfg.sizeMin, 1.3f);
  EXPECT_FLOAT_EQ (cfg.sizeMax, CoronaConfig{}.sizeMax); // untouched key
}

// The bug this guards against was never in the code — the shipped config.json
// overrode sizeMin/sizeMax with values that put the corona entirely underneath
// the blob. Tuning those numbers must not silently switch the effect off again.
TEST (CoronaScaling, ShippedConfigKeepsCoronaVisibleAtRealisticLevel)
{

  auto const cfg = loadCoronaConfig (shippedSkin ());
  auto const level = coronaVuLevel (realisticPeak, realisticRms, cfg.vuMax);
  auto const scale = coronaScaleFactor (level, cfg);

  EXPECT_TRUE (coronaExtendsBeyondBlob (scale))
      << "config.json yields corona scale " << scale
      << " at a normal playback level — the corona stays hidden under the blob";
}

}

// ── What the shader's blob is actually handed ───────────────────────────
//
// Captured from the running rig on 2026-09-16 by sharing port 7772 with the
// app (SO_REUSEPORT) while programme material played: /vu/1 arrived at peak
// 0.323, rms 0.096. The other three channels were silent to five decimal
// places, which is what a four-channel rig looks like most of the time.
//
// The blob used to be a 2D disc and took its level through coronaVuLevel().
// When it moved into the shader (ce10153) the scaling was left behind and the
// raw peak went in instead — so the level the blob reacts to became whatever
// the meter happened to read, with no vuMax under it. The maintainer's report
// was "die blobs reagieren nicht mehr auf input vu".

namespace
{
constexpr float rigPeak = 0.323f;   // /vu/1, loudest channel, real material
constexpr float rigRms = 0.096f;
}

TEST (CoronaScaling, RealMaterialDrivesTheBlobMostOfTheWay)
{
  CoronaConfig cfg;
  auto const level = coronaVuLevel (rigPeak, rigRms, cfg.vuMax);

  EXPECT_GT (level, 0.7f)
      << "a channel that is plainly playing barely moves the blob";
  EXPECT_LE (level, 1.f);
}

TEST (CoronaScaling, TheRawPeakIsNotAUsableBlobLevel)
{
  // The other half: it has to be worth scaling. A raw peak of 0.32 sits below
  // the threshold the blob's own bolt needs (vu > 0.25 only just), so most of
  // what the blob can do never comes out.
  CoronaConfig cfg;

  EXPECT_GT (coronaVuLevel (rigPeak, rigRms, cfg.vuMax) / rigPeak, 2.f)
      << "scaling changes nothing, so it cannot be what went missing";
}

TEST (CoronaScaling, SilenceStaysSilentAfterScaling)
{
  CoronaConfig cfg;
  EXPECT_FLOAT_EQ (coronaVuLevel (0.f, 0.f, cfg.vuMax), 0.f);
}

TEST (CoronaScaling, TheScalingIsActuallyWiredIn)
{
  // Three times in three days a coupling was lost in a rebuild while every
  // test stayed green, because the tests checked the piece and nobody checked
  // that the piece was still plugged in: the coloured band drawn off the bolt
  // core, the trajectory's depth left out of the line map, and this one.
  //
  // coronaVuLevel() had no caller at all outside its own tests. It is the only
  // place the blob's level is put on a scale, so with nothing calling it the
  // blob reacted to whatever the meter happened to read.
  juce::File const root (A3_UI_SOURCE_DIR);
  auto callers = 0;

  for (auto const &entry : juce::RangedDirectoryIterator (
           root, true, "*.cc", juce::File::findFiles))
    {
      auto const name = entry.getFile ().getFileName ();
      if (name == "CoronaScaling.cc")
        continue;

      if (entry.getFile ().loadFileAsString ().contains ("coronaVuLevel ("))
        ++callers;
    }

  EXPECT_GT (callers, 0)
      << "nothing scales the blob's VU; the raw meter reading reaches the "
         "shader";
}

TEST (CoronaScaling, TheCoronasOwnSizeIsWiredInToo)
{
  // The other half of the same break. coronaScaleFactor() is how far the
  // corona reaches for a given level, and it is the only part of this a skin
  // can set — sizeMin and sizeMax. With nothing calling it the shader carried
  // its own hard-wired ramp instead, so "too subtle" had no control to answer
  // it.
  juce::File const root (A3_UI_SOURCE_DIR);
  auto callers = 0;

  for (auto const &entry : juce::RangedDirectoryIterator (
           root, true, "*.cc", juce::File::findFiles))
    {
      if (entry.getFile ().getFileName () == "CoronaScaling.cc")
        continue;

      if (entry.getFile ().loadFileAsString ().contains ("coronaScaleFactor ("))
        ++callers;
    }

  EXPECT_GT (callers, 0)
      << "the corona's reach is hard-wired in the shader; sizeMin/sizeMax "
         "reach nothing";
}

TEST (CoronaScaling, TheCoronaReachesAtLeastAsFarAsTheRampItReplaced)
{
  // sizeMin/sizeMax described a ring drawn around a 2D disc. They now say how
  // far the shader's corona reaches, in the same unit -- blob radii -- but the
  // ramp they replaced ran 1.9 to 4.3, and the old ring numbers (1.1 to 2.2)
  // would have made the blob quieter while answering a complaint that it was
  // too quiet.
  CoronaConfig cfg;

  EXPECT_GE (coronaScaleFactor (0.f, cfg), 1.9f) << "a silent blob shrank";
  EXPECT_GE (coronaScaleFactor (1.f, cfg), 4.3f) << "a loud blob lost its reach";
}

TEST (CoronaScaling, TheCoronaGrowsEnoughToBeRead)
{
  // "wenns nur die blitze sind ist es zu subtil" -- the reach has to be worth
  // looking at across the range real material covers, not merely non-zero.
  CoronaConfig cfg;
  auto const quiet = coronaScaleFactor (0.f, cfg);
  auto const loud = coronaScaleFactor (1.f, cfg);

  EXPECT_GT (loud / quiet, 1.5f) << "a corona that barely moves says nothing";
}
