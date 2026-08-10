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

// One frame captured from a live system via `oscdump 7772`. The four speakers
// span a 4.5x range at the input — that spread is the whole point of the
// display and has to survive to the output.
struct SpeakerVu
{
  float peak, rms;
};
constexpr SpeakerVu loudest{ 0.073298f, 0.019750f };  // /vu/5
constexpr SpeakerVu quietest{ 0.016308f, 0.004394f }; // /vu/7

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
