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
#include <a3-motion-ui/components/ClipSettingsComponent.hh>

using namespace a3;

namespace
{

// Fitting every caption to its own box made "Q" three times the height of
// "end-action" standing next to it — a size difference that reads as a
// hierarchy which is not there. One size for all of them, and the tightest
// caption in the bar is the one that picks it.

// The bar as it is actually drawn on the device's 768x1024 screen: a quarter
// of the height, four sections side by side. Derived from
// ClipSettingsComponent::paint() — a section's content is 152 px wide with a
// 7 px gap between its columns, and a control box is 37 px tall.
constexpr int contentWidth = 152;
constexpr int columnGap = 7;
constexpr int controlBoxHeight = 37;

// A box tall enough that the height cap is out of the way and only the string
// widths decide. The shipped bar is not that box — see
// TheBoxIsWhatLimitsTheShippedBar below.
constexpr int roomyBox = 200;

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
  auto const size = sharedCaptionSize (30.f, contentWidth, columnGap, roomyBox);

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

// Given room, the theme's size is used as it is — at any setting. This is the
// function's actual contract; what the shipped bar does with it is the next
// test.
TEST (SharedCaptionSize, TheThemeSizeIsUsedWhereThereIsRoomForIt)
{
  EXPECT_NEAR (sharedCaptionSize (15.f, 600, 10, roomyBox), 15.f, 0.01f);
  EXPECT_NEAR (sharedCaptionSize (15.f * 1.75f, 600, 10, roomyBox), 26.25f,
               0.01f);
}

// And in the bar as shipped there is no such room, which is worth stating
// rather than discovering again: a control box is 37 px tall, so a caption row
// of 40% of it caps the font at ~11 px long before the setting's 175% is
// reached. Font Size cannot make this bar's text bigger; a taller box can —
// Pot Size today, a layout that grows with the font eventually. See
// issues/a3-motion-ui-clip-settings-layout-overflows-at-large-fonts.md.
TEST (SharedCaptionSize, TheBoxIsWhatLimitsTheShippedBar)
{
  auto const atDefault
      = sharedCaptionSize (15.f, contentWidth, columnGap, controlBoxHeight);
  auto const atLargest
      = sharedCaptionSize (15.f * 1.75f, contentWidth, columnGap,
                           controlBoxHeight);

  EXPECT_NEAR (atLargest, atDefault, 0.01f);
  EXPECT_NEAR (atLargest,
               controlBoxHeight * captionRowShare / rowHeightFactor, 0.01f);
}

TEST (SharedCaptionSize, StaysReadableInABarTooSmallForIt)
{
  EXPECT_GE (sharedCaptionSize (30.f, 20, 2, 8), 7.f);
}

// The values had the same disparity the captions had: "1" was drawn at full
// size next to a "Forward" shrunk to a third of it, because each was fitted to
// its own box.
TEST (SharedValueSize, EveryValueFitsItsOwnColumn)
{
  auto const size
      = sharedValueSize (30.f, contentWidth, columnGap, controlBoxHeight);

  for (auto const &entry : valueTable)
    {
      auto const columnWidth
          = static_cast<float> ((contentWidth - (entry.columns - 1) * columnGap)
                                / entry.columns);

      EXPECT_LE (widthOf (entry.text, size), columnWidth)
          << entry.text << " is cut off at the shared size";
    }
}

// A control that shows a value has no knob (the toggles, and Speed since its
// knob was dropped), so its box holds exactly two rows. They have to fit — at
// 175% the speed knob's value row used to eat the whole box and leave a lone
// "1" standing where a control should be.
TEST (SharedValueSize, AValueAndItsCaptionFitTheirBoxTogether)
{
  // Not below 30: under that the readability floor wins over the share, and a
  // bar with 20 px control boxes has a layout problem, not a font problem.
  for (auto const box : { 30, 37, 55, 80, 140 })
    for (auto const base : { 11.f, 15.f, 18.f, 26.25f, 31.5f })
      {
        auto const rows
            = (sharedCaptionSize (base, contentWidth, columnGap, box)
               + sharedValueSize (base, contentWidth, columnGap, box))
              * rowHeightFactor;

        EXPECT_LE (rows, static_cast<float> (box) + 0.01f)
            << "box " << box << ", base " << base;
      }
}

// The value is the thing the control is about; its caption only names it, so
// the value gets the larger share of the box. Checked with columns wide enough
// that the words are not the limit — in the shipped bar they are, see
// TheWidestWordIsWhatPinsTheValues, and there the value comes out slightly
// smaller than its caption no matter what the shares say.
TEST (SharedValueSize, TheValueGetsTheLargerShareOfTheBox)
{
  for (auto const box : { 30, 37, 55, 80 })
    EXPECT_GT (sharedValueSize (60.f, 600, 10, box),
               sharedCaptionSize (60.f, 600, 10, box))
        << "box " << box;
}

// The value words used to be what pinned the values: "PingPong" had to fit
// Motion's third of a section — 46 px — which held every value in the bar
// below its own caption's size, no matter how much room the box had.
//
// "16/16", the longest a fade can read, pins it again, but only just: a slash
// and four digits are a shade wider than the box would allow. What has to hold
// is the rule that came out of it — a value is never drawn smaller than the
// caption naming it — so that is what is asserted, with the box as a ceiling
// the words may approach but not exceed.
TEST (SharedValueSize, TheValuesAreLimitedByTheBoxNotByTheirWords)
{
  auto const size
      = sharedValueSize (18.f, contentWidth, columnGap, controlBoxHeight);
  auto const boxLimit = controlBoxHeight * valueRowShare / rowHeightFactor;

  EXPECT_LE (size, boxLimit + 0.01f) << "the box is the ceiling";
  EXPECT_GT (size, boxLimit * 0.9f)
      << "a value word must not pull the whole bar far below its box";
  EXPECT_GT (size,
             sharedCaptionSize (15.f, contentWidth, columnGap,
                                controlBoxHeight))
      << "a value must not be drawn smaller than the caption naming it";
}

}

// ── die Leiste folgt der Schrift, nicht umgekehrt ───────────────────────
//
// Bis 2026-08-27 war es andersherum: die Leiste war auf ein Viertel des
// Schirms genagelt, daraus ergab sich die Reglerbox, und die deckelte die
// Schrift. Body Font Size hat in dieser Leiste deshalb *nichts* getan —
// am Gerät gemessen 14 px Ink bei 100 % wie bei 175 %, während Pot Size auf
// 24 px ging. Siehe
// issues/a3-motion-ui-clip-settings-layout-overflows-at-large-fonts.md.

TEST (ClipSettingsHeight, TheBoxHoldsItsKnobAndBothTextRows)
{
  auto const knob = 30;
  auto const box = controlBoxHeightForFont (15.f, knob);

  EXPECT_GE (box, knob + 2 * (int)(15.f * rowHeightFactor))
      << "knob plus a value row plus a caption row";
}

TEST (ClipSettingsHeight, TheBoxGrowsWithTheFont)
{
  EXPECT_GT (controlBoxHeightForFont (26.f, 30),
             controlBoxHeightForFont (15.f, 30));
}

TEST (ClipSettingsHeight, TheBoxGrowsWithTheKnob)
{
  EXPECT_GT (controlBoxHeightForFont (15.f, 50),
             controlBoxHeightForFont (15.f, 30));
}

// The whole point: at a larger font the bar must ask for more room, or the
// setting cannot reach the text.
TEST (ClipSettingsHeight, TheBarGrowsWithTheBodyFont)
{
  auto const atDefault = clipSettingsPreferredHeight (18.f, 15.f, 30);
  auto const atLargest = clipSettingsPreferredHeight (18.f, 15.f * 1.75f, 30);

  EXPECT_GT (atLargest, atDefault);
}

TEST (ClipSettingsHeight, TheBarGrowsWithTheHeaderFontToo)
{
  EXPECT_GT (clipSettingsPreferredHeight (18.f * 1.75f, 15.f, 30),
             clipSettingsPreferredHeight (18.f, 15.f, 30));
}

// A bar that may grow needs a ceiling, or 175 % body next to 175 % pot size
// leaves the sphere nothing. Beyond it the font is capped again, which is the
// old behaviour — but only at the extreme, not at every setting.
TEST (ClipSettingsHeight, TheBarNeverEatsMoreThanItsShareOfTheScreen)
{
  auto constexpr screen = 1024;
  auto const huge = clipSettingsPreferredHeight (18.f * 1.75f, 15.f * 1.75f, 60);

  EXPECT_LE (clipSettingsHeightWithin (huge, screen),
             (int)(screen * maxClipSettingsScreenShare));
  EXPECT_EQ (clipSettingsHeightWithin (200, screen), 200)
      << "a modest request passes through untouched";
}

// The strip on the right holds what is not the shown clip's. It is meant to
// read as an aside rather than a fifth equal section, so it is half as wide --
// and the four sections beside it stay equal to each other.
TEST (ClipSettingsLayout, TheGlobalStripIsHalfASection)
{
  for (auto const rowWidth : { 400, 703, 704, 1024, 1920 })
    {
      auto const section = ClipSettingsComponent::clipSectionWidth (rowWidth);
      auto const strip
          = rowWidth - ClipSettingsComponent::numClipSections * section;

      EXPECT_GT (section, 0) << "row width " << rowWidth;

      // Integer division leaves at most a few pixels over; the strip must not
      // absorb them into something that reads as a section of its own.
      EXPECT_NEAR (strip, section / 2, ClipSettingsComponent::numClipSections)
          << "row width " << rowWidth << ": section " << section << ", strip "
          << strip;
    }
}
