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

#include <cmath>

#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/io/PadFunctions.hh>

#include <set>

#include <a3-motion-ui/components/ClipSettingsLayout.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>

using namespace a3;

namespace
{

// The device panel's width; the bar gets the bottom quarter of its height
// (see A3MotionUIComponent::resized()).
constexpr int panelWidth = 1280;
constexpr int barHeight = 180;

juce::Rectangle<int> const bar{ 0, 0, panelWidth, barHeight };

// The sizes config/skins/default.json ships with, and the height the bar
// asks for at them — not a fixed 180px. Held at a fixed height the global
// section ran out of room for its grid and the rows collapsed onto each
// other, which says nothing about the layout and everything about the
// number in the test.
constexpr float defaultHeaderSize = 18.f;
constexpr float defaultBodySize = 14.f;
constexpr float defaultPotSize = 0.9f;

juce::Rectangle<int>
grownBar (float headerSize, float bodySize, float potSize)
{
  return { 0, 0, panelWidth,
           clipSettingsPreferredHeight (
               headerSize, bodySize, knobDiameterForFont (bodySize, potSize)) };
}

ClipSettingsLayout
defaultLayout ()
{
  return layOutClipSettings (grownBar (defaultHeaderSize, defaultBodySize,
                                       defaultPotSize),
                             defaultHeaderSize, defaultBodySize,
                             defaultPotSize);
}

}

TEST (ClipSettingsLayout, EverySectionHasItsControls)
{
  auto const l = defaultLayout ();

  // Shape: the picture, the clip field, then the direction and end action
  EXPECT_EQ (l.controls[0].size (), 4u);
  EXPECT_EQ (l.controls[1].size (), 3u); // Elevation: the clips and reach
  // Motion: rot, fade, bias, dir, end, the two squeezes, the three sweeps
  EXPECT_EQ (l.controls[2].size (), 10u);
  EXPECT_EQ (l.controls[3].size (), 1u); // Global: the rec mode
}

// The count each section reports must be the count it lays out, or a tap
// lands on a control that was never drawn.
TEST (ClipSettingsLayout, TheCountMatchesWhatIsLaidOut)
{
  auto const l = defaultLayout ();

  for (int s = 0; s < numClipSettingsSections; ++s)
    EXPECT_EQ (static_cast<int> (l.controls[static_cast<size_t> (s)].size ()),
               numControlsInSection (s))
        << "section " << s;
}

TEST (ClipSettingsLayout, EveryControlSitsInsideItsSectionCard)
{
  auto const l = defaultLayout ();

  for (int s = 0; s < numClipSettingsSections; ++s)
    for (auto const &control : l.controls[static_cast<size_t> (s)])
      EXPECT_TRUE (l.sectionCards[static_cast<size_t> (s)].contains (control))
          << "section " << s << " control escapes its card";
}

TEST (ClipSettingsLayout, ControlsWithinASectionDoNotOverlap)
{
  auto const l = defaultLayout ();

  for (auto const &section : l.controls)
    for (size_t a = 0; a < section.size (); ++a)
      for (size_t b = a + 1; b < section.size (); ++b)
        EXPECT_TRUE (section[a].getIntersection (section[b]).isEmpty ())
            << "controls " << a << " and " << b << " overlap";
}

TEST (ClipSettingsLayout, SectionCardsDoNotOverlapEachOther)
{
  auto const l = defaultLayout ();

  for (size_t a = 0; a < l.sectionCards.size (); ++a)
    for (size_t b = a + 1; b < l.sectionCards.size (); ++b)
      EXPECT_TRUE (
          l.sectionCards[a].getIntersection (l.sectionCards[b]).isEmpty ())
          << "cards " << a << " and " << b << " overlap";
}

// The regression from issues/a3-motion-ui-clip-settings-layout-overflows-at-
// large-fonts.md: at large fonts and large knobs the bar used to spill. Here
// it is a condition rather than something to notice on a screen.
TEST (ClipSettingsLayout, NothingEscapesTheBarAtAnyPotOrFontSize)
{
  for (float potSize : { 0.6f, 0.9f, 1.f, 1.4f, 1.8f })
    for (float bodySize : { 9.f, 12.f, 16.f, 22.f, 28.f })
      {
        auto const l = layOutClipSettings (bar, bodySize * 1.3f, bodySize,
                                           potSize);

        for (int s = 0; s < numClipSettingsSections; ++s)
          for (auto const &control : l.controls[static_cast<size_t> (s)])
            EXPECT_TRUE (bar.contains (control))
                << "pot " << potSize << " body " << bodySize << " section "
                << s;
      }
}

TEST (ClipSettingsLayout, ClipAndGlobalPanelsSplitTheBarWithoutOverlap)
{
  auto const l = defaultLayout ();

  EXPECT_TRUE (l.clipBounds.getIntersection (l.globalBounds).isEmpty ());
  EXPECT_EQ (l.clipBounds.getWidth () + l.globalBounds.getWidth (),
             panelWidth);
}
TEST (ClipSettingsLayout, OnlyFewValuedControlsAdvanceOnTap)
{
  // Elevation is three continuous values -- the two clips and reach. The
  // swell that swept reach has gone to the ACTION page. Nothing here steps.
  for (int sub = 0; sub < 3; ++sub)
    EXPECT_FALSE (tapAdvancesValue (1, sub)) << "elevation " << sub;

  // Motion is ten knobs and no lists at all now: everything in it is dragged.
  for (int sub = 0; sub < 10; ++sub)
    EXPECT_FALSE (tapAdvancesValue (2, sub)) << "sub " << sub;

  // Shape: the library and the clips are too long to tap through; the
  // direction (2) and the end action (3) have a handful of states each and
  // came here from Motion. A renumbering that misses this reads the
  // direction's words off the end action's list and vice versa.
  EXPECT_FALSE (tapAdvancesValue (0, 0));
  EXPECT_FALSE (tapAdvancesValue (0, 1));
  EXPECT_TRUE (tapAdvancesValue (0, 2));
  EXPECT_TRUE (tapAdvancesValue (0, 3));
  EXPECT_FALSE (tapAdvancesValue (0, 4)) << "there is no fifth control";

  // Global: the rec mode has few states.
  EXPECT_TRUE (tapAdvancesValue (3, 0));
}

// The two are alternatives, not layers: a tap either steps a value on or
// flips it, and a control that claimed both would do both on one tap.
TEST (ClipSettingsLayout, NoControlBothStepsAndToggles)
{
  for (int section = 0; section < numClipSettingsSections; ++section)
    for (int sub = 0; sub < numControlsInSection (section); ++sub)
      EXPECT_FALSE (tapAdvancesValue (section, sub)
                    && tapTogglesValue (section, sub))
          << "section " << section << " sub " << sub;
}

// The frame around a section is a line, not a border zone. Every control it
// holds has to sit inside the same content area the fonts were sized against
// — when those were two separate calculations the text was fitted to a
// narrower box than it was drawn in, and the sections looked cramped at
// widths where they were not.
TEST (ClipSettingsLayout, ControlsStayInsideTheirSectionContent)
{
  auto const l = defaultLayout ();

  for (int section = 0; section < numClipSettingsSections; ++section)
    {
      auto const content
          = sectionContentBounds (l.sectionCards[static_cast<size_t> (section)]);

      for (auto const &cell : l.controls[static_cast<size_t> (section)])
        EXPECT_TRUE (content.contains (cell))
            << "section " << section << ": " << cell.toString ()
            << " outside " << content.toString ();
    }
}

// A frame that eats more than a tenth of a section's width is a border zone
// again. At the device's width the three clip sections are a sixth each, and
// what they lose to their own inset they lose from four-value rows.
// Each of the three sections carries a lock at the right end of its title
// row: a square the size of the row, so it is hit without aiming while the
// other hand is busy, and inside its own card so it belongs to the section it
// holds rather than floating between two.
TEST (ClipSettingsLayout, EachSectionCarriesALockOnItsTitleRow)
{
  for (int height : { 200, 314, 460 })
    {
      auto const l = layOutClipSettings ({ 0, 0, 768, height }, 14.f, 12.f, 1.f);

      for (int section = 0; section < 3; ++section)
        {
          auto const s = static_cast<size_t> (section);
          auto const lock = l.sectionLocks[s];

          ASSERT_FALSE (lock.isEmpty ()) << "section " << section;
          EXPECT_TRUE (l.sectionCards[s].contains (lock))
              << "section " << section;

          // Square, and out of the title row -- not out of the controls.
          EXPECT_EQ (lock.getWidth (), lock.getHeight ()) << section;
          EXPECT_FALSE (lock.intersects (l.sectionLabels[s]))
              << "the lock and the word would be drawn over each other, "
              << "section " << section;

          for (auto const &cell : l.controls[s])
            EXPECT_FALSE (lock.intersects (cell))
                << "the lock covers a control of section " << section;
        }

      // The global strip is the device's and holds no clip, so it has none.
      EXPECT_TRUE (l.sectionLocks[3].isEmpty ());
    }
}

TEST (ClipSettingsLayout, TheSectionFrameCostsLittleWidth)
{
  auto const l = defaultLayout ();

  for (int section = 0; section < numClipSettingsSections; ++section)
    {
      auto const card = l.sectionCards[static_cast<size_t> (section)];
      auto const content = sectionContentBounds (card);

      EXPECT_LE (card.getWidth () - content.getWidth (), card.getWidth () / 10)
          << "section " << section;
    }
}

// Two pages share the bar — the clip's settings and the panel's pads — and the
// strip that switches them closes the header row, where the readout used to
// be. The readout moved into the global strip, which is the one part of the
// bar that stands on both pages.
TEST (ClipSettingsLayout, TheHeaderReadsLeftToRightInTheOrderItIsReachedFor)
{
  // Folder, the three views of the clip, the two slots, the four things you
  // do to it. The browser leads because it is where a set begins; the
  // transport closes because it is what you touch once everything else is
  // decided.
  auto const l = defaultLayout ();

  std::vector<juce::Rectangle<int> > row;
  for (size_t ch = 0; ch < numChannelColumns; ++ch)
    {
      row.push_back (l.channelFaces[ch]);
    }
  row.push_back (l.tabClip);
  row.push_back (l.tabRecord);
  row.push_back (l.tabAction);
  row.push_back (l.tabController);
  row.push_back (l.tabBrowser);

  int previousRight = 0;
  for (size_t i = 0; i < row.size (); ++i)
    {
      ASSERT_FALSE (row[i].isEmpty ()) << "item " << i;
      EXPECT_GE (row[i].getX (), previousRight) << "item " << i;
      previousRight = row[i].getRight ();

      // On the header's own line, not above or below it.
      EXPECT_EQ (row[i].getY (), l.channelFaces[0].getY ()) << "item " << i;
      EXPECT_EQ (row[i].getHeight (), l.channelFaces[0].getHeight ())
          << "item " << i;
    }
}

// The row used to mix square marks with wider words, on the reasoning that a
// mark needs no room and a word does. With the channel faces in it that
// stopped being true: a face is a mark *and* a value, there are eight things
// in the row, and eight keys of two sizes read as two rows side by side. One
// size for all of them now -- see TheHeaderKeysAreAllOneSize.

TEST (ClipSettingsLayout, TheHeaderIsTallEnoughToHitAtTheSizeItShipsAt)
{
  // The whole row is pressed mid-set by a hand that is also doing something
  // else. It used to be a twelfth of the bar, which left it under a fingertip.
  for (int height : { 250, 314, 400 })
    {
      auto const l = layOutClipSettings ({ 0, 0, 768, height }, 14.f, 12.f, 1.f);
      EXPECT_GE (l.channelFaces[0].getHeight (), fingertipSize)
          << "height " << height;
    }
}

TEST (ClipSettingsLayout, TheHeaderNeverEatsTheBarItSitsOn)
{
  // A skin can cut the bar down (clipSettingsHeightScale). A row that insisted
  // on a fingertip there would take it out of the controls underneath, which
  // is where the values actually are -- so below a certain size the row gives
  // way rather than the section it heads.
  for (int height : { 120, 160, 200, 250, 314, 400 })
    {
      auto const l = layOutClipSettings ({ 0, 0, 768, height }, 14.f, 12.f, 1.f);
      EXPECT_LE (l.headerHeight, juce::jmax (18, height / 6))
          << "height " << height;
      EXPECT_FALSE (l.clipContent.isEmpty ()) << "height " << height;
    }
}
// A tab is switched mid-set, with one hand, without looking away from the
// deck. The header row is thin, so width is the only room there is to give.
TEST (ClipSettingsLayout, ATabIsWideEnoughToHit)
{
  auto const l = defaultLayout ();

  EXPECT_GE (l.tabRecord.getWidth (), fingertipSize);
  EXPECT_GE (l.tabController.getWidth (), fingertipSize);
}

// Where the clip part's content begins, and the only place that says so. The
// controller page fills this rather than the whole clip part, because it used
// to work the header's height out for itself from the font — off by eleven
// pixels against the bar's own arithmetic, which drew the page's top row of
// pads under the tabs that switch to it.
TEST (ClipSettingsLayout, TheClipContentStartsBelowTheHeaderRow)
{
  auto const l = defaultLayout ();

  EXPECT_FALSE (l.clipContent.isEmpty ());
  EXPECT_TRUE (l.clipBounds.contains (l.clipContent));

  EXPECT_GE (l.clipContent.getY (), l.channelFaces[0].getBottom ());
  EXPECT_GE (l.clipContent.getY (), l.tabBrowser.getBottom ());
  EXPECT_GE (l.clipContent.getY (), l.tabController.getBottom ());

  // And it is what the sections are laid out in, so the two cannot drift.
  for (int section = 0; section < numClipSettingsSections - 1; ++section)
    EXPECT_TRUE (l.clipContent.contains (
        l.sectionCards[static_cast<size_t> (section)]))
        << "section " << section;
}

// Motion used to fit inside whatever Elevation and the global strip asked for
// — with four rows of knobs above its buttons it can be the tallest of the
// three, and a bar sized for the other two squeezed it.
//
// Asked at the height the bar asks for. A skin may cut the bar below that
// (clipSettingsHeightScale goes to half), and the code says plainly that what
// the content needs is "a floor for legibility, not a law" — so this does not
// test a promise nobody made. What *is* promised at any size is the next test.
TEST (ClipSettingsLayout, MotionsControlsStayBigEnoughToHit)
{
  for (float bodySize : { 9.f, defaultBodySize, 24.f })
    for (float potSize : { 0.6f, defaultPotSize, 1.4f })
      {
        auto const l = layOutClipSettings (
            grownBar (defaultHeaderSize, bodySize, potSize), defaultHeaderSize,
            bodySize, potSize);

        for (size_t sub = 0; sub < l.controls[2].size (); ++sub)
          {
            auto const cell = l.controls[2][sub];
            EXPECT_GE (cell.getHeight (), 10)
                << "sub " << sub << ", body " << bodySize << " pot " << potSize;
            EXPECT_GE (cell.getWidth (), 10)
                << "sub " << sub << ", body " << bodySize << " pot " << potSize;
          }
      }
}

// Four knob rows and a button floor. Short of room they give up the same
// amount: a section that helps itself row by row leaves the whole shortfall
// on one row, which came out a sliver while the others were untouched.
TEST (ClipSettingsLayout, MotionsRowsShareWhateverRoomThereIs)
{
  for (int height : { 160, 200, 250, 314, 400 })
    {
      auto const l = layOutClipSettings ({ 0, 0, 768, height }, 14.f, 12.f, 1.f);
      auto const &motion = l.controls[2];
      ASSERT_EQ (motion.size (), 10u) << "height " << height;

      // Every row is a pair, and both halves of a pair are the same height.
      for (auto const &pair : { std::pair<int, int>{ 0, 7 },
                                { 5, 6 },
                                { 9, 8 },
                                { 1, 2 },
                                { 3, 4 } })
        EXPECT_EQ (motion[static_cast<size_t> (pair.first)].getHeight (),
                   motion[static_cast<size_t> (pair.second)].getHeight ())
            << "pair " << pair.first << "/" << pair.second << " at height "
            << height;

      // And every knob row is the same height as every other.
      for (int sub : { 5, 9, 1 })
        EXPECT_EQ (motion[0].getHeight (),
                   motion[static_cast<size_t> (sub)].getHeight ())
            << "sub " << sub << " at height " << height;
    }
}

// Which knob sits where. Five rows of two, each a standing value beside the
// movement that works on it: rot with its spin, reach with its swell, each
// squeeze with its own stretch, the fade with the bias. Sub-index order is
// reading order, which it had not been since the section started growing.
TEST (ClipSettingsLayout, MotionReadsAsPairsDownTheSection)
{
  for (int height : { 200, 314, 460 })
    {
      auto const l = layOutClipSettings ({ 0, 0, 768, height }, 14.f, 12.f, 1.f);
      auto const &motion = l.controls[2];
      ASSERT_EQ (motion.size (), 10u) << "height " << height;

      for (size_t row = 0; row + 1 < motion.size (); row += 2)
        {
          auto const &left = motion[row];
          auto const &right = motion[row + 1];

          EXPECT_EQ (left.getY (), right.getY ()) << "row " << row / 2;
          EXPECT_LT (left.getRight (), right.getX () + 1) << "row " << row / 2;
          EXPECT_EQ (left.getHeight (), right.getHeight ())
              << "row " << row / 2;

          // Under the row before it, and in the same two columns.
          if (row >= 2)
            {
              EXPECT_LE (motion[row - 2].getBottom (), left.getY () + 1)
                  << "row " << row / 2 << " at height " << height;
              EXPECT_EQ (motion[row - 2].getX (), left.getX ())
                  << "row " << row / 2;
              EXPECT_EQ (motion[row - 1].getX (), right.getX ())
                  << "row " << row / 2;
            }
        }
    }
}

// The Shape section has two faces. The front is the clip as it plays -- what
// shape, how fast, which way round -- and the back is the take you are about
// to make: how long, how its join is closed, and the trajectory appearing as
// you play it in. Pressing REC turns the card over, so what you are recording
// is drawn where what you are playing usually is.
TEST (ClipSettingsLayout, ShapesTwoFacesUseTheSameRoom)
{
  auto const front = defaultLayout ();
  auto const back = layOutClipSettings (
      grownBar (defaultHeaderSize, defaultBodySize, defaultPotSize),
      defaultHeaderSize, defaultBodySize, defaultPotSize, BarPage::Record);

  // The section itself does not move: it is one card showing one side or the
  // other, not two cards.
  EXPECT_EQ (front.sectionCards[0], back.sectionCards[0]);

  // Everything else in the bar is untouched -- the record page swaps this one
  // section and nothing more.
  for (int section = 1; section < numClipSettingsSections; ++section)
    EXPECT_EQ (front.sectionCards[static_cast<size_t> (section)],
               back.sectionCards[static_cast<size_t> (section)]);
}

// Twelve, because speed runs from a 128th of a bar to sixteen bars and eight
// buttons cannot say twelve things. They are the section's floor either way,
// so the bar still reads as one row of buttons across its bottom.
TEST (ClipSettingsLayout, TheFrontHasFourSpeedsAndTheBackEightLengths)
{
  auto const front = defaultLayout ();
  auto const back = layOutClipSettings (
      grownBar (defaultHeaderSize, defaultBodySize, defaultPotSize),
      defaultHeaderSize, defaultBodySize, defaultPotSize, BarPage::Record);

  // Four, not the whole range. The eight rows that went are what the clip
  // field stands in -- see speedButtonLog2.
  EXPECT_EQ (numSpeedButtons, 4);
  EXPECT_EQ (numRecordLengths, 8);

  auto const used = [] (auto const &buttons, int count) {
    for (int i = 0; i < count; ++i)
      {
        EXPECT_FALSE (buttons[static_cast<size_t> (i)].isEmpty ()) << i;
        for (int j = i + 1; j < count; ++j)
          EXPECT_FALSE (buttons[static_cast<size_t> (i)].intersects (
              buttons[static_cast<size_t> (j)]))
              << i << " overlaps " << j;
      }
  };

  used (front.speedButtons, numSpeedButtons);
  used (back.lengthButtons, numRecordLengths);
}
// Menu, Rec and Tap sit in the global strip beside the clip's sections. They
// are not sub-elements of it — no encoder reaches them, only a finger — so
// they live beside `controls`, not in it.
TEST (ClipSettingsLayout, TheActionButtonsSitInTheGlobalStrip)
{
  auto const l = defaultLayout ();
  auto const card = l.sectionCards[3];

  EXPECT_TRUE (card.contains (l.menuButton));
  EXPECT_TRUE (card.contains (l.recButton));
  EXPECT_TRUE (card.contains (l.tapButton));
}

TEST (ClipSettingsLayout, TheActionButtonsDoNotOverlapEachOther)
{
  auto const l = defaultLayout ();

  std::vector<juce::Rectangle<int>> const row{ l.recModeButton,
                                               l.clockModeButton,
                                               l.menuButton, l.recButton,
                                               l.tapButton };

  for (size_t a = 0; a < row.size (); ++a)
    for (size_t b = a + 1; b < row.size (); ++b)
      EXPECT_TRUE (row[a].getIntersection (row[b]).isEmpty ())
          << "buttons " << a << " and " << b << " overlap";
}
// The strip's six buttons stand for the panel's six function keys, so they are
// laid out like them: one size for all of them, two columns of three, filled
// top-left to bottom-right. Left down: tap, clock, rec. Right down: recmode,
// menu, shift. A button that is a different size from the others reads as a
// different kind of thing, and these are all the same kind.
TEST (ClipSettingsLayout, TheSixFunctionButtonsAreOneGrid)
{
  auto const l = defaultLayout ();

  std::array<juce::Rectangle<int>, 6> const all{
    l.tapButton,      l.recModeButton, l.clockModeButton,
    l.menuButton,     l.recButton,     l.shiftButton,
  };

  for (auto const &b : all)
    {
      EXPECT_EQ (b.getWidth (), l.tapButton.getWidth ());
      EXPECT_EQ (b.getHeight (), l.tapButton.getHeight ());
    }

  // Left column, top to bottom.
  EXPECT_EQ (l.tapButton.getX (), l.clockModeButton.getX ());
  EXPECT_EQ (l.tapButton.getX (), l.recButton.getX ());
  EXPECT_LT (l.tapButton.getY (), l.clockModeButton.getY ());
  EXPECT_LT (l.clockModeButton.getY (), l.recButton.getY ());

  // Right column, the same three rows.
  EXPECT_EQ (l.recModeButton.getX (), l.menuButton.getX ());
  EXPECT_EQ (l.recModeButton.getX (), l.shiftButton.getX ());
  EXPECT_EQ (l.recModeButton.getY (), l.tapButton.getY ());
  EXPECT_EQ (l.menuButton.getY (), l.clockModeButton.getY ());
  EXPECT_EQ (l.shiftButton.getY (), l.recButton.getY ());

  EXPECT_LT (l.tapButton.getRight (), l.recModeButton.getX ());

  // Under the grid, not beside it.
  for (auto const &column : l.channelGrid)
    for (auto const &cell : column)
      EXPECT_LE (cell.getBottom (), l.tapButton.getY ());
}

// A target a finger can actually hit, at every size the bar is used at.
//
// The bar's height follows the font — see A3MotionUIComponent::resized() and
// clipSettingsPreferredHeight() — so the height has to follow it here too.
// Held at a fixed 180 px this failed at body size 28, which is a combination
// the app never puts on screen.
TEST (ClipSettingsLayout, TheActionButtonsStayBigEnoughToHit)
{
  for (float potSize : { 0.6f, 1.f, 1.8f })
    for (float bodySize : { 9.f, 16.f, 28.f })
      {
        auto const headerSize = bodySize * 1.3f;
        auto const knobDiam = knobDiameterForFont (bodySize, potSize);
        auto const l = layOutClipSettings (
            grownBar (headerSize, bodySize, potSize), headerSize, bodySize,
            potSize);

        for (auto const &b : { l.recModeButton, l.clockModeButton, l.menuButton,
                         l.recButton, l.tapButton })
          {
            // 34 is the floor the layout clamps to; above it the buttons
            // follow knobDiam like every other control in the bar. It was 24,
            // which measured fine and missed the point: a fingertip is wider
            // than that, and TAP was hard to hit.
            EXPECT_GE (b.getWidth (), 40) << "pot " << potSize;
            EXPECT_GE (b.getHeight (), 34) << "body " << bodySize;
          }
      }
}


// Freq, Q and the third value belong to a channel each, not to the clip the
// bar happens to show — so they are a grid of their own in the global
// section, one column per channel.
TEST (ClipSettingsLayout, TheChannelGridHasAColumnPerChannelAndThreeRows)
{
  auto const l = defaultLayout ();
  auto const card = l.sectionCards[3];

  for (int col = 0; col < numChannelColumns; ++col)
    for (int row = 0; row < numChannelRows; ++row)
      EXPECT_TRUE (card.contains (
          l.channelGrid[static_cast<size_t> (col)][static_cast<size_t> (row)]))
          << "channel " << col << " row " << row << " escapes the section";
}

TEST (ClipSettingsLayout, NoTwoGridCellsOverlap)
{
  auto const l = defaultLayout ();

  std::vector<juce::Rectangle<int>> cells;
  for (auto const &column : l.channelGrid)
    for (auto const &cell : column)
      cells.push_back (cell);

  for (size_t a = 0; a < cells.size (); ++a)
    for (size_t b = a + 1; b < cells.size (); ++b)
      EXPECT_TRUE (cells[a].getIntersection (cells[b]).isEmpty ())
          << "cells " << a << " and " << b << " overlap";
}

// Channels read left to right, the three values top to bottom — freq, Q, 3d.
TEST (ClipSettingsLayout, TheGridReadsLeftToRightAndTopToBottom)
{
  auto const l = defaultLayout ();

  for (int col = 1; col < numChannelColumns; ++col)
    EXPECT_GT (l.channelGrid[static_cast<size_t> (col)][0].getX (),
               l.channelGrid[static_cast<size_t> (col - 1)][0].getX ());

  for (int row = 1; row < numChannelRows; ++row)
    EXPECT_GT (l.channelGrid[0][static_cast<size_t> (row)].getY (),
               l.channelGrid[0][static_cast<size_t> (row - 1)].getY ());
}

// The grid must not run into the strip that holds the rec mode and the
// buttons beside it.
TEST (ClipSettingsLayout, TheGridClearsTheActionButtonsAndTheRecMode)
{
  auto const l = defaultLayout ();

  for (auto const &column : l.channelGrid)
    for (auto const &cell : column)
      {
        EXPECT_TRUE (cell.getIntersection (l.recModeButton).isEmpty ());
        EXPECT_TRUE (cell.getIntersection (l.clockModeButton).isEmpty ());
        EXPECT_TRUE (cell.getIntersection (l.menuButton).isEmpty ());
        EXPECT_TRUE (cell.getIntersection (l.recButton).isEmpty ());
        EXPECT_TRUE (cell.getIntersection (l.tapButton).isEmpty ());
      }
}

// A cell a finger can hit, at every size the bar is used at.
TEST (ClipSettingsLayout, GridCellsStayBigEnoughToHit)
{
  for (float potSize : { 0.6f, 1.f, 1.8f })
    for (float bodySize : { 9.f, 16.f, 28.f })
      {
        auto const headerSize = bodySize * 1.3f;
        auto const knobDiam = knobDiameterForFont (bodySize, potSize);
        auto const l = layOutClipSettings (
            grownBar (headerSize, bodySize, potSize), headerSize, bodySize,
            potSize);

        // The invariant that matters: a cell holds the knob it draws. An
        // absolute floor was the wrong test — at the smallest font and pot
        // setting knobDiameterForFont() bottoms out at 10 px, and a 12 px
        // cell around a 10 px knob is right, not cramped. Everything on the
        // bar is that small at that setting.
        // Only that a cell stays usable. It cannot be promised the bar's
        // standard knob diameter any more: the global section is a quarter
        // of the bar, and at the largest pot size four columns of full-size
        // knobs want more than that quarter holds — paintGridKnob then
        // draws to the cell. What the grid gets at the sizes the device
        // ships with is asserted separately, below.
        juce::ignoreUnused (knobDiam);

        for (auto const &column : l.channelGrid)
          for (auto const &cell : column)
            {
              EXPECT_GE (cell.getHeight (), 10)
                  << "pot " << potSize << " body " << bodySize;
              EXPECT_GE (cell.getWidth (), 10)
                  << "pot " << potSize << " body " << bodySize;
            }
      }
}


// At the sizes the device actually ships with, the grid gets the fifth more
// it asks for — the clamp above is for the extremes, not the normal case.
TEST (ClipSettingsLayout, TheGridGetsItsFullKnobAtShippedSizes)
{
  constexpr float bodySize = 14.f;
  constexpr float headerSize = 18.f;
  constexpr float potSize = 0.9f; // config/skins/default.json

  auto const knobDiam = knobDiameterForFont (bodySize, potSize);
  auto const gridKnob = static_cast<int> (knobDiam * 1.2f);

  juce::Rectangle<int> const grown{
    0, 0, panelWidth,
    clipSettingsPreferredHeight (headerSize, bodySize, knobDiam)
  };
  auto const l = layOutClipSettings (grown, headerSize, bodySize, potSize);

  for (auto const &column : l.channelGrid)
    for (auto const &cell : column)
      {
        EXPECT_GE (cell.getWidth (), gridKnob);
        EXPECT_GE (cell.getHeight (), gridKnob);
      }
}


// The take's length is eight buttons on the Shape section's floor, two rows
// of four, reading 1/4 .. 32 in order. It used to be a dropdown offering the
// whole range from 1/128 to 16 bars.
TEST (ClipSettingsLayout, TheLengthButtonsSitInTwoRowsInOrder)
{
  // On the Shape section's back, which is where the take is described. The
  // front carries the speeds now.
  auto const l = layOutClipSettings (
      grownBar (defaultHeaderSize, defaultBodySize, defaultPotSize),
      defaultHeaderSize, defaultBodySize, defaultPotSize, BarPage::Record);
  auto const card = l.sectionCards[0];

  for (int i = 0; i < numRecordLengths; ++i)
    EXPECT_TRUE (card.contains (l.lengthButtons[static_cast<size_t> (i)]))
        << "length " << recordLengthNames[i] << " escapes its section";

  // Four on each row.
  for (int i = 1; i < 4; ++i)
    {
      EXPECT_EQ (l.lengthButtons[static_cast<size_t> (i)].getY (),
                 l.lengthButtons[0].getY ());
      EXPECT_GT (l.lengthButtons[static_cast<size_t> (i)].getX (),
                 l.lengthButtons[static_cast<size_t> (i - 1)].getX ());
    }
  for (int i = 5; i < numRecordLengths; ++i)
    {
      EXPECT_EQ (l.lengthButtons[static_cast<size_t> (i)].getY (),
                 l.lengthButtons[4].getY ());
      EXPECT_GT (l.lengthButtons[static_cast<size_t> (i)].getX (),
                 l.lengthButtons[static_cast<size_t> (i - 1)].getX ());
    }
  EXPECT_GT (l.lengthButtons[4].getY (), l.lengthButtons[0].getY ());
}

TEST (ClipSettingsLayout, NoTwoLengthButtonsOverlap)
{
  auto const l = defaultLayout ();

  for (size_t a = 0; a < numRecordLengths; ++a)
    for (size_t b = a + 1; b < numRecordLengths; ++b)
      EXPECT_TRUE (l.lengthButtons[a]
                       .getIntersection (l.lengthButtons[b])
                       .isEmpty ())
          << "lengths " << recordLengthNames[a] << " and "
          << recordLengthNames[b] << " overlap";
}

// The pictogram keeps the room above them, and they do not run into it.
TEST (ClipSettingsLayout, TheLengthButtonsClearThePictogram)
{
  auto const l = layOutClipSettings (
      grownBar (defaultHeaderSize, defaultBodySize, defaultPotSize),
      defaultHeaderSize, defaultBodySize, defaultPotSize, BarPage::Record);

  for (auto const &b : l.lengthButtons)
    {
      EXPECT_TRUE (b.getIntersection (l.trajectoryIcon).isEmpty ());
      EXPECT_GE (b.getY (), l.trajectoryIcon.getBottom ());
    }
}

// The wording and the powers of two have to agree: the button says what the
// take will be, and log2 is what the setting holds.
// The speeds read from "as recorded" outwards, and each name is the power of
// two it stands for. Two things one test, because a row of numbers that
// claims a scale and does not keep one is worse than no scale at all.
TEST (ClipSettingsLayout, TheSpeedsRunFromAsRecordedIntoTheFastEnd)
{
  ASSERT_GT (numSpeedButtons, 0);
  EXPECT_EQ (speedButtonLog2[0], 0) << "the row has to start where a hand does";

  for (int i = 0; i < numSpeedButtons; ++i)
    {
      auto const log2 = speedButtonLog2[i];
      auto const expected
          = log2 >= 0 ? juce::String (static_cast<int> (std::exp2 (log2)))
                      : "1/" + juce::String (
                            static_cast<int> (std::exp2 (-log2)));

      EXPECT_EQ (juce::String (speedButtonNames[i]), expected) << "button " << i;

      if (i > 0)
        EXPECT_LT (log2, speedButtonLog2[i - 1])
            << "button " << i << " does not carry on away from 1";
    }

  // And they are laid out in that order, left to right.
  auto const l = defaultLayout ();
  for (int i = 1; i < numSpeedButtons; ++i)
    EXPECT_LT (l.speedButtons[static_cast<size_t> (i - 1)].getX (),
               l.speedButtons[static_cast<size_t> (i)].getX ())
        << "button " << i;
}

TEST (ClipSettingsLayout, TheLengthNamesMatchTheirPowersOfTwo)
{
  for (int i = 0; i < numRecordLengths; ++i)
    {
      auto const log2 = recordLengthLog2[i];
      auto const expected
          = log2 >= 0 ? juce::String (static_cast<int> (std::exp2 (log2)))
                      : "1/" + juce::String (
                            static_cast<int> (std::exp2 (-log2)));

      EXPECT_EQ (juce::String (recordLengthNames[i]), expected);
    }
}

// ── The header's transport keys ──────────────────────────────────────────

// They led the header row once, then closed it, and now they have left it
// altogether -- which is what makes room for a fourth view of the clip. In
// reading order over the global strip, rec first and act last.
TEST (ClipSettingsLayout, TheTransportReadsLeftToRightOverTheStrip)
{
  auto const layout = layOutClipSettings ({ 0, 0, 768, 300 }, 14.f, 12.f, 1.f);

  int previousRight = layout.globalBounds.getX ();
  for (int i = 0; i < numTransportKeys; ++i)
    {
      auto const &key = layout.transportButtons[static_cast<size_t> (i)];
      ASSERT_FALSE (key.isEmpty ()) << "key " << i;
      EXPECT_GE (key.getX (), previousRight)
          << "key " << i << " overlaps its neighbour";
      previousRight = key.getRight ();
    }

  EXPECT_LE (previousRight, layout.globalBounds.getRight ())
      << "the row still fits the strip";

  // And nothing of them is left in the clip header, where the tabs now are.
  for (auto const &key : layout.transportButtons)
    EXPECT_FALSE (layout.tabAction.intersects (key));
}
// Three views share the row with four channel faces, their toggles and the
// folder. The tabs are how you change page and are hit mid-set, so each keeps
// a fingertip.
//
// From the device's own width up. The screen is 768 wide (1024 tall, turned
// on its side), and below that the row cannot hold what it holds: twelve
// targets at a fingertip each are 408 pixels before a single gap, against
// three quarters of the width. Testing widths the device does not have would
// only ask the layout to do something no layout can.
TEST (ClipSettingsLayout, TheThreeTabsKeepTheirRoomAtEveryWidth)
{
  for (int width : { 768, 1024, 1280, 1920 })
    {
      auto const layout
          = layOutClipSettings ({ 0, 0, width, 300 }, 14.f, 12.f, 1.f);

      for (auto const &tab : { layout.tabClip, layout.tabRecord,
                               layout.tabAction, layout.tabController })
        {
          EXPECT_GE (tab.getWidth (), fingertipSize) << "width " << width;
          EXPECT_FALSE (tab.isEmpty ()) << "width " << width;
        }

      // In reading order, none of them overlapping.
      EXPECT_LE (layout.channelFaces[0].getRight (), layout.tabClip.getX ());
      EXPECT_LE (layout.tabClip.getRight (), layout.tabRecord.getX ());
      EXPECT_LE (layout.tabRecord.getRight (), layout.tabAction.getX ());
      EXPECT_LE (layout.tabAction.getRight (), layout.tabController.getX ());
    }
}

// ── Shape: the name beside the knob, the picture given the room ──────────

// The picture keeps its own name and the field names the clip. Two names
// because they are two things -- which figure the sound traces, and which
// values it is played with -- and the two controls here change one each. A
// field naming the shape would have been a second copy of what is written
// over the picture, and nothing at all saying which clip you are on.
TEST (ClipSettingsLayout, ThePictureNamesTheShapeAndTheFieldNamesTheClip)
{
  auto const front
      = layOutClipSettings ({ 0, 0, 768, 300 }, 14.f, 12.f, 1.f, BarPage::Clip);
  auto const back = layOutClipSettings ({ 0, 0, 768, 300 }, 14.f, 12.f, 1.f,
                                        BarPage::Record);

  ASSERT_FALSE (front.trajectoryIcon.isEmpty ());
  ASSERT_FALSE (front.clipField.isEmpty ());
  EXPECT_EQ (front.trajectoryName, front.trajectoryIcon)
      << "the shape's name lies over its picture";
  EXPECT_FALSE (front.clipField.intersects (front.trajectoryIcon))
      << "the field would be drawn over the picture it stands under";

  // In reading order: the picture, the field, then the two lists.
  ASSERT_EQ (front.controls[0].size (), 4u);
  EXPECT_EQ (front.controls[0][0], front.trajectoryIcon);
  EXPECT_EQ (front.controls[0][1], front.clipField);
  EXPECT_EQ (front.controls[0][2], front.directionButton);
  EXPECT_EQ (front.controls[0][3], front.endActionButton);

  // On the back face there is no field -- which clip is in the slot is not a
  // question the take you are about to record asks -- and an empty cell is
  // one nothing can land on.
  ASSERT_FALSE (back.trajectoryIcon.isEmpty ());
  EXPECT_TRUE (back.clipField.isEmpty ());
  EXPECT_EQ (back.trajectoryName, back.trajectoryIcon);
  ASSERT_EQ (back.controls[0].size (), 4u);
  EXPECT_TRUE (back.controls[0][1].isEmpty ());
  EXPECT_TRUE (back.controls[0][2].isEmpty ())
      << "the take being recorded has no direction key of its own";
  EXPECT_TRUE (back.controls[0][3].isEmpty ());
}

// The field is a fingertip tall wherever the bar is, and stands between the
// picture and the speeds -- a control reached for mid-set, in the order it is
// reached for: see what is in the slot, change it, then set how fast it runs.
TEST (ClipSettingsLayout, TheClipFieldStandsBetweenThePictureAndTheSpeeds)
{
  for (int height : { 200, 250, 314, 460 })
    {
      auto const l = layOutClipSettings ({ 0, 0, 768, height }, 14.f, 12.f, 1.f);

      ASSERT_FALSE (l.clipField.isEmpty ()) << "height " << height;
      EXPECT_GE (l.clipField.getHeight (), fingertipSize)
          << "height " << height;

      EXPECT_LE (l.trajectoryIcon.getBottom (), l.clipField.getY ())
          << "height " << height;
      EXPECT_LE (l.clipField.getBottom (), l.speedButtons[0].getY ())
          << "height " << height;
      EXPECT_TRUE (l.sectionCards[0].contains (l.clipField))
          << "height " << height;
    }
}

TEST (ClipSettingsLayout, ThePictureIsTheBiggestThingInTheSection)
{
  // Guards the reason for the change rather than its mechanics. Not stated as
  // a fraction of the card: twelve speed buttons in three rows leave the clip
  // face less than a third, and a test demanding one would be demanding the
  // buttons go away. What has to hold is that the picture outranks every
  // single thing around it -- which is exactly what failed when it was a strip
  // sharing its box with the name.
  for (auto const page : { BarPage::Clip, BarPage::Record })
    {
      auto const layout
          = layOutClipSettings ({ 0, 0, 768, 300 }, 14.f, 12.f, 1.f, page);
      auto const face = page == BarPage::Clip ? "clip face" : "record face";

      EXPECT_GT (layout.trajectoryIcon.getHeight (), layout.buttonHeight)
          << face;
      EXPECT_GT (layout.trajectoryIcon.getHeight (),
                 layout.controls[0][1].getHeight ())
          << face;
    }
}

// ── The header keys are the pads, reached another way ────────────────────

TEST (ClipSettingsLayout, EveryTransportKeyNamesTheRightPad)
{
  // The bar's four keys go through the pad handler, so this lookup decides
  // what each of them does. Action and Stop were swapped on the pads page once
  // by deriving the order rather than reading it off the tables; this reads it
  // off the tables and checks it round-trips.
  for (index_t slot = 0; slot < numPadSlots; ++slot)
    for (auto const function : { PadFunction::PlayPause, PadFunction::Stop,
                                 PadFunction::Action, PadFunction::Settings })
      {
        auto const pad = padIndexFor (function, slot);

        EXPECT_EQ (padFunctionByPadIndex[pad], function)
            << "slot " << slot << ", pad " << pad;
        EXPECT_EQ (slotForPadIndex[pad], slot)
            << "slot " << slot << ", pad " << pad;
      }
}

TEST (ClipSettingsLayout, TheFourTransportKeysAreFourDifferentPads)
{
  for (index_t slot = 0; slot < numPadSlots; ++slot)
    {
      std::set<index_t> pads;
      for (auto const key : transportKeyOrder)
        {
          auto const function = key == TransportKey::Record
                                    ? PadFunction::PlayPause
                                    : key == TransportKey::Stop
                                          ? PadFunction::Stop
                                          : key == TransportKey::PlayPause
                                                ? PadFunction::PlayPause
                                                : PadFunction::Action;
          pads.insert (padIndexFor (function, slot));
        }
      // Record and PlayPause deliberately name the same pad -- recording is
      // armed by Play|Pause with Record held -- so three distinct pads.
      EXPECT_EQ (pads.size (), 3u) << "slot " << slot;
    }
}
TEST (ClipSettingsLayout, TheClipFacesStillHaveTheirTransportKeys)
{
  for (auto const page : { BarPage::Clip, BarPage::Record })
    {
      auto const layout
          = layOutClipSettings ({ 0, 0, 768, 300 }, 14.f, 12.f, 1.f, page);

      for (auto const &key : layout.transportButtons)
        EXPECT_FALSE (key.isEmpty ())
            << (page == BarPage::Clip ? "clip face" : "record face");
    }
}
// ── The "not saved" mark ─────────────────────────────────────────────────

TEST (ClipSettingsLayout, TheDriftMarkSitsInsideWhateverItMarks)
{
  // One rule for the slot key and the browser field: they mean the same thing
  // by it, and a mark sitting differently in the two would read as two
  // different marks.
  for (auto const &bounds :
       { juce::Rectangle<int>{ 0, 0, 34, 34 },
         juce::Rectangle<int>{ 10, 20, 120, 60 },
         juce::Rectangle<int>{ 0, 0, 200, 18 } })
    {
      auto const mark = driftMark (bounds);

      ASSERT_FALSE (mark.isEmpty ()) << bounds.toString ();
      EXPECT_TRUE (bounds.contains (mark))
          << mark.toString () << " is not inside " << bounds.toString ();
      EXPECT_EQ (mark.getWidth (), mark.getHeight ()) << "it is a dot";
    }
}

TEST (ClipSettingsLayout, TheDriftMarkStaysVisibleOnASmallControl)
{
  // A skin can shrink the bar. Below a few pixels the mark stops being a dot
  // and becomes a stray pixel, which reads as a rendering fault rather than as
  // information.
  for (int size : { 12, 18, 24, 34, 60 })
    EXPECT_GE (driftMark ({ 0, 0, size, size }).getWidth (), 3)
        << "control " << size << "px";
}

TEST (ClipSettingsLayout, TheDriftMarkIsAFootnoteNotTheContent)
{
  // Top right and small: it comments on the control rather than taking it
  // over, so the value underneath stays readable while it is showing.
  juce::Rectangle<int> const bounds{ 0, 0, 120, 60 };
  auto const mark = driftMark (bounds);

  EXPECT_GT (mark.getX (), bounds.getCentreX ()) << "right half";
  EXPECT_LT (mark.getBottom (), bounds.getCentreY ()) << "top half";
  EXPECT_LT (mark.getWidth () * mark.getHeight (),
             bounds.getWidth () * bounds.getHeight () / 20)
      << "it has taken over the control";
}

TEST (ClipSettingsLayout, NothingToMarkMeansNoMark)
{
  EXPECT_TRUE (driftMark ({}).isEmpty ());
}
// ── The global strip's card ──────────────────────────────────────────────

// The four transport keys stand in the band over the strip, and the card used
// to begin under them -- which left them floating above a panel they plainly
// belong to. The card reaches up over them now, so the strip reads as one
// block: what you do to a clip, and the device functions under it.
TEST (ClipSettingsLayout, TheGlobalCardReachesOverTheTransportKeys)
{
  auto const l = layOutClipSettings ({ 0, 0, 1280, 300 }, 18.f, 14.f, 1.f);

  for (auto const &key : l.transportButtons)
    {
      ASSERT_FALSE (key.isEmpty ());
      EXPECT_TRUE (l.sectionCards[3].contains (key))
          << "a transport key stands outside the card under it";
    }
}

// And the card has no title any more. "global" named a panel whose contents
// name themselves -- twelve knobs with their rows written beside them and six
// keys with words on them -- and the row it took is a row the grid wanted.
TEST (ClipSettingsLayout, TheGlobalCardIsNotTitled)
{
  auto const l = layOutClipSettings ({ 0, 0, 1280, 300 }, 18.f, 14.f, 1.f);

  EXPECT_TRUE (l.sectionLabels[3].isEmpty ())
      << "the global strip still spends a row saying what it is";

  // The three clip sections keep theirs: those do need naming.
  for (size_t i = 0; i < 3; ++i)
    EXPECT_FALSE (l.sectionLabels[i].isEmpty ()) << "section " << i;
}

// The grid still clears the keys above it, whatever the card does.
TEST (ClipSettingsLayout, TheChannelGridStaysClearOfTheTransportKeys)
{
  for (int height : { 200, 300, 400 })
    {
      auto const l = layOutClipSettings ({ 0, 0, 1280, height }, 18.f, 14.f, 1.f);

      for (auto const &column : l.channelGrid)
        for (auto const &cell : column)
          for (auto const &key : l.transportButtons)
            EXPECT_FALSE (cell.intersects (key))
                << "a grid cell runs into a transport key at height " << height;
    }
}

// ── The sections after the reshuffle ─────────────────────────────────────

// Motion comes before Elevation now. What a clip *is* and how it *moves* are
// the two things reached for while playing; where it sits on the sphere is
// set once and left alone, so it goes to the far end.
TEST (ClipSettingsLayout, MotionStandsBeforeElevation)
{
  auto const l = defaultLayout ();

  EXPECT_LT (l.sectionCards[0].getX (), l.sectionCards[2].getX ())
      << "Shape must still come first";
  EXPECT_LT (l.sectionCards[2].getX (), l.sectionCards[1].getX ())
      << "Motion must stand before Elevation";

  for (size_t i = 0; i < 3; ++i)
    ASSERT_FALSE (l.sectionCards[i].isEmpty ()) << "card " << i;
}
// ── What the reshuffle replaced ──────────────────────────────────────────
//
// Five cases held the old arrangement: Shape's knob column, its knob between
// the picture and the buttons, Motion's three lists, and where the fade sat.
// rot, fade and bias are Motion's now and Shape carries only the picture, so
// those cases described a bar that no longer exists. What they were
// protecting is kept here in the shape it has.

// The picture takes the room the knob column had, and still stands on the
// button grid beneath it -- a row that nearly lines up reads as a mistake, one
// that lines up exactly reads as structure.
TEST (ClipSettingsLayout, ThePictureStandsOnTheButtonGrid)
{
  for (auto const page : { BarPage::Clip, BarPage::Record })
    {
      auto const l = layOutClipSettings (
          grownBar (defaultHeaderSize, defaultBodySize, defaultPotSize),
          defaultHeaderSize, defaultBodySize, defaultPotSize, page);

      auto const &button = page == BarPage::Record ? l.lengthButtons[0]
                                                   : l.speedButtons[0];
      ASSERT_FALSE (l.trajectoryIcon.isEmpty ());
      ASSERT_FALSE (button.isEmpty ());

      EXPECT_EQ (l.trajectoryIcon.getX (), button.getX ())
          << "the picture does not start on the button grid";
      EXPECT_LE (l.trajectoryIcon.getBottom (), button.getY ())
          << "the picture runs into the buttons";
    }
}
// The fade is a Motion value now. It reads a take's gaps and decides which are
// drawn through -- something a movement does over time, not something the
// picture is.
TEST (ClipSettingsLayout, TheFadeIsAMotionValueNow)
{
  auto const l = defaultLayout ();

  ASSERT_EQ (l.controls[2].size (), 10u);
  ASSERT_EQ (l.controls[0].size (), 4u);

  // Index one since the spin left -- see OnlyFewValuedControlsAdvanceOnTap.
  EXPECT_FALSE (l.controls[2][1].isEmpty ());
  EXPECT_TRUE (l.sectionCards[2].contains (l.controls[2][1]));
  EXPECT_FALSE (l.sectionCards[0].contains (l.controls[2][1]));
}

// ── Shape after the tidy-up ──────────────────────────────────────────────

// The picture takes the whole width now. It had three of the four button
// columns and left the fourth to a knob column that no longer exists, so a
// quarter of the section was empty air beside the one thing worth looking at.
TEST (ClipSettingsLayout, ThePictureTakesTheWholeWidth)
{
  for (auto const page : { BarPage::Clip, BarPage::Record })
    {
      auto const l = layOutClipSettings (
          grownBar (defaultHeaderSize, defaultBodySize, defaultPotSize),
          defaultHeaderSize, defaultBodySize, defaultPotSize, page);

      auto const &last = page == BarPage::Record
                             ? l.lengthButtons[numRecordLengths - 1]
                             : l.speedButtons[numSpeedButtons - 1];

      // Out to the right edge of the button grid, not three columns of four.
      EXPECT_GE (l.trajectoryIcon.getRight (), last.getRight ())
          << "the picture stops short of the grid";
    }
}

// Every button row is the same height, whatever the section has room for. A
// row that gives up its height while the others keep theirs reads as a
// mistake, and at small bar heights that is what the last row did.
TEST (ClipSettingsLayout, EveryButtonRowIsTheSameHeight)
{
  for (int height : { 160, 200, 250, 314, 400 })
    for (auto const page : { BarPage::Clip, BarPage::Record })
      {
        auto const l = layOutClipSettings ({ 0, 0, 768, height }, 14.f, 12.f,
                                           1.f, page);

        auto const count
            = page == BarPage::Record ? numRecordLengths : numSpeedButtons;
        auto const &buttons0 = page == BarPage::Record ? l.lengthButtons[0]
                                                       : l.speedButtons[0];

        for (int i = 1; i < count; ++i)
          {
            auto const &b = page == BarPage::Record
                                ? l.lengthButtons[static_cast<size_t> (i)]
                                : l.speedButtons[static_cast<size_t> (i)];
            EXPECT_EQ (b.getHeight (), buttons0.getHeight ())
                << "button " << i << " at height " << height;
          }
      }
}

// ── Motion after the tidy-up ─────────────────────────────────────────────// ── The elevation graphic, which is a control now ────────────────────────

// The circle is the sphere seen from the side, north at the top. A finger on
// it says where the middle of the trajectory should sit, so the y it lands on
// has to come back as that fraction -- and the two ends have to be exactly 0
// and 1, or the poles would be unreachable by a hair.
TEST (ClipSettingsLayout, TheGraphicReadsAHeightOutOfAPoint)
{
  juce::Rectangle<int> const bounds{ 20, 40, 100, 100 };
  auto const circle = elevationCircleBounds (bounds);

  ASSERT_FALSE (circle.isEmpty ());

  EXPECT_FLOAT_EQ (elevationBaseAt (bounds, circle.getY ()), 0.f);
  EXPECT_FLOAT_EQ (elevationBaseAt (bounds, circle.getBottom ()), 1.f);
  EXPECT_NEAR (elevationBaseAt (bounds, circle.getCentreY ()), 0.5f, 0.02f);
}

// Above and below the circle it holds at the poles rather than running past
// them. A finger that slides off the top must not wrap round to the bottom.
TEST (ClipSettingsLayout, AFingerPastTheCircleHoldsAtThePole)
{
  juce::Rectangle<int> const bounds{ 0, 0, 80, 80 };

  EXPECT_FLOAT_EQ (elevationBaseAt (bounds, -500), 0.f);
  EXPECT_FLOAT_EQ (elevationBaseAt (bounds, 500), 1.f);
}

// It reads the same circle the graphic draws, whatever shape the cell is --
// otherwise the line would sit where the finger did not.
TEST (ClipSettingsLayout, TheCircleIsSquareInsideWhateverCellItGets)
{
  for (auto const bounds : { juce::Rectangle<int>{ 0, 0, 200, 60 },
                             juce::Rectangle<int>{ 0, 0, 60, 200 },
                             juce::Rectangle<int>{ 5, 7, 90, 90 } })
    {
      auto const circle = elevationCircleBounds (bounds);

      EXPECT_EQ (circle.getWidth (), circle.getHeight ())
          << "the circle is not round";
      EXPECT_TRUE (bounds.contains (circle)) << "the circle leaves its cell";
      EXPECT_EQ (circle.getCentreX (), bounds.getCentreX ());
      EXPECT_EQ (circle.getCentreY (), bounds.getCentreY ());
    }
}

// ── The sections after reach and swell went home ─────────────────────────

// Elevation is four: the two clips, then reach with the swell that sweeps it.
// flat, flat-elevation and pole are gone -- the base the graphic sets says
// what they said, and says it in one place you can see.
TEST (ClipSettingsLayout, ElevationIsTheTwoClipsAndTheSway)
{
  EXPECT_EQ (numControlsInSection (1), 3);

  auto const l = defaultLayout ();
  ASSERT_EQ (l.controls[1].size (), 3u);

  // Nothing in it toggles any more.
  for (int sub = 0; sub < 3; ++sub)
    EXPECT_FALSE (tapTogglesValue (1, sub)) << "sub " << sub;
}

// And Motion holds each standing value beside the movement that works on it:
// the angle with its spin, the two squeezes, the reach with its swell, the
// fade with its bias, and the two lists.
TEST (ClipSettingsLayout, MotionIsTheMovementAndEverythingThatMovesIt)
{
  EXPECT_EQ (numControlsInSection (2), 10);

  auto const l = defaultLayout ();
  ASSERT_EQ (l.controls[2].size (), 10u);

  // Ten knobs and nothing that steps: the two lists went to Shape, where what
  // a pass does when it runs out belongs with the take.
  for (int sub = 0; sub < 10; ++sub)
    EXPECT_FALSE (tapAdvancesValue (2, sub)) << "knob " << sub;
}

// The axis moves inside the clips, not through them. clip-top and
// clip-bottom cut the sphere down from each end, and the middle of the
// trajectory has to stay in what is left -- a base outside the band would be
// a line you can see but the sound cannot reach.
TEST (ClipSettingsLayout, TheBaseStaysInsideTheClips)
{
  juce::Rectangle<int> const bounds{ 0, 0, 100, 100 };
  auto const circle = elevationCircleBounds (bounds);

  // A third clipped off the top, a quarter off the bottom.
  auto constexpr low = 0.33f;
  auto constexpr high = 0.75f;

  EXPECT_FLOAT_EQ (elevationBaseAt (bounds, circle.getY (), low, high), low)
      << "a finger at the north pole must stop at the top clip";
  EXPECT_FLOAT_EQ (
      elevationBaseAt (bounds, circle.getBottom (), low, high), high)
      << "a finger at the south pole must stop at the bottom clip";

  // Inside the band it is untouched.
  auto const middle = elevationBaseAt (bounds, circle.getCentreY (), low, high);
  EXPECT_GE (middle, low);
  EXPECT_LE (middle, high);
  EXPECT_NEAR (middle, 0.5f, 0.02f);
}

// Clips that have been pushed past each other leave no band at all, and the
// axis then has exactly one place to be: where they crossed.
TEST (ClipSettingsLayout, CrossedClipsPinTheAxis)
{
  juce::Rectangle<int> const bounds{ 0, 0, 100, 100 };
  auto const circle = elevationCircleBounds (bounds);

  EXPECT_FLOAT_EQ (elevationBaseAt (bounds, circle.getY (), 0.6f, 0.6f), 0.6f);
  EXPECT_FLOAT_EQ (
      elevationBaseAt (bounds, circle.getBottom (), 0.6f, 0.6f), 0.6f);
}

// Ear height is where a sound is level with the listener, and it is the one
// place in the circle worth being able to hit exactly. In a circle a few
// dozen pixels tall it is a single row otherwise, which is not a target.
TEST (ClipSettingsLayout, TheAxisSnapsToEarHeight)
{
  EXPECT_FLOAT_EQ (snapElevationBase (0.5f), 0.5f);
  EXPECT_FLOAT_EQ (snapElevationBase (0.49f), 0.5f);
  EXPECT_FLOAT_EQ (snapElevationBase (0.515f), 0.5f);

  // And lets go again a little further out, or the equator would be a hole
  // you cannot set a value next to.
  EXPECT_FLOAT_EQ (snapElevationBase (0.42f), 0.42f);
  EXPECT_FLOAT_EQ (snapElevationBase (0.6f), 0.6f);

  // The poles are exact already -- they are what the clamp lands on.
  EXPECT_FLOAT_EQ (snapElevationBase (0.f), 0.f);
  EXPECT_FLOAT_EQ (snapElevationBase (1.f), 1.f);
}

// The axis follows the finger rather than stepping. It was increments for a
// moment, and those arrive once per drag threshold -- twelve pixels apart --
// so the line lurched a step at a time and never sat where the finger was.
// What replaced them is TouchControl::onDragTo, and what it hands over is a
// position, so the maths is elevationBaseAt() either way. The cases above
// cover it.

// ── The header row after the channel keys ────────────────────────────────

// Four channel faces where CLIP and the two slot keys were, each in its
// channel's colour with its own 1/2 toggle beside it. Touching a face is
// "show me this channel's clip", which is what CLIP used to mean -- except
// that it now says *which* clip, and there are four of them on screen at
// once instead of one you have to remember.
TEST (ClipSettingsLayout, TheHeaderCarriesAFaceForEveryChannel)
{
  auto const l = defaultLayout ();

  for (size_t ch = 0; ch < numChannelColumns; ++ch)
    {
      ASSERT_FALSE (l.channelFaces[ch].isEmpty ()) << "face " << ch;

      // A fingertip in both directions: hit mid-set, one-handed, and the
      // number in it has to be readable at a glance.
      EXPECT_GE (l.channelFaces[ch].getHeight (), fingertipSize)
          << "face " << ch;
      EXPECT_GE (l.channelFaces[ch].getWidth (), fingertipSize)
          << "face " << ch;
    }

  // Left to right, in channel order.
  for (size_t ch = 1; ch < numChannelColumns; ++ch)
    EXPECT_GT (l.channelFaces[ch].getX (), l.channelFaces[ch - 1].getX ())
        << "channel " << ch << " is out of order";
}

// CLIP is back, and the faces mean something else than they did.
//
// They were built as the clip tab's replacement -- "show me this channel's
// clip" -- which only made sense while the clip view was the only thing they
// could lead to. They are the selector for the whole settings area now: which
// clip REC, ACTION and CLIP are all describing. That leaves the clip view
// itself without a way back to it from the pads or the browser, so it has its
// own key again, standing where the row of views begins.
TEST (ClipSettingsLayout, TheClipTabStandsAtTheHeadOfTheViews)
{
  auto const l = defaultLayout ();

  ASSERT_FALSE (l.tabClip.isEmpty ());
  EXPECT_GT (l.tabClip.getX (), l.channelFaces[numChannelColumns - 1].getRight ());
  EXPECT_LT (l.tabClip.getRight (), l.tabRecord.getX () + 1);

  EXPECT_GE (l.tabClip.getWidth (), fingertipSize);
  EXPECT_EQ (l.tabClip.getWidth (), l.tabRecord.getWidth ());
  EXPECT_EQ (l.tabClip.getY (), l.tabRecord.getY ());
  EXPECT_EQ (l.tabClip.getHeight (), l.tabRecord.getHeight ());

  EXPECT_TRUE (l.slotButtons[0].isEmpty ())
      << "the shared slot keys moved into the channel faces";
}

// The four faces stand together in a frame of their own, the way the global
// strip's two blocks do.
//
// Without it, nine keys in a row read as nine of the same thing -- and they
// are not: five of them choose what you are looking at, four choose what it
// is you are looking at. The frame is what says so, and it is what lets the
// faces be narrower without reading as keys that came out wrong.
TEST (ClipSettingsLayout, TheFacesStandTogetherInAFrameOfTheirOwn)
{
  for (int width : { 768, 1024, 1280 })
    {
      auto const l
          = layOutClipSettings ({ 0, 0, width, 300 }, 14.f, 12.f, 1.f);

      ASSERT_FALSE (l.channelFacesFrame.isEmpty ()) << "width " << width;

      for (size_t ch = 0; ch < numChannelColumns; ++ch)
        EXPECT_TRUE (l.channelFacesFrame.contains (l.channelFaces[ch]))
            << "face " << ch << " at width " << width;

      // Clear of the views: a frame that ran under the CLIP key would say the
      // key belonged to the group.
      EXPECT_LE (l.channelFacesFrame.getRight (), l.tabClip.getX ())
          << "width " << width;
      EXPECT_TRUE (l.clipBounds.contains (l.channelFacesFrame))
          << "width " << width;
    }
}

// The folder goes to the far right, where the slot keys were. It is the way
// out of the clip you are on, so it sits at the end of the row rather than
// leading it.
TEST (ClipSettingsLayout, TheFolderClosesTheRow)
{
  auto const l = defaultLayout ();

  ASSERT_FALSE (l.tabBrowser.isEmpty ());
  EXPECT_GT (l.tabBrowser.getX (), l.tabController.getX ());
  EXPECT_GT (l.tabBrowser.getX (), l.channelFaces[numChannelColumns - 1].getX ());

  // Still inside the clip part, and the same key as every other in the row
  // -- see TheHeaderKeysAreAllOneSize.
  EXPECT_TRUE (l.clipBounds.contains (l.tabBrowser));
  EXPECT_EQ (l.tabBrowser.getWidth (), l.tabRecord.getWidth ());
}

// And the three remaining views keep their place between the faces and the
// folder, in reading order.
TEST (ClipSettingsLayout, TheThreeViewsStandBetweenThem)
{
  auto const l = defaultLayout ();

  EXPECT_GT (l.tabClip.getX (), l.channelFaces[numChannelColumns - 1].getX ());
  EXPECT_GT (l.tabRecord.getX (), l.tabClip.getX ());
  EXPECT_GT (l.tabAction.getX (), l.tabRecord.getX ());
  EXPECT_GT (l.tabController.getX (), l.tabAction.getX ());
}

// Every channel has a face of its own, on every page, and they read left to
// right before the views. The face carries the slot number, so which slot a
// channel is on is answered without leaving where you are.
TEST (ClipSettingsLayout, EveryChannelHasAFaceOnEveryPage)
{
  for (auto const page : { BarPage::Clip, BarPage::Record,
                           BarPage::Controller })
    {
      auto const l
          = layOutClipSettings ({ 0, 0, 768, 300 }, 14.f, 12.f, 1.f, page);

      int previousRight = 0;
      for (size_t ch = 0; ch < numChannelColumns; ++ch)
        {
          ASSERT_FALSE (l.channelFaces[ch].isEmpty ()) << "channel " << ch;

          EXPECT_GE (l.channelFaces[ch].getX (), previousRight)
              << "channel " << ch;
          previousRight = l.channelFaces[ch].getRight ();
        }

      // Before the views, which are before the folder.
      EXPECT_LE (previousRight, l.tabClip.getX ());
    }
}

// Two kinds of key in the row, and each kind is one size.
//
// It was one size for all eight, which was right while they were all the same
// kind of thing. They are not any more: the five views switch what the area
// shows, the four faces switch which clip it shows. Same height, because they
// share a row and a hand goes along it in one sweep; the faces narrower and
// framed, because a group that reads as a group can afford to.
TEST (ClipSettingsLayout, TheHeaderHasTwoKindsOfKeyAndEachIsOneSize)
{
  for (int width : { 768, 1024, 1280 })
    {
      auto const l
          = layOutClipSettings ({ 0, 0, width, 300 }, 14.f, 12.f, 1.f);

      std::vector<juce::Rectangle<int> > views{ l.tabClip, l.tabRecord,
                                                l.tabAction, l.tabController,
                                                l.tabBrowser };

      for (size_t i = 1; i < views.size (); ++i)
        EXPECT_EQ (views[i].getWidth (), views[0].getWidth ())
            << "view " << i << " at width " << width;

      for (size_t ch = 1; ch < numChannelColumns; ++ch)
        EXPECT_EQ (l.channelFaces[ch].getWidth (),
                   l.channelFaces[0].getWidth ())
            << "face " << ch << " at width " << width;

      // Narrower than a view, and still a fingertip: the row is hit mid-set.
      EXPECT_LT (l.channelFaces[0].getWidth (), views[0].getWidth ())
          << "width " << width;
      EXPECT_GE (l.channelFaces[0].getWidth (), fingertipSize)
          << "width " << width;

      // One height throughout -- that part does not change.
      for (auto const &key : views)
        EXPECT_EQ (key.getHeight (), l.channelFaces[0].getHeight ())
            << "width " << width;
    }
}

// ── The global strip, rearranged ─────────────────────────────────────────

// The transport moved down into the strip, under the knobs it belongs with.
// It stood in the band above, at the header's height, which made it the one
// row of keys in the bar that was a different size from every other.
TEST (ClipSettingsLayout, TheTransportStandsUnderTheKnobs)
{
  auto const l = defaultLayout ();

  for (size_t i = 0; i < l.transportButtons.size (); ++i)
    {
      ASSERT_FALSE (l.transportButtons[i].isEmpty ()) << "key " << i;

      // Inside the strip's card, below every knob in the grid.
      EXPECT_TRUE (l.sectionCards[3].contains (l.transportButtons[i]))
          << "key " << i;
      for (auto const &column : l.channelGrid)
        EXPECT_GE (l.transportButtons[i].getY (), column.back ().getBottom ())
            << "key " << i;
    }

  // Level with each other, left to right, and all one size.
  for (size_t i = 1; i < l.transportButtons.size (); ++i)
    {
      EXPECT_EQ (l.transportButtons[i].getY (), l.transportButtons[0].getY ());
      EXPECT_EQ (l.transportButtons[i].getHeight (),
                 l.transportButtons[0].getHeight ());
      EXPECT_GT (l.transportButtons[i].getX (),
                 l.transportButtons[i - 1].getX ());
    }
}

// As tall as the strip's other keys. It used to be the header's height, which
// is a different size from everything it now stands among.
TEST (ClipSettingsLayout, TheTransportIsAsTallAsTheOtherKeys)
{
  auto const l = defaultLayout ();

  EXPECT_EQ (l.transportButtons[0].getHeight (), l.buttonHeight);
  EXPECT_EQ (l.tapButton.getHeight (), l.buttonHeight);
}

// The channel numbers over the grid are gone. Each column already wears its
// channel's colour, and a colour is read without being read -- the numbers
// were a row of the strip spent saying what four colours already say.
TEST (ClipSettingsLayout, TheGridHasNoChannelNumbers)
{
  auto const l = defaultLayout ();

  for (auto const &label : l.channelLabels)
    EXPECT_TRUE (label.isEmpty ());
}

// The two blocks stand apart: the grid in one frame, the transport in
// another, so the strip reads as what it is -- values above, actions below.
TEST (ClipSettingsLayout, TheGridAndTheTransportHaveTheirOwnFrames)
{
  auto const l = defaultLayout ();

  ASSERT_FALSE (l.channelGridFrame.isEmpty ());
  ASSERT_FALSE (l.transportFrame.isEmpty ());

  EXPECT_FALSE (l.channelGridFrame.intersects (l.transportFrame));
  EXPECT_LT (l.channelGridFrame.getY (), l.transportFrame.getY ());

  // Each frame holds what it is a frame for.
  for (auto const &column : l.channelGrid)
    for (auto const &cell : column)
      EXPECT_TRUE (l.channelGridFrame.contains (cell));
  for (auto const &key : l.transportButtons)
    EXPECT_TRUE (l.transportFrame.contains (key));
}

// The transport used to stand in the band over the strip, at the header's
// height, and it was square there because a mark needs no width. Three cases
// held that shape. It is inside the strip now, under the knobs, sized like
// every other key there -- see TheTransportStandsUnderTheKnobs and
// TheTransportIsAsTallAsTheOtherKeys.
//
// What those three protected and this keeps: the keys are all one size, they
// are on every page, and the band above the strip is empty.
TEST (ClipSettingsLayout, TheTransportIsOneRowOfEqualKeysOnEveryPage)
{
  for (auto const page : { BarPage::Clip, BarPage::Record,
                           BarPage::Controller, BarPage::Browser })
    {
      auto const l
          = layOutClipSettings ({ 0, 0, 768, 400 }, 14.f, 12.f, 1.f, page);

      for (size_t i = 0; i < l.transportButtons.size (); ++i)
        {
          ASSERT_FALSE (l.transportButtons[i].isEmpty ()) << "key " << i;
          EXPECT_EQ (l.transportButtons[i].getWidth (),
                     l.transportButtons[0].getWidth ())
              << "key " << i;
          EXPECT_EQ (l.transportButtons[i].getHeight (),
                     l.transportButtons[0].getHeight ())
              << "key " << i;
        }

      // Nothing is left in the band the header row stands on.
      EXPECT_TRUE (l.readout.isEmpty ());
    }
}


// Every knob in the global grid shows what is carrying it.
//
// The engine has always sent all three moving -- 3d on the accent, freq and Q
// on envelopes of their own -- but the grid handed freq and Q their own value
// as their reach, which is an arc of zero length. Two thirds of what the
// device was doing had to be taken on trust.
TEST (ClipSettingsLayout, AGridKnobsArcReachesWhereTheModulationCarriedIt)
{
  EXPECT_FLOAT_EQ (gridKnobReach (0.3f, 0.8f), 0.8f);

  // At rest the arc has no length: the effective value is the set one exactly,
  // or the knob would drift every time an envelope finished.
  EXPECT_FLOAT_EQ (gridKnobReach (0.3f, 0.3f), 0.3f);

  // Never backwards. envelopeOver() only ever raises, and a ceiling dialled
  // under the floor leaves the floor alone -- an arc running back from the
  // pointer would draw a modulation that cannot happen.
  EXPECT_FLOAT_EQ (gridKnobReach (0.6f, 0.1f), 0.6f);

  // And never off either end, whatever it is handed.
  EXPECT_FLOAT_EQ (gridKnobReach (-1.f, 2.f), 1.f);
  EXPECT_FLOAT_EQ (gridKnobReach (2.f, 2.f), 1.f);
  EXPECT_FLOAT_EQ (gridKnobReach (-1.f, -1.f), 0.f);
}
