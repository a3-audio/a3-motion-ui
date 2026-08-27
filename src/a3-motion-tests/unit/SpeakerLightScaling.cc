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

#include <a3-motion-ui/components/SpeakerLightScaling.hh>

#include <cmath>

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
  // Keep the parsed var alive — binding a reference straight to
  // JSON::parse(...)["key"] dangles once the temporary dies.
  auto const parsed = shippedSkin ();
  EXPECT_FALSE (parsed.isVoid ()) << "no skin to check";
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
  auto const parsed = shippedSkin ();
  ASSERT_FALSE (parsed.isVoid ()) << "no skin to check";
  auto const &speakerLight = parsed["speakerLight"];

  EXPECT_TRUE (speakerLight.hasProperty ("edgeSoftness"));
  EXPECT_TRUE (speakerLight.hasProperty ("beamIntensity"));
  EXPECT_TRUE (speakerLight.hasProperty ("boltWidth"));
  EXPECT_TRUE (speakerLight.hasProperty ("root"));

  // A softness of 1 has no soft edge at all; 0 fades from the axis outwards
  // and undoes the flat top.
  auto const softness = static_cast<float> (speakerLight["edgeSoftness"]);
  EXPECT_GT (softness, 0.f);
  EXPECT_LT (softness, 1.f);
}

TEST (SpeakerLightScaling, ShippedConfigKeepsLoudestSpeakerBright)
{
  auto const params = shippedParams ();

  // What reaches the screen is the level scaled by beamIntensity, so asserting
  // on the level alone lets a dimmed beamIntensity pass a test that is meant
  // to guard brightness.
  //
  // The threshold used to be 0.25, from when the beams were the display. They
  // are an indicator now — the energy map carries the spatial information —
  // so the bar is that they read at all without drowning the net that is
  // supposed to look like it comes out of them.
  auto const level
      = speakerLightLevel (loudestRms, params.vuMax, params.curve);
  auto const onScreen = level * params.beamIntensity;

  EXPECT_GT (onScreen, 0.08f);
  EXPECT_LT (onScreen, 0.6f);
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

// Angles are measured off the beam's axis, so 70 means 70 degrees to either
// side and 140 across.
TEST (SpeakerLightGeometry, AngleAndWidthAreInverses)
{
  EXPECT_NEAR (beamHalfAngleDegrees (coneWidthFromAngle (70.f)), 70.f, 0.01f);
  EXPECT_NEAR (beamHalfAngleDegrees (coneWidthFromAngle (26.f)), 26.f, 0.01f);
}

// The cone widens with level again: a loudspeaker's dispersion is fixed, but
// this is a picture of how much sound is coming out, and a quiet beam that
// stays as wide as a loud one carries no information in its shape.
TEST (SpeakerLightGeometry, ConeWidensFromTheQuietAngleToTheLoudOne)
{
  auto constexpr quiet = 26.f;
  auto constexpr loud = 70.f;

  EXPECT_FLOAT_EQ (beamAngleAtLevel (0.f, quiet, loud), quiet);
  EXPECT_FLOAT_EQ (beamAngleAtLevel (1.f, quiet, loud), loud);
  EXPECT_FLOAT_EQ (beamAngleAtLevel (0.5f, quiet, loud), 48.f);
}

TEST (SpeakerLightGeometry, SpreadStaysFiniteAtAGrazingAngle)
{
  // tan blows up at 90 degrees; a config typo must not take the beam with it.
  EXPECT_TRUE (std::isfinite (beamSpreadTangent (coneWidthFromAngle (90.f))));
  EXPECT_TRUE (std::isfinite (beamSpreadTangent (coneWidthFromAngle (120.f))));
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
  auto const parsed = shippedSkin ();
  ASSERT_FALSE (parsed.isVoid ()) << "no skin to check";
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
  auto const parsed = shippedSkin ();
  ASSERT_FALSE (parsed.isVoid ()) << "no skin to check";
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
  auto const parsed = shippedSkin ();
  ASSERT_FALSE (parsed.isVoid ()) << "no skin to check";
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
  auto const parsed = shippedSkin ();
  ASSERT_FALSE (parsed.isVoid ()) << "no skin to check";
  auto const &speakerLight = parsed["speakerLight"];

  ASSERT_TRUE (speakerLight.hasProperty ("attack"));
  ASSERT_TRUE (speakerLight.hasProperty ("decay"));

  EXPECT_LT (static_cast<float> (speakerLight["attack"]),
             static_cast<float> (speakerLight["decay"]));
}


// ── how big the sphere may get ──────────────────────────────────────────
//
// The sphere's scale and the speaker radius have to be picked together: past a
// point the icons run off the shorter edge. This was a comment next to a
// constexpr; it is a config value now, so it needs a guard.
//
// The geometry the guard has to match (MotionComponent::paintOverChildren):
// the four speakers sit at 45, 135, 225 and 315 degrees — on the diagonals,
// not on the axes — each drawn as a speakerIconSize square rotated to face the
// centre. So an icon's distance from a vertical screen edge is
// cos(45) * speakerRadius, not speakerRadius, and the rotated square reaches
// cos(45) * speakerIconSize beyond its own centre along that same axis.
//
// The transform (updateBoundsAndTransform) maps normalised 1.0 onto
// shorterSide * sphereScale / 2 pixels, so "on screen" is sphereScale times
// the normalised extent staying under 1.

TEST (SphereScale, TheLimitFollowsTheDiagonalPlacementOfTheSpeakers)
{
  // Treating a speaker as if it sat on the axis at its full radius caps the
  // scale at 1 / (1.4 + 0.198) = 0.625 and rejects sizes that are visibly
  // fine: measured on the running app at 0.72, the outermost 50 px on either
  // side are background and nothing is clipped.
  EXPECT_TRUE (speakerIconsFitOnScreen (0.72f, 1.4f));

  // cos(45) * (1.4 + 0.28) = 1.188, so the real limit is 1 / 1.188.
  EXPECT_TRUE (speakerIconsFitOnScreen (0.84f, 1.4f));
  EXPECT_FALSE (speakerIconsFitOnScreen (0.85f, 1.4f));
}

// The sphere's size is a skin value, edited in the skin editor like the rest
// of the skin — there is no preset list guarding the range any more. This
// pins the span a skin may reasonably use, so that a change to the icon
// geometry which narrows it shows up here rather than on the screen.
TEST (SphereScale, TheUsableRangeReachesFromSmallToJustOverFourFifths)
{
  for (auto scale : { 0.45f, 0.54f, 0.62f, 0.72f, 0.82f })
    EXPECT_TRUE (speakerIconsFitOnScreen (scale, 1.4f)) << "scale " << scale;
}

TEST (SphereScale, IconsFitAtTheShippedScale)
{
  auto const parsed = shippedSkin ();
  ASSERT_FALSE (parsed.isVoid ()) << "no skin to check";

  ASSERT_TRUE (parsed.hasProperty ("sphereScale"));

  EXPECT_TRUE (speakerIconsFitOnScreen (
      static_cast<float> (parsed["sphereScale"]),
      static_cast<float> (parsed["speakerLight"]["speakerRadius"])));
}

TEST (SphereScale, AnOversizedSphereIsRejected)
{
  EXPECT_FALSE (speakerIconsFitOnScreen (0.9f, 1.4f))
      << "at 0.9 the icons are clipped, which is how this was found";
}

TEST (SphereScale, PullingTheSpeakersInBuysRoom)
{
  EXPECT_FALSE (speakerIconsFitOnScreen (0.86f, 1.4f));
  EXPECT_TRUE (speakerIconsFitOnScreen (0.86f, 1.0f));
}

}
