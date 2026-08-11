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

#include <a3-motion-ui/components/SpeakerLightScaling.hh>

using namespace a3;

namespace
{

// Captured from the running rig via `oscdump 7772` — 574 messages over six
// seconds of programme material. The spread between the loudest and quietest
// speaker is the whole point of the display and has to survive to the output.
// Only the rms is used: peaks on this rig run a crest factor around 3, so a
// peak-driven beam chases transients instead of holding still long enough to
// be compared against its neighbours.
//
// An earlier capture put these an order of magnitude lower, which is how
// `vuMax` ended up at 0.025 — far under the real signal range, so every
// speaker clipped against it for most of the time and they all read equally
// bright. Re-measure these before trusting them again.
constexpr float loudestRms = 0.0763f;  // /vu/8, mean
constexpr float quietestRms = 0.0165f; // /vu/6, mean
constexpr float loudestRmsPeak = 0.1869f; // /vu/8, highest single message

struct ShippedParams
{
  float vuMax, curve, beamIntensity;
};

ShippedParams
shippedParams ()
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  EXPECT_TRUE (file.existsAsFile ()) << "no config.json at " << A3_CONFIG_JSON_PATH;

  // Keep the parsed var alive — binding a reference straight to
  // JSON::parse(...)["key"] dangles once the temporary dies.
  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &speakerLight = parsed["speakerLight"];

  // Both must be set explicitly — falling back to a default here is what made
  // the beams indistinguishable in the first place.
  EXPECT_TRUE (speakerLight.hasProperty ("vuMax"));
  EXPECT_TRUE (speakerLight.hasProperty ("curve"));

  return { static_cast<float> (speakerLight["vuMax"]),
           static_cast<float> (speakerLight["curve"]),
           static_cast<float> (speakerLight["beamIntensity"]) };
}

TEST (SpeakerLightScaling, ShippedConfigSeparatesLoudAndQuietSpeakers)
{
  auto const params = shippedParams ();

  auto const high = speakerLightLevel (loudestRms, params.vuMax, params.curve);
  auto const low = speakerLightLevel (quietestRms, params.vuMax, params.curve);

  // The perceptual exponent compresses the input spread; below roughly 3x the
  // beams read as equally bright on screen.
  EXPECT_GT (high / low, 3.f)
      << "speaker levels " << high << " and " << low << " are too close";
}

// beamConeExp and beamFalloff were dead for a while: loaded from the config,
// pushed into uniforms, never read by the shader. They are wired up now, so a
// config that drops them would silently fall back to the defaults again.
TEST (SpeakerLightScaling, ShippedConfigSetsBeamShapeParameters)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &speakerLight = parsed["speakerLight"];

  EXPECT_TRUE (speakerLight.hasProperty ("beamConeExp"));
  EXPECT_TRUE (speakerLight.hasProperty ("beamFalloff"));
  EXPECT_TRUE (speakerLight.hasProperty ("beamIntensity"));

  // An exponent of 1 leaves the cone at its full ~67 degrees, which makes the
  // four beams overlap into a single glow.
  EXPECT_GT (static_cast<float> (speakerLight["beamConeExp"]), 1.f);
}

// The four speakers sit 90 degrees apart, so at full level the cones should
// just touch: a half-angle of 45 degrees. Wider than that and they overlap
// into one another, which is what made them unreadable.
TEST (SpeakerLightScaling, ShippedConfigConesJustTouchAtFullLevel)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &speakerLight = parsed["speakerLight"];

  ASSERT_TRUE (speakerLight.hasProperty ("widthStart"));
  ASSERT_TRUE (speakerLight.hasProperty ("widthEnd"));

  auto const widthStart = static_cast<float> (speakerLight["widthStart"]);
  auto const widthEnd = static_cast<float> (speakerLight["widthEnd"]);

  EXPECT_LT (widthStart, widthEnd) << "cone must widen with level, not narrow";
  EXPECT_NEAR (beamHalfAngleDegrees (widthEnd), 45.f, 1.f);
  EXPECT_LT (beamHalfAngleDegrees (widthStart), 45.f);
}

TEST (SpeakerLightScaling, ShippedConfigKeepsLoudestSpeakerBright)
{
  auto const params = shippedParams ();

  // Contrast alone is not enough — a steep curve with a high vuMax separates
  // the speakers but leaves all of them nearly black. What reaches the screen
  // is the level scaled by beamIntensity, so asserting on the level alone lets
  // a dimmed beamIntensity pass a test that is meant to guard brightness.
  auto const level
      = speakerLightLevel (loudestRms, params.vuMax, params.curve);

  EXPECT_GT (level * params.beamIntensity, 0.25f);
}

// The failure mode this guards against: vuMax below the actual signal range
// pins the level at 1.0 for most of the time, so the beams sit at the stop and
// only dip in the gaps — indistinguishable from each other, and reading like a
// peak meter even though they are fed rms.
TEST (SpeakerLightScaling, ShippedConfigLeavesHeadroomAboveTheLoudestSpeaker)
{
  auto const params = shippedParams ();

  EXPECT_LT (speakerLightLevel (loudestRmsPeak, params.vuMax, params.curve),
             1.f)
      << "vuMax " << params.vuMax << " is below the measured rms range";
}

// A step up settles to 1 - 1/e of the way after one time constant. Expressing
// the smoothing in seconds rather than per-frame coefficients keeps it
// independent of the frame rate.
TEST (SpeakerLightScaling, EnvelopeRisesWithTheAttackTimeConstant)
{
  auto constexpr attack = 0.08f;
  auto constexpr decay = 0.4f;

  EXPECT_NEAR (speakerLightEnvelope (0.f, 1.f, attack, decay, attack), 0.632f,
               0.01f);
}

TEST (SpeakerLightScaling, EnvelopeFallsWithTheDecayTimeConstant)
{
  auto constexpr attack = 0.08f;
  auto constexpr decay = 0.4f;

  // A fast attack next to a slow decay: picking the wrong branch here is what
  // turns an rms-driven envelope back into a peak follower.
  EXPECT_NEAR (speakerLightEnvelope (1.f, 0.f, attack, decay, decay), 0.368f,
               0.01f);
}

// The beams are meant to hold still long enough to be compared against each
// other, which needs a decay slower than the attack.
TEST (SpeakerLightScaling, ShippedConfigHoldsTheBeamsLongerThanItRaisesThem)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &speakerLight = parsed["speakerLight"];

  ASSERT_TRUE (speakerLight.hasProperty ("attack"));
  ASSERT_TRUE (speakerLight.hasProperty ("decay"));

  EXPECT_LT (static_cast<float> (speakerLight["attack"]),
             static_cast<float> (speakerLight["decay"]));
}

}
