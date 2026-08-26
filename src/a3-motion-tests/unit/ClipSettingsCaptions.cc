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

#include <a3-motion-ui/components/ClipSettingsCaptions.hh>

using namespace a3;

namespace
{

// Fitting every caption to its own box made "Q" three times the height of
// "end-action" standing next to it — a size difference that reads as a
// hierarchy which is not there. One size for all of them, and the tightest
// caption in the bar is the one that picks it.

// The numbers below are the bar at its shipped size: a section's content is
// ~200 px wide with a ~10 px gap between columns, and a control box is ~55 px
// tall. Measured from ClipSettingsComponent::paint() on a 1024x768 screen.
constexpr int contentWidth = 203;
constexpr int columnGap = 10;
constexpr int controlBoxHeight = 55;

float
widthOf (juce::String const &text, float size)
{
  return juce::GlyphArrangement::getStringWidth (
      juce::Font (juce::FontOptions (size)), text);
}

TEST (SharedCaptionSize, EveryCaptionFitsItsOwnColumn)
{
  auto const size = sharedCaptionSize (30.f, contentWidth, columnGap,
                                       controlBoxHeight);

  for (auto const &caption : captionTable)
    {
      auto const columnWidth
          = static_cast<float> ((contentWidth - (caption.columns - 1) * columnGap)
                                / caption.columns);

      EXPECT_LE (widthOf (caption.text, size), columnWidth)
          << caption.text << " is cut off at the shared size";
    }
}

// The point of a shared size is that it is shared — a caption that is drawn
// smaller than the rest is the bug this replaces.
TEST (SharedCaptionSize, TheNarrowestColumnDecidesForAll)
{
  // A tall box, so that width is the only thing deciding here — the height
  // cap has its own test below.
  auto const size = sharedCaptionSize (30.f, contentWidth, columnGap, 200);

  float tightest = std::numeric_limits<float>::max ();
  for (auto const &caption : captionTable)
    {
      auto const columnWidth
          = static_cast<float> ((contentWidth - (caption.columns - 1) * columnGap)
                                / caption.columns);
      auto const width = widthOf (caption.text, 30.f);
      if (width > 0.f)
        tightest = juce::jmin (tightest, 30.f * columnWidth / width);
    }

  EXPECT_NEAR (size, juce::jmin (30.f, tightest), 0.5f);
}

// The Font Size setting has to still mean something: a bar with room to spare
// draws its captions at the size the theme asks for, not at the fitted one.
TEST (SharedCaptionSize, ASmallerThemeSizeIsUsedAsIs)
{
  EXPECT_NEAR (sharedCaptionSize (11.f, contentWidth, columnGap,
                                  controlBoxHeight),
               11.f, 0.01f);
}

// A caption row is a fraction of its control box. In a short box the whole
// set has to come down together, or the one caption in the tightest box is
// silently shrunk again by drawFittedText.
TEST (SharedCaptionSize, AShortControlBoxCapsThemAll)
{
  auto const roomy = sharedCaptionSize (30.f, contentWidth, columnGap, 100);
  auto const cramped = sharedCaptionSize (30.f, contentWidth, columnGap, 20);

  EXPECT_LT (cramped, roomy);
  EXPECT_LE (cramped * 1.25f, 20.f * 0.5f + 0.01f)
      << "the caption row must not take more than half the box";
}

// The whole point of the Font Size setting is that it changes something. One
// shared size is only worth having if the largest setting still comes out
// visibly larger than the default in the bar the app actually ships — a fit
// that pins every caption to the same pixels no matter the setting would be
// the old bug wearing a different hat.
TEST (SharedCaptionSize, TheFontSettingStillReachesTheCaptions)
{
  auto const atDefault
      = sharedCaptionSize (15.f, contentWidth, columnGap, controlBoxHeight);
  auto const atLargest
      = sharedCaptionSize (15.f * 1.75f, contentWidth, columnGap,
                           controlBoxHeight);

  EXPECT_GT (atLargest, atDefault * 1.2f)
      << "default " << atDefault << ", largest " << atLargest;
}

TEST (SharedCaptionSize, StaysReadableInABarTooSmallForIt)
{
  EXPECT_GE (sharedCaptionSize (30.f, 20, 2, 8), 7.f);
}

}
