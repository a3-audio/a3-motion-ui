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

// These were dead for a while: loaded from the config, pushed into uniforms,
// never read by the shader. They are wired up now, so a config that drops them
// would silently fall back to the defaults again.
TEST (SpeakerLightScaling, ShippedConfigSetsBeamShapeParameters)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &speakerLight = parsed["speakerLight"];

  EXPECT_TRUE (speakerLight.hasProperty ("edgeSoftness"));
  EXPECT_TRUE (speakerLight.hasProperty ("beamFalloff"));
  EXPECT_TRUE (speakerLight.hasProperty ("beamIntensity"));
  EXPECT_TRUE (speakerLight.hasProperty ("absorb"));
  EXPECT_TRUE (speakerLight.hasProperty ("innerIntensity"));

  // A softness of 1 has no soft edge at all; 0 fades from the axis outwards
  // and undoes the flat top.
  auto const softness = static_cast<float> (speakerLight["edgeSoftness"]);
  EXPECT_GT (softness, 0.f);
  EXPECT_LT (softness, 1.f);
}

// The four speakers sit 90 degrees apart, so at full level the cones should
// just touch: a half-angle of 45 degrees. Wider than that and they overlap
// into one another, which is what made them unreadable.
// The coverage angle is a property of the loudspeaker, so it is fixed rather
// than growing with level — the level drives brightness alone. This replaces
// the old widthStart/widthEnd pair, whose 45 degree half-angle at full level
// made neighbouring cones touch.
TEST (SpeakerLightScaling, ShippedConfigSetsTheCoverageAngle)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &speakerLight = parsed["speakerLight"];

  ASSERT_TRUE (speakerLight.hasProperty ("coverageAngle"));

  auto const coverage = static_cast<float> (speakerLight["coverageAngle"]);

  EXPECT_GT (coverage, 0.f);
  EXPECT_LT (coverage, 180.f);
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

// ── Beam geometry ───────────────────────────────────────────────────────
//
// The beam used to be an angular cone whose apex was a mathematical point at
// the speaker's centre — inside the icon, and infinitely thin there, so it
// looked like it grew through the loudspeaker rather than out of it. It is now
// a truncated cone that starts at the horn's mouth with the mouth's width.

TEST (SpeakerLightGeometry, BeamStartsExactlyAtTheApertureWidth)
{
  EXPECT_FLOAT_EQ (beamHalfWidthAt (0.f, speakerApertureHalfWidth, 1.f),
                   speakerApertureHalfWidth);
}

// Coverage angles are quoted the way a loudspeaker's are: the full angle the
// cone opens to, not the half-angle off its axis.
TEST (SpeakerLightGeometry, CoverageAngleIsTheFullConeNotTheHalf)
{
  EXPECT_NEAR (beamHalfAngleDegrees (coneWidthFromCoverageAngle (70.f)), 35.f,
               0.01f);
}

// Having the aperture width at s=0 is not enough on its own — a profile that
// peaks on the axis and falls off still *looks* like it starts as a point. The
// mouth has to be lit across its whole width.
TEST (SpeakerLightGeometry, BeamIsAtFullBrightnessAcrossTheWholeMouth)
{
  auto constexpr halfWidth = 0.2f;
  auto constexpr softness = 0.7f;

  EXPECT_FLOAT_EQ (beamProfile (0.f, halfWidth, softness), 1.f);
  EXPECT_FLOAT_EQ (beamProfile (halfWidth * 0.5f, halfWidth, softness), 1.f);
}

TEST (SpeakerLightGeometry, BeamFadesOutAtItsEdge)
{
  auto constexpr halfWidth = 0.2f;
  auto constexpr softness = 0.7f;

  EXPECT_FLOAT_EQ (beamProfile (halfWidth, halfWidth, softness), 0.f);
  EXPECT_GT (beamProfile (halfWidth * 0.85f, halfWidth, softness), 0.f);
  EXPECT_LT (beamProfile (halfWidth * 0.85f, halfWidth, softness), 1.f);
}

TEST (SpeakerLightGeometry, BeamCastsNoLightBehindTheMouth)
{
  EXPECT_FLOAT_EQ (beamHalfWidthAt (-0.1f, speakerApertureHalfWidth, 1.f), 0.f);
}

// widthEnd 0.2929 is the 45 degree half-angle where neighbouring cones touch,
// and 45 degrees is exactly one unit of spread per unit of travel.
TEST (SpeakerLightGeometry, FullWidthSpreadsAtFortyFiveDegrees)
{
  EXPECT_NEAR (beamSpreadTangent (0.2929f), 1.f, 0.001f);
}

TEST (SpeakerLightGeometry, MouthSitsOutsideTheSphere)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const speakerRadius
      = static_cast<float> (parsed["speakerLight"]["speakerRadius"]);

  // A mouth at or inside the surface would start the beam in the middle of the
  // volume it is supposed to be entering.
  EXPECT_GT (speakerMouthRadius (speakerRadius), 1.f);
}

// The sphere is translucent, so the beam is not blocked — it is absorbed on
// its way through. Path length is what drives that.
TEST (SpeakerLightGeometry, OnAxisPathCrossesTheFullDiameter)
{
  auto constexpr mouthRadius = 1.3f;

  // Far side of the sphere, straight down the axis.
  EXPECT_NEAR (beamPathInsideSphere (mouthRadius + 1.f, 0.f, mouthRadius), 2.f,
               0.001f);
}

TEST (SpeakerLightGeometry, PathIsHalfTheDiameterAtTheCentre)
{
  auto constexpr mouthRadius = 1.3f;

  EXPECT_NEAR (beamPathInsideSphere (mouthRadius, 0.f, mouthRadius), 1.f,
               0.001f);
}

TEST (SpeakerLightGeometry, RayPassingBesideTheSphereIsNeverAbsorbed)
{
  auto constexpr mouthRadius = 1.3f;

  // Far enough off-axis that the ray misses the unit circle entirely.
  EXPECT_FLOAT_EQ (beamPathInsideSphere (mouthRadius, 2.f, mouthRadius), 0.f);
}

TEST (SpeakerLightGeometry, AbsorptionLeavesTheFarSideDimmerThanTheNearSide)
{
  auto constexpr coefficient = 1.5f;

  EXPECT_FLOAT_EQ (beamAbsorption (0.f, coefficient), 1.f);
  EXPECT_LT (beamAbsorption (2.f, coefficient),
             beamAbsorption (0.5f, coefficient) * 0.5f);
}

// Weight for the volume lighting: how much sphere a view ray traverses. The
// speakers all sit in the z=0 plane, so the ray crosses the beam's densest
// plane exactly at its own position and one sample per speaker is enough.
TEST (SpeakerLightGeometry, TraversalIsThickestAtTheCentreAndZeroAtTheRim)
{
  EXPECT_FLOAT_EQ (sphereHalfChord (0.f), 1.f);
  EXPECT_FLOAT_EQ (sphereHalfChord (1.f), 0.f);
  EXPECT_NEAR (sphereHalfChord (0.6f), 0.8f, 0.001f);
}

// Measured on the running rig via `oscdump 7774`: the subwoofer sits an order
// of magnitude below the shipped vuMax of 0.3, which left the corona at a
// level of 0.016 — invisible. Same failure the speaker beams had.
constexpr float subwooferRmsMean = 0.0129f;
constexpr float subwooferRmsMax = 0.0237f;

TEST (SphereCorona, ShippedConfigMakesTheCoronaVisible)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &glow = parsed["sphereGlow"];

  ASSERT_TRUE (glow.hasProperty ("vuMax"));
  ASSERT_TRUE (glow.hasProperty ("intensity"));

  auto const vuMax = static_cast<float> (glow["vuMax"]);
  auto const curve = static_cast<float> (glow["curve"]);
  auto const intensity = static_cast<float> (glow["intensity"]);

  EXPECT_GT (speakerLightLevel (subwooferRmsMean, vuMax, curve) * intensity,
             0.1f)
      << "vuMax " << vuMax << " is far above the subwoofer's actual range";
}

TEST (SphereCorona, ShippedConfigLeavesHeadroomAboveTheSubwoofer)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &glow = parsed["sphereGlow"];

  EXPECT_LT (speakerLightLevel (subwooferRmsMax,
                                static_cast<float> (glow["vuMax"]),
                                static_cast<float> (glow["curve"])),
             1.f);
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
