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

// One frame captured from a live system via `oscdump 7772`, at the levels the
// rig actually runs at. The spread between the loudest and quietest speaker is
// the whole point of the display and has to survive to the output — in
// particular the loudest one must not clip against vuMax, which flattens it
// against its neighbours.
struct SpeakerVu
{
  float peak, rms;
};
constexpr SpeakerVu loudest{ 0.197768f, 0.015763f };  // /vu/8
constexpr SpeakerVu quietest{ 0.033785f, 0.002692f }; // /vu/6

struct ShippedParams
{
  float vuMax, curve;
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
           static_cast<float> (speakerLight["curve"]) };
}

TEST (SpeakerLightScaling, ShippedConfigSeparatesLoudAndQuietSpeakers)
{
  auto const params = shippedParams ();

  auto const high
      = speakerLightLevel (loudest.peak, loudest.rms, params.vuMax, params.curve);
  auto const low = speakerLightLevel (quietest.peak, quietest.rms, params.vuMax,
                                      params.curve);

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

TEST (SpeakerLightScaling, ShippedConfigKeepsLoudestSpeakerBright)
{
  auto const params = shippedParams ();

  // Contrast alone is not enough — a steep curve with a high vuMax separates
  // the speakers but leaves all of them nearly black.
  EXPECT_GT (
      speakerLightLevel (loudest.peak, loudest.rms, params.vuMax, params.curve),
      0.6f);
}

}
