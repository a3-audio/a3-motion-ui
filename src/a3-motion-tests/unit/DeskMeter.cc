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

#include <a3-motion-ui/components/DeskMeter.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

using namespace a3;

// The desk's channel meter, written out from a3-mixer (a3_mixer_meters.py,
// display_panel.py, decided 2026-10-07): LED n lights once the peak reaches
// its threshold, four green, two yellow, two red; a display's bar fills
// exactly n/8 for n LEDs and carries ticks at 4/8 and 6/8.
namespace
{
constexpr float desk[8] = { -36.f, -24.f, -18.f, -12.f, -9.f, -6.f, -3.f, 0.f };

int
deskLeds (float db)
{
  auto lit = 0;
  for (auto threshold : desk)
    lit += db >= threshold ? 1 : 0;
  return lit;
}

float
amplitudeAt (float db)
{
  return std::pow (10.f, db / 20.f);
}

juce::Rectangle<int> const column{ 0, 0, 40, 240 }; // FULL: a column, foot down
juce::Rectangle<int> const row{ 0, 0, 240, 40 };    // FPV: a row, foot left

juce::Image
painted (VuReading reading, juce::Rectangle<int> bounds, VuDirection direction)
{
  juce::Image image (juce::Image::ARGB, bounds.getRight (), bounds.getBottom (),
                     true);
  juce::Graphics g (image);
  paintDeskMeter (g, bounds, reading, direction);
  return image;
}

int
pixelsOf (juce::Image const &image, juce::Colour colour)
{
  auto count = 0;
  for (int y = 0; y < image.getHeight (); ++y)
    for (int x = 0; x < image.getWidth (); ++x)
      count += image.getPixelAt (x, y) == colour ? 1 : 0;
  return count;
}

/** How far along the travel the bar reaches, in pixels from the foot. */
int
reach (DeskMeterGeometry const &m, VuDirection direction)
{
  if (m.bar.isEmpty ())
    return 0;
  return direction == VuDirection::Up ? m.well.getBottom () - m.bar.getY ()
                                      : m.bar.getRight () - m.well.getX ();
}

/** Where a mark stands along the travel, in pixels from the foot. */
int
markAt (DeskMeterGeometry const &m, int mark, VuDirection direction)
{
  auto const &r = m.marks[static_cast<std::size_t> (mark)];
  return direction == VuDirection::Up ? m.well.getBottom () - r.getBottom ()
                                      : r.getX () - m.well.getX ();
}
}

TEST (DeskMeter, EveryLevelLightsWhatTheDeskLights)
{
  for (auto db = -60.f; db <= 3.f; db += 0.25f)
    {
      EXPECT_EQ (deskLedsLit (amplitudeAt (db)), deskLeds (db)) << db;
      EXPECT_FLOAT_EQ (deskBarFraction (amplitudeAt (db)),
                       static_cast<float> (deskLeds (db)) / 8.f)
          << db;
    }
  EXPECT_EQ (deskLedsLit (0.f), 0);
  EXPECT_FLOAT_EQ (deskBarFraction (0.f), 0.f);
}

TEST (DeskMeter, TheLedsWearTheDesksColours)
{
  for (int i = 0; i < 4; ++i)
    EXPECT_EQ (deskLedBands[static_cast<std::size_t> (i)], vuGreenBand) << i;
  EXPECT_EQ (deskLedBands[4], vuYellowBand);
  EXPECT_EQ (deskLedBands[5], vuYellowBand);
  EXPECT_EQ (deskLedBands[6], vuRedBand);
  EXPECT_EQ (deskLedBands[7], vuRedBand);
  EXPECT_FLOAT_EQ (deskMarkFractions[0], 0.5f);
  EXPECT_FLOAT_EQ (deskMarkFractions[1], 0.75f);
}

// The fault the maintainer saw on the rig: -5 dBFS is the desk's sixth LED,
// yellow; Motion painted red. In either direction.
TEST (DeskMeter, MinusFiveIsYellowAtItsHeadAndNeverRed)
{
  for (auto direction : { VuDirection::Up, VuDirection::Right })
    {
      auto const bounds = direction == VuDirection::Up ? column : row;
      auto const m = deskMeterGeometry (bounds, { amplitudeAt (-5.f), 0.f }, direction);
      EXPECT_FALSE (m.bands[vuYellowBand].isEmpty ());
      EXPECT_TRUE (m.bands[vuRedBand].isEmpty ());
      auto const image = painted ({ amplitudeAt (-5.f), amplitudeAt (-5.f) },
                                  bounds, direction);
      EXPECT_EQ (pixelsOf (image, vuBandColour (theme (), vuRedBand)), 0);
      EXPECT_GT (pixelsOf (image, vuBandColour (theme (), vuYellowBand)), 0);
    }
}

// The bar reaches the yellow mark exactly when LED 5 lights (-9 dBFS) and
// the red one when LED 7 does (-3 dBFS), as on the desk's displays.
TEST (DeskMeter, TheBarReachesAMarkExactlyWhenItsLedLights)
{
  for (auto direction : { VuDirection::Up, VuDirection::Right })
    {
      auto const bounds = direction == VuDirection::Up ? column : row;
      auto const at = [&] (float db) {
        return deskMeterGeometry (bounds, { amplitudeAt (db), 0.f }, direction);
      };
      EXPECT_LE (reach (at (-9.5f), direction), markAt (at (-9.5f), 0, direction));
      EXPECT_GT (reach (at (-9.f), direction), markAt (at (-9.f), 0, direction));
      EXPECT_LE (reach (at (-3.5f), direction), markAt (at (-3.5f), 1, direction));
      EXPECT_GT (reach (at (-3.f), direction), markAt (at (-3.f), 1, direction));
    }
}

TEST (DeskMeter, TheMarksStandBesideTheBarNotOnIt)
{
  for (auto direction : { VuDirection::Up, VuDirection::Right })
    {
      auto const bounds = direction == VuDirection::Up ? column : row;
      auto const m = deskMeterGeometry (bounds, { 1.f, 1.f }, direction);
      for (auto const &mark : m.marks)
        {
          EXPECT_FALSE (mark.isEmpty ());
          EXPECT_TRUE (bounds.contains (mark));
          EXPECT_FALSE (mark.intersects (m.well));
        }
    }
}

TEST (DeskMeter, SilenceIsAnEmptyWellWithItsMarks)
{
  auto const m = deskMeterGeometry (row, {}, VuDirection::Right);
  EXPECT_TRUE (m.bar.isEmpty ());
  EXPECT_TRUE (m.hold.isEmpty ());
  EXPECT_FALSE (m.well.isEmpty ());
  auto const image = painted ({}, row, VuDirection::Right);
  for (auto band : { vuGreenBand, vuYellowBand, vuRedBand })
    EXPECT_EQ (pixelsOf (image, vuBandColour (theme (), band)), 0) << band;
  EXPECT_GT (pixelsOf (image, toColour (theme ().textMuted)), 0) << "the marks";
}

// The held peak is a line where a bar at the held level would end; inside
// the bar it would mark nothing.
TEST (DeskMeter, TheHeldPeakIsALineAboveTheBar)
{
  auto const m = deskMeterGeometry (
      column, { amplitudeAt (-40.f), amplitudeAt (-7.f) }, VuDirection::Up);
  ASSERT_FALSE (m.hold.isEmpty ());
  auto const fiveEighths = m.well.getBottom ()
                           - juce::roundToInt (m.well.getHeight () * 5.f / 8.f);
  EXPECT_EQ (m.hold.getY (), fiveEighths);
  EXPECT_LT (m.hold.getHeight (), m.well.getHeight () / 8);

  auto const level = amplitudeAt (-7.f);
  EXPECT_TRUE (deskMeterGeometry (column, { level, level }, VuDirection::Up)
                   .hold.isEmpty ());
}

TEST (DeskMeter, AnOverIsHatched)
{
  EXPECT_FALSE (deskMeterClips ({ 1.f, 1.f }));
  EXPECT_TRUE (deskMeterClips ({ 1.f, 1.2f }));
  auto const clean = painted ({ 1.f, 1.f }, column, VuDirection::Up);
  auto const over = painted ({ 1.f, 1.2f }, column, VuDirection::Up);
  auto const red = vuBandColour (theme (), vuRedBand);
  EXPECT_LT (pixelsOf (over, red), pixelsOf (clean, red));
  EXPECT_GT (pixelsOf (over, red), 0);
}

TEST (DeskMeter, AnEmptyBoxPaintsNothing)
{
  juce::Image image (juce::Image::ARGB, 4, 4, true);
  juce::Graphics g (image);
  paintDeskMeter (g, {}, { 1.f, 1.f });
  EXPECT_EQ (image.getPixelAt (1, 1), juce::Colour ());
}

// The copy above against the desk's own code, where a3-mixer is checked out
// beside this repository: its functions run (python3) over the same sweep,
// and the displays' mark length is read from display_panel.py. Skipped
// without a checkout.
TEST (DeskMeter, TheDesksOwnCodeAgrees)
{
  auto const ui = juce::File (A3_UI_SOURCE_DIR).getParentDirectory ().getParentDirectory ();
  juce::File scripts;
  for (auto const *up : { "../../a3-mixer", "../../../../a3-mixer", "../a3-mixer" })
    {
      auto const candidate = ui.getChildFile (up).getChildFile ("software/scripts");
      if (candidate.getChildFile ("a3_mixer_meters.py").existsAsFile ())
        {
          scripts = candidate;
          break;
        }
    }
  if (scripts == juce::File ())
    GTEST_SKIP () << "no a3-mixer checkout beside " << ui.getFullPathName ();

  juce::String const program
      = "import sys; sys.path.insert(0, sys.argv[1]); import a3_mixer_meters as m; "
        "print(m.YELLOW_FROM_FRACTION, m.RED_FROM_FRACTION, len(m.CHANNEL_LED_THRESHOLDS_DB), "
        "' '.join(str(t) for t in m.CHANNEL_LED_THRESHOLDS_DB), ' '.join(m.CHANNEL_LED_COLOURS), "
        "' '.join(repr(m.bar_fraction(d / 4.0)) for d in range(-240, 13)))";
  juce::ChildProcess python;
  ASSERT_TRUE (python.start (juce::StringArray{ "python3", "-I", "-c", program,
                                                scripts.getFullPathName () }));
  auto const out = python.readAllProcessOutput ();
  python.waitForProcessToFinish (5000);
  juce::StringArray words;
  words.addTokens (out, " \n", "");
  words.removeEmptyStrings ();
  ASSERT_EQ (words.size (), 3 + 8 + 8 + 253) << out;

  EXPECT_FLOAT_EQ (words[0].getFloatValue (), deskMarkFractions[0]);
  EXPECT_FLOAT_EQ (words[1].getFloatValue (), deskMarkFractions[1]);
  EXPECT_EQ (words[2].getIntValue (), deskLedCount);
  for (int i = 0; i < 8; ++i)
    EXPECT_FLOAT_EQ (words[3 + i].getFloatValue (),
                     deskLedThresholdsDb[static_cast<std::size_t> (i)]) << i;
  char const *bandName[] = { "green", "yellow", "red" };
  for (int i = 0; i < 8; ++i)
    EXPECT_EQ (words[11 + i], bandName[deskLedBands[static_cast<std::size_t> (i)]]) << i;
  for (int d = -240; d <= 12; ++d)
    EXPECT_FLOAT_EQ (words[19 + d + 240].getFloatValue (),
                     deskBarFraction (amplitudeAt (static_cast<float> (d) / 4.f)))
        << static_cast<float> (d) / 4.f << " dBFS";

  auto const panel = scripts.getChildFile ("a3-mixer-set-display/display_panel.py")
                         .loadFileAsString ();
  EXPECT_FLOAT_EQ (panel.fromFirstOccurrenceOf ("\nMARK_OF_METER = ", false, false)
                       .upToFirstOccurrenceOf ("\n", false, false)
                       .getFloatValue (),
                   deskMarkOfMeter);
}
