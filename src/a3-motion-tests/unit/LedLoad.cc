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

#include <a3-motion-ui/io/LedLoad.hh>

using namespace a3;

// The app keeps the same estimate as the controller's firmware, so a picture
// the firmware would dim shows up in the app's log first (#21). These pin the
// estimate to the firmware's numbers: 1 mA a chip, 20 mA a colour at full.

TEST (LedLoad, DarkKeysDrawOnlyWhatTheChipsNeed)
{
  LedLoad load (44);
  EXPECT_EQ (load.estimateMilliamps (), 44u);
}

TEST (LedLoad, AFullWhiteKeyAddsSixtyMilliamps)
{
  LedLoad load (44);
  load.set (3, juce::Colours::white);
  EXPECT_EQ (load.estimateMilliamps (), 44u + 60u);
}

TEST (LedLoad, AKeyToldAgainIsReplacedNotAdded)
{
  LedLoad load (44);
  load.set (3, juce::Colours::white);
  load.set (3, juce::Colour (0, 0, 0));
  EXPECT_EQ (load.estimateMilliamps (), 44u);
}

TEST (LedLoad, TheIdleColourOnEveryKeyIsWhatTheIssueEstimated)
{
  // config.json's idle colour, (31, 214, 205): 450 of 765, 35 mA a key.
  LedLoad load (44);
  for (std::size_t led = 0; led < 44; ++led)
    load.set (led, juce::Colour (31, 214, 205));
  EXPECT_EQ (load.estimateMilliamps (), 44u + 44u * 450u * 20u / 255u);
}

TEST (LedLoad, AnIdOutsideTheStripIsIgnored)
{
  LedLoad load (44);
  load.set (44, juce::Colours::white);
  load.set (200, juce::Colours::white);
  EXPECT_EQ (load.estimateMilliamps (), 44u);
}

TEST (LedLoad, ThePeakStaysWhenTheLightGoesDown)
{
  LedLoad load (44);
  load.set (0, juce::Colours::white);
  load.set (1, juce::Colours::white);
  load.set (1, juce::Colour (0, 0, 0));
  EXPECT_EQ (load.peakMilliamps (), 44u + 120u);
  EXPECT_EQ (load.estimateMilliamps (), 44u + 60u);
}

TEST (LedLoad, OnlyAPeakWorthALineIsTaken)
{
  LedLoad load (44);
  load.set (0, juce::Colours::white);
  EXPECT_TRUE (load.takeNewPeak (10));
  EXPECT_FALSE (load.takeNewPeak (10)) << "the same peak is not news";

  load.set (1, juce::Colour (0, 0, 60)); // about 5 mA more
  EXPECT_FALSE (load.takeNewPeak (10));

  load.set (2, juce::Colours::white);
  EXPECT_TRUE (load.takeNewPeak (10));
}

// Over the firmware's budget, the controller dims every key. The app says so
// in its log once each time a picture crosses it -- so a skin that is too
// bright shows up as a line, not as keys that look dull for no reason.
TEST (LedLoad, APictureOverTheFirmwareBudgetIsSaidOncePerCrossing)
{
  LedLoad load (44);
  EXPECT_FALSE (load.takeBudgetCrossing ());

  for (std::size_t led = 0; led < 44; ++led)
    load.set (led, juce::Colours::white);
  EXPECT_GT (load.estimateMilliamps (), firmwareLedBudgetMa);
  EXPECT_TRUE (load.takeBudgetCrossing ());
  EXPECT_FALSE (load.takeBudgetCrossing ()) << "still over is not news";

  for (std::size_t led = 0; led < 44; ++led)
    load.set (led, juce::Colour (0, 0, 0));
  EXPECT_FALSE (load.takeBudgetCrossing ());

  for (std::size_t led = 0; led < 44; ++led)
    load.set (led, juce::Colours::white);
  EXPECT_TRUE (load.takeBudgetCrossing ()) << "a second crossing is news again";
}

TEST (LedLoad, TheRestingPictureIsUnderTheBudget)
{
  // The highest the app showed in a full session on 2026-09-26: 1144.
  EXPECT_LT (1144u, firmwareLedBudgetMa);
}
