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

#include <cstdlib>
#include <limits>

#include <JuceHeader.h>

#include <cmath>

#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/io/PadFunctions.hh>

#include <set>

#include <a3-motion-ui/components/ClipKnobs.hh>
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
  auto const knobDiam = knobDiameterForFont (bodySize, potSize);
  return { 0, 0, panelWidth,
           clipSettingsPreferredHeight (headerSize, bodySize, knobDiam)
               + channelRowHeight (knobDiam, panelWidth) };
}

/** The header's keys in the order they stand, left to right (2026-09-27):
 *  CLIP MOTION ACTION CHMIX REC. FILES, MAINMIX and PADS went up into the
 *  global strip. */
std::vector<juce::Rectangle<int> >
headerKeys (ClipSettingsLayout const &l)
{
  return { l.tabClip, l.tabMotion, l.tabAction, l.tabMixer, l.tabRecord };
}

/** The global strip's keys, left to right: FILES MAINMIX PADS. */
std::vector<juce::Rectangle<int> >
globalKeys (ClipSettingsLayout const &l)
{
  return { l.tabBrowser, l.tabMainMix, l.tabController };
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
  EXPECT_EQ (l.controls[1].size (), 4u); // Elevation: the clips, sway, elv
  // Motion: rot, spin, reach, swell, the squeezes and stretches, fade, bias,
  // and tilt and roll with their sweeps (2026-09-27)
  EXPECT_EQ (l.controls[2].size (), 14u);
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

// Inside the card it is drawn in: its section's, or -- for fade, bias and the
// rec mode -- the REC page's card (cardOfControl).
TEST (ClipSettingsLayout, EveryControlSitsInsideItsSectionCard)
{
  auto const l = defaultLayout ();

  for (int s = 0; s < numClipSettingsSections; ++s)
    for (size_t sub = 0; sub < l.controls[static_cast<size_t> (s)].size ();
         ++sub)
      EXPECT_TRUE (cardOfControl (l, s, static_cast<int> (sub))
                       .contains (l.controls[static_cast<size_t> (s)][sub]))
          << "section " << s << " sub " << sub << " escapes its card";
}

// No two controls a page shows overlap. Two on different pages may: the REC
// card lies where Elevation and Motion stand on CLIP.
TEST (ClipSettingsLayout, ControlsWithinASectionDoNotOverlap)
{
  for (auto const page : { BarPage::Clip, BarPage::Motion, BarPage::Record })
    for (int s = 0; s < numClipSettingsSections; ++s)
      {
        // Each page as that page lays itself out (2026-09-27).
        auto const l = layOutClipSettings (
            grownBar (defaultHeaderSize, defaultBodySize, defaultPotSize),
            defaultHeaderSize, defaultBodySize, defaultPotSize, page);
        auto const &section = l.controls[static_cast<size_t> (s)];
        for (size_t a = 0; a < section.size (); ++a)
          for (size_t b = a + 1; b < section.size (); ++b)
            if (controlIsOnPage (s, static_cast<int> (a), page)
                && controlIsOnPage (s, static_cast<int> (b), page))
              {
                EXPECT_TRUE (
                    section[a].getIntersection (section[b]).isEmpty ())
                    << "section " << s << ": controls " << a << " and " << b
                    << " overlap";
              }
      }
}

// No two cards a page shows overlap. Cards of different pages may -- Shape
// and Motion both start in the left column, on CLIP and MOTION.
TEST (ClipSettingsLayout, SectionCardsDoNotOverlapEachOther)
{
  auto const bar
      = grownBar (defaultHeaderSize, defaultBodySize, defaultPotSize);
  auto const on = [&bar] (BarPage page) {
    return layOutClipSettings (bar, defaultHeaderSize, defaultBodySize,
                               defaultPotSize, page);
  };
  auto const clip = on (BarPage::Clip);
  auto const motion = on (BarPage::Motion);
  auto const rec = on (BarPage::Record);

  std::vector<std::vector<juce::Rectangle<int> > > const pages{
    { clip.sectionCards[0], clip.playCard, clip.lengthCard,
      clip.sectionCards[3] },
    { motion.sectionCards[2], motion.sectionCards[3] },
    { rec.sectionCards[0], rec.sectionCards[3] },
  };

  for (size_t page = 0; page < pages.size (); ++page)
    for (size_t a = 0; a < pages[page].size (); ++a)
      for (size_t b = a + 1; b < pages[page].size (); ++b)
        EXPECT_TRUE (pages[page][a].getIntersection (pages[page][b]).isEmpty ())
            << "page " << page << ": cards " << a << " and " << b << " overlap";
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

TEST (ClipSettingsLayout, ControlsStayInsideTheirSectionContent)
{
  auto const l = defaultLayout ();

  for (int section = 0; section < numClipSettingsSections; ++section)
    for (size_t sub = 0;
         sub < l.controls[static_cast<size_t> (section)].size (); ++sub)
      {
        auto const content = sectionContentBounds (
            cardOfControl (l, section, static_cast<int> (sub)));
        auto const cell = l.controls[static_cast<size_t> (section)][sub];
        EXPECT_TRUE (content.contains (cell))
            << "section " << section << ": " << cell.toString ()
            << " outside " << content.toString ();
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

// The header row is CLIP MOTION ACTION CHMIX REC, in that order (2026-09-27;
// FILES, MAINMIX and PADS lead the global strip since).
// MAINMIX came down from the status bar on 2026-09-26: the big mixer is
// opened from where the channel's own strip is. The channel
// faces that led it went up into the global strip on 2026-09-26, where the
// 4x3 grid was: which clip the bar describes is a device-wide choice, like
// the transport under them.
TEST (ClipSettingsLayout, TheHeaderReadsLeftToRightInTheOrderItIsReachedFor)
{
  auto const l = defaultLayout ();

  auto const row = headerKeys (l);

  int previousRight = 0;
  for (size_t i = 0; i < row.size (); ++i)
    {
      ASSERT_FALSE (row[i].isEmpty ()) << "item " << i;
      EXPECT_GE (row[i].getX (), previousRight) << "item " << i;
      previousRight = row[i].getRight ();

      EXPECT_EQ (row[i].getY (), l.tabClip.getY ()) << "item " << i;
      EXPECT_EQ (row[i].getHeight (), l.tabClip.getHeight ()) << "item " << i;
    }

  for (auto const &face : l.channelFaces)
    EXPECT_FALSE (l.clipBounds.intersects (face))
        << "a channel face is still in the clip's header";
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
      EXPECT_GE (l.tabClip.getHeight (), fingertipSize)
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

  EXPECT_GE (l.clipContent.getY (), l.tabClip.getBottom ());
  EXPECT_GE (l.clipContent.getY (), l.tabRecord.getBottom ());

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

// Four knob rows since fade and bias went to REC. Short of room they give up the same
// amount: a section that helps itself row by row leaves the whole shortfall
// on one row, which came out a sliver while the others were untouched.
TEST (ClipSettingsLayout, MotionsRowsShareWhateverRoomThereIs)
{
  for (int height : { 160, 200, 250, 314, 400 })
    {
      auto const l = layOutClipSettings ({ 0, 0, 768, height }, 14.f, 12.f, 1.f);
      auto const &motion = l.controls[2];
      ASSERT_EQ (motion.size (), 14u) << "height " << height;

      // Every row is a pair, and both halves of a pair are the same height.
      for (auto const &pair : { std::pair<int, int>{ 0, 1 },
                                { 2, 3 },
                                { 4, 5 },
                                { 6, 7 } })
        EXPECT_EQ (motion[static_cast<size_t> (pair.first)].getHeight (),
                   motion[static_cast<size_t> (pair.second)].getHeight ())
            << "pair " << pair.first << "/" << pair.second << " at height "
            << height;

      // And every knob row is the same height as every other.
      for (int sub : { 2, 4, 6 })
        EXPECT_EQ (motion[0].getHeight (),
                   motion[static_cast<size_t> (sub)].getHeight ())
            << "sub " << sub << " at height " << height;
    }
}

// Which knob sits where on the MOTION page: two rows of four, each standing
// value beside the movement that works on it -- rot spin reach swell over
// sqzX strX sqzY strY.
TEST (ClipSettingsLayout, MotionReadsAsPairsDownTheSection)
{
  for (int height : { 200, 314, 460 })
    {
      auto const l = layOutClipSettings ({ 0, 0, 768, height }, 14.f, 12.f, 1.f);
      auto const &motion = l.controls[2];
      ASSERT_EQ (motion.size (), 14u) << "height " << height;

      for (size_t i = 1; i < 4; ++i)
        {
          EXPECT_EQ (motion[i].getY (), motion[0].getY ()) << "sub " << i;
          EXPECT_EQ (motion[i + 4].getY (), motion[4].getY ()) << "sub " << i;
          EXPECT_LE (motion[i - 1].getRight (), motion[i].getX ())
              << "sub " << i;
        }

      EXPECT_GE (motion[4].getY (), motion[0].getBottom ())
          << "height " << height;
      for (size_t i = 0; i < 4; ++i)
        EXPECT_EQ (motion[i].getX (), motion[i + 4].getX ()) << "column " << i;
    }
}

// Twelve, because speed runs from a 128th of a bar to sixteen bars and eight
// buttons cannot say twelve things. They are the section's floor, so the bar
// reads as one row of buttons across its bottom.
//
// There used to be a second face here with eight length keys on it. A take is
// as long as the clip it is recorded over now -- see RecordingLength.hh -- so
// the keys are gone and the section has one face.
TEST (ClipSettingsLayout, TheShapeSectionHasFourSpeedKeys)
{
  auto const front = defaultLayout ();

  // Four, not the whole range. The eight rows that went are what the clip
  // field stands in -- see numSpeedButtons.
  EXPECT_EQ (numSpeedButtons, 4);

  for (int i = 0; i < numSpeedButtons; ++i)
    {
      EXPECT_FALSE (front.speedButtons[static_cast<size_t> (i)].isEmpty ())
          << i;
      for (int j = i + 1; j < numSpeedButtons; ++j)
        EXPECT_FALSE (front.speedButtons[static_cast<size_t> (i)].intersects (
            front.speedButtons[static_cast<size_t> (j)]))
            << i << " overlaps " << j;
    }
}













// A key can be dragged anywhere in the range, so every value in it has to
// arrive on the key as something readable -- including the positive end, which
// is slower than recorded and was never on a key before. Against a four-beat
// take, since a name only means anything once there is a take to apply it to.
TEST (ClipSettingsLayout, EverySpeedInTheRangeIsWordedAndWordedOnlyOnce)
{
  std::set<juce::String> seen;

  for (int log2 = speedLog2Min; log2 <= speedLog2Max; ++log2)
    {
      auto const name = speedKeyName (log2, 4.f);
      EXPECT_FALSE (name.isEmpty ()) << "speed " << log2;
      EXPECT_NE (name, "--") << "speed " << log2;
      EXPECT_TRUE (seen.insert (name).second)
          << "speed " << log2 << " is worded as something already used";
    }

  // The ends, in the ticks a four-beat take actually runs: a 128th of it is
  // a thirty-second of a beat, sixteen times it is sixty-four beats.
  EXPECT_EQ (speedKeyName (speedLog2Min, 4.f), "1/32");
  EXPECT_EQ (speedKeyName (speedLog2Max, 4.f), "64");
}

// The whole range is reachable a key at a time, and the ends of it are ends:
// a drag that ran off one and came back on the other would be a key nobody
// could aim at.
TEST (ClipSettingsLayout, ADragWalksASpeedKeyAcrossTheRangeAndStopsAtItsEnds)
{
  EXPECT_EQ (draggedSpeedLog2 (0, 1), 1);
  EXPECT_EQ (draggedSpeedLog2 (0, -1), -1);
  EXPECT_EQ (draggedSpeedLog2 (0, 0), 0);

  EXPECT_EQ (draggedSpeedLog2 (speedLog2Max, 1), speedLog2Max);
  EXPECT_EQ (draggedSpeedLog2 (speedLog2Min, -1), speedLog2Min);

  // Every value the range holds is a drag away from every other one.
  for (int log2 = speedLog2Min; log2 <= speedLog2Max; ++log2)
    EXPECT_EQ (draggedSpeedLog2 (speedLog2Min, log2 - speedLog2Min), log2);
}

// A key being dragged is the only one that has anything to say while the
// finger is down. The value walks through what the other keys carry on its
// way somewhere, and lighting them as it passes is exactly the picture --
// four keys taking turns -- that this gesture was changed to be rid of.
TEST (ClipSettingsLayout, ADraggedSpeedKeyIsTheOnlyOneLitWhileItMoves)
{
  std::array<int, numSpeedButtons> const keys{ 0, -3, -4, -6 };

  // Key 1 has been dragged as far as what key 2 carries, and key 2 stays dark.
  for (int i = 0; i < numSpeedButtons; ++i)
    EXPECT_EQ (speedKeyIsActive (keys, i, -4, 1), i == 1) << "key " << i;

  // Even where no key at all carries the speed under the finger.
  for (int i = 0; i < numSpeedButtons; ++i)
    EXPECT_EQ (speedKeyIsActive (keys, i, -5, 1), i == 1) << "key " << i;
}

// And when the finger comes up the ordinary rule resumes -- on the key that
// now carries the clip's speed, so there is no jump on release either.
TEST (ClipSettingsLayout, WithNoFingerDownTheKeyCarryingTheSpeedIsLit)
{
  std::array<int, numSpeedButtons> const keys{ 0, -3, -4, -6 };

  for (int i = 0; i < numSpeedButtons; ++i)
    EXPECT_EQ (speedKeyIsActive (keys, i, -4, noSpeedKeyDragged), i == 2)
        << "key " << i;

  // A clip playing at a speed no key carries lights none of them, which is
  // the honest answer to "which of these is it".
  for (int i = 0; i < numSpeedButtons; ++i)
    EXPECT_FALSE (speedKeyIsActive (keys, i, -5, noSpeedKeyDragged))
        << "key " << i;
}

// Two keys may be assigned the same speed -- that is the performer's to do,
// and both lighting is the truth about them rather than something to hide.
TEST (ClipSettingsLayout, TwoKeysCarryingOneSpeedBothLight)
{
  std::array<int, numSpeedButtons> const keys{ 0, -3, -3, -6 };

  EXPECT_TRUE (speedKeyIsActive (keys, 1, -3, noSpeedKeyDragged));
  EXPECT_TRUE (speedKeyIsActive (keys, 2, -3, noSpeedKeyDragged));

  // ... and a drag on one of them still lights only the one under the finger.
  EXPECT_TRUE (speedKeyIsActive (keys, 1, -3, 1));
  EXPECT_FALSE (speedKeyIsActive (keys, 2, -3, 1));
}

// The four length keys stand two by two, in reading order -- in CLIP's
// fields since 2026-09-27, in REC's right card for now.
TEST (ClipSettingsLayout, TheSpeedKeysAreLaidOutInOrder)
{
  auto const l = defaultLayout ();
  auto const &k = l.speedButtons;

  for (auto const &key : k)
    {
      EXPECT_TRUE (l.clipContent.contains (key));
      EXPECT_GE (key.getHeight (), fingertipSize);
    }

  EXPECT_EQ (k[0].getY (), k[1].getY ());
  EXPECT_EQ (k[2].getY (), k[3].getY ());
  EXPECT_GE (k[2].getY (), k[0].getBottom ());
  EXPECT_EQ (k[0].getX (), k[2].getX ());
  EXPECT_EQ (k[1].getX (), k[3].getX ());
  EXPECT_GE (k[1].getX (), k[0].getRight ());
}

// ── The header's transport keys ──────────────────────────────────────────

// Every key in the header keeps a fingertip, at every width from the
// device's own 768 up, in reading order and none overlapping.
TEST (ClipSettingsLayout, TheTabsKeepTheirRoomAtEveryWidth)
{
  for (int width : { 768, 1024, 1280, 1920 })
    {
      auto const layout
          = layOutClipSettings ({ 0, 0, width, 300 }, 14.f, 12.f, 1.f);
      auto const row = headerKeys (layout);

      for (size_t i = 0; i < row.size (); ++i)
        {
          EXPECT_GE (row[i].getWidth (), fingertipSize)
              << "key " << i << " at width " << width;
          if (i > 0)
            {
              EXPECT_LE (row[i - 1].getRight (), row[i].getX ())
                  << "key " << i << " at width " << width;
            }
        }
    }
}

// The row of faces and tabs fills the bar's width: the margin left of the
// first face and the margin right of the last tab are the same.
//
// It did not. The span was worked out for *six* views -- and eleven gaps --
// while only five are ever placed (clip, action, controller, mixer, browser),
// so a whole view's width plus a gap was left standing against the right edge:
// about 90 px on the device's own 768. A row that stops short of the edge it
// is drawn against reads as a mistake, and this one was one.
TEST (ClipSettingsLayout, TheHeaderRowFillsTheWidth)
{
  for (int width : { 768, 1024, 1280, 1920 })
    {
      auto const l = layOutClipSettings ({ 0, 0, width, 300 }, 14.f, 12.f, 1.f);

      // Against clipBounds, not the rect handed in: the global strip takes the
      // right quarter, and the header row ends where the clip part does.
      auto const marginLeft = l.tabClip.getX () - l.clipBounds.getX ();
      auto const marginRight
          = l.clipBounds.getRight () - headerKeys (l).back ().getRight ();

      EXPECT_LE (std::abs (marginLeft - marginRight), 1)
          << "width " << width << ": left " << marginLeft << ", right "
          << marginRight;
    }
}

// The remainder of the integer division is spread across the tabs rather than
// collected against the right edge, so the row ends flush. Which means the
// tabs may differ by a pixel -- and by no more than that, or "spread" would
// be a word for something else.
TEST (ClipSettingsLayout, TheTabsShareTheRemainderEvenly)
{
  for (int width : { 768, 1024, 1280, 1920 })
    {
      auto const l = layOutClipSettings ({ 0, 0, width, 300 }, 14.f, 12.f, 1.f);

      int widest = 0;
      int narrowest = std::numeric_limits<int>::max ();
      for (auto const &tab : headerKeys (l))
        {
          widest = juce::jmax (widest, tab.getWidth ());
          narrowest = juce::jmin (narrowest, tab.getWidth ());
        }

      EXPECT_LE (widest - narrowest, 1)
          << "width " << width << ": " << narrowest << ".." << widest;
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

  ASSERT_FALSE (front.trajectoryIcon.isEmpty ());
  ASSERT_FALSE (front.clipField.isEmpty ());
  EXPECT_EQ (front.trajectoryName, front.trajectoryIcon)
      << "the shape's name lies over its picture";
  EXPECT_FALSE (front.clipField.intersects (front.trajectoryIcon))
      << "the field would be drawn over the picture it stands over";

  // In reading order: the picture, the field, then the two lists.
  ASSERT_EQ (front.controls[0].size (), 4u);
  EXPECT_EQ (front.controls[0][0], front.trajectoryIcon);
  EXPECT_EQ (front.controls[0][1], front.clipField);
  EXPECT_EQ (front.controls[0][2], front.directionButton);
  EXPECT_EQ (front.controls[0][3], front.endActionButton);

  // The field used to be empty on a second face, where which clip is in the
  // slot was not a question the take being recorded asked. That face is gone
  // and the field is always there -- it is where the next take's length is
  // written now.
}

// The clip picker stands over the picture in the CLIP page's left column
// (2026-09-26): which clip, then what it looks like.
TEST (ClipSettingsLayout, TheClipFieldStandsOverThePicture)
{
  for (int height : { 200, 250, 314, 460 })
    {
      auto const l = layOutClipSettings ({ 0, 0, 768, height }, 14.f, 12.f, 1.f);

      ASSERT_FALSE (l.clipField.isEmpty ()) << "height " << height;
      EXPECT_GE (l.clipField.getHeight (), fingertipSize)
          << "height " << height;
      EXPECT_LE (l.clipField.getBottom (), l.trajectoryIcon.getY ())
          << "height " << height;
      EXPECT_TRUE (l.sectionCards[0].contains (l.clipField))
          << "height " << height;
      EXPECT_TRUE (l.sectionCards[0].contains (l.trajectoryIcon))
          << "height " << height;
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
  for (auto const page : { BarPage::Clip })
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

// And it stays a dot on a big field: CLIP's fields are a quarter of the bar
// wide since 2026-09-27, and a fifth of that was a coin, not a footnote.
TEST (ClipSettingsLayout, TheDriftMarkStaysSmallOnABigField)
{
  EXPECT_LE (driftMark ({ 0, 0, 200, 130 }).getWidth (), fingertipSize / 3);
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
  auto const l = layOutClipSettings ({ 0, 0, 1280, 300 }, 18.f, 14.f, 1.f,
                                     BarPage::Motion);

  EXPECT_TRUE (l.sectionLabels[3].isEmpty ())
      << "the global strip still spends a row saying what it is";

  // CLIP and MOTION lost theirs on 2026-09-27 -- see
  // TheClipPageHasNoHeadings and TheMotionPageIsOneAreaInTheEncodersRows.
}


// ── The sections after the reshuffle ─────────────────────────────────────

// ── What the reshuffle replaced ──────────────────────────────────────────
//
// Five cases held the old arrangement: Shape's knob column, its knob between
// the picture and the buttons, Motion's three lists, and where the fade sat.
// rot, fade and bias are Motion's now and Shape carries only the picture, so
// those cases described a bar that no longer exists. What they were
// protecting is kept here in the shape it has.

// The fade is a Motion value now. It reads a take's gaps and decides which are
// drawn through -- something a movement does over time, not something the
// picture is.
TEST (ClipSettingsLayout, TheFadeIsAMotionValueNow)
{
  auto const l = defaultLayout ();

  ASSERT_EQ (l.controls[2].size (), 14u);
  ASSERT_EQ (l.controls[0].size (), 4u);

  // Index one since the spin left -- see OnlyFewValuedControlsAdvanceOnTap.
  EXPECT_FALSE (l.controls[2][1].isEmpty ());
  EXPECT_TRUE (l.sectionCards[2].contains (l.controls[2][1]));
}

// ── Shape after the tidy-up ──────────────────────────────────────────────

// The picture takes the column's width, as the picker over it does.
TEST (ClipSettingsLayout, ThePictureTakesTheWholeWidth)
{
  auto const l = defaultLayout ();
  EXPECT_EQ (l.trajectoryIcon.getX (), l.clipField.getX ());
  EXPECT_EQ (l.trajectoryIcon.getWidth (), l.clipField.getWidth ());
}

// Every button row is the same height, whatever the section has room for. A
// row that gives up its height while the others keep theirs reads as a
// mistake, and at small bar heights that is what the last row did.
TEST (ClipSettingsLayout, EveryButtonRowIsTheSameHeight)
{
  for (int height : { 160, 200, 250, 314, 400 })
    for (auto const page : { BarPage::Clip })
      {
        auto const l = layOutClipSettings ({ 0, 0, 768, height }, 14.f, 12.f,
                                           1.f, page);

        auto const count = numSpeedButtons;
        auto const &buttons0 = l.speedButtons[0];

        for (int i = 1; i < count; ++i)
          {
            auto const &b = l.speedButtons[static_cast<size_t> (i)];
            EXPECT_EQ (b.getHeight (), buttons0.getHeight ())
                << "button " << i << " at height " << height;
          }
      }
}

// ── Motion after the tidy-up ─────────────────────────────────────────────
// ── The elevation graphic, a picture again (2026-09-26) ──────────────────
// The circle is round and centred, whatever shape the cell is.
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

// Elevation is four: the two clips, the sway, and elv -- the base the graphic
// used to set by touch, a knob left of the sway since 2026-09-26. Sub-index 3,
// so the three before it keep theirs.
TEST (ClipSettingsLayout, ElevationIsTheTwoClipsTheSwayAndElv)
{
  EXPECT_EQ (numControlsInSection (1), 4);

  auto const l = defaultLayout ();
  ASSERT_EQ (l.controls[1].size (), 4u);

  for (int sub = 0; sub < 4; ++sub)
    EXPECT_FALSE (tapTogglesValue (1, sub)) << "sub " << sub;

  auto const sway = l.controls[1][2];
  auto const elv = l.controls[1][3];
  EXPECT_EQ (elv.getY (), sway.getY ());
  EXPECT_LE (elv.getRight (), sway.getX ()) << "elv stands left of sway";
  EXPECT_TRUE (l.sectionCards[1].contains (elv));
}

// And Motion holds each standing value beside the movement that works on it:
// the angle with its spin, the two squeezes, the reach with its swell, the
// fade with its bias, and the two lists.
TEST (ClipSettingsLayout, MotionIsTheMovementAndEverythingThatMovesIt)
{
  EXPECT_EQ (numControlsInSection (2), 14);

  auto const l = defaultLayout ();
  ASSERT_EQ (l.controls[2].size (), 14u);

  // Knobs and nothing that steps: the two lists went to Shape, where what
  // a pass does when it runs out belongs with the take.
  for (int sub = 0; sub < 14; ++sub)
    EXPECT_FALSE (tapAdvancesValue (2, sub)) << "knob " << sub;
}

// elv turns the way a level does: clockwise is higher. The base counts from
// the top (0 is the north pole), so the knob shows it the other way up.
TEST (ClipSettingsLayout, ElvRaisesTheLineClockwise)
{
  EXPECT_FLOAT_EQ (elevationBaseForKnob (1.f, 0.f, 0.f), 0.f);
  EXPECT_FLOAT_EQ (elevationBaseForKnob (0.f, 0.f, 0.f), 1.f);
  EXPECT_FLOAT_EQ (knobForElevationBase (0.2f), 0.8f);
  EXPECT_FLOAT_EQ (knobForElevationBase (elevationBaseForKnob (0.3f, 0.f, 0.f)),
                   0.3f);
}

// The axis moves inside the clips, not through them. clip-top and
// clip-bottom cut the sphere down from each end, and the middle of the
// trajectory has to stay in what is left -- a base outside the band would be
// a line you can see but the sound cannot reach.
TEST (ClipSettingsLayout, TheBaseStaysInsideTheClips)
{
  // A third clipped off the top, a quarter off the bottom.
  EXPECT_FLOAT_EQ (elevationBaseForKnob (1.f, 0.33f, 0.25f), 0.33f)
      << "turned full up it must stop at the top clip";
  EXPECT_FLOAT_EQ (elevationBaseForKnob (0.f, 0.33f, 0.25f), 0.75f)
      << "turned full down it must stop at the bottom clip";
}

// Clips that have been pushed past each other leave no band at all, and the
// axis then has exactly one place to be: where they crossed.
TEST (ClipSettingsLayout, CrossedClipsPinTheAxis)
{
  EXPECT_FLOAT_EQ (elevationBaseForKnob (1.f, 0.6f, 0.4f), 0.6f);
  EXPECT_FLOAT_EQ (elevationBaseForKnob (0.f, 0.6f, 0.4f), 0.6f);
}

// Ear height is as easy to land on with the knob as it was with a finger.
TEST (ClipSettingsLayout, ElvSnapsToEarHeight)
{
  EXPECT_FLOAT_EQ (elevationBaseForKnob (0.51f, 0.f, 0.f), 0.5f);
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

// ── The header row after the channel keys ────────────────────────────────

// Four channel faces, each in its channel's colour with its slot number in
// it. Touching a face is "show me this channel's clip". Since 2026-09-27 they
// stand in a row of their own across the whole bar, over the sphere's edge.
TEST (ClipSettingsLayout, TheChannelRowCarriesAFaceForEveryChannel)
{
  for (int width : { 768, 1024, 1280 })
    {
      auto const l = layOutClipSettings ({ 0, 0, width, 300 }, 14.f, 12.f, 1.f);

      for (size_t ch = 0; ch < numChannelColumns; ++ch)
        {
          ASSERT_FALSE (l.channelFaces[ch].isEmpty ()) << "face " << ch;
          EXPECT_TRUE (l.channelFacesFrame.contains (l.channelFaces[ch]))
              << "face " << ch << " at width " << width;

          // A fingertip in both directions: hit mid-set, one-handed, and the
          // number in it has to be readable at a glance.
          EXPECT_GE (l.channelFaces[ch].getHeight (), fingertipSize)
              << "face " << ch << " at width " << width;
          EXPECT_GE (l.channelFaces[ch].getWidth (), fingertipSize)
              << "face " << ch << " at width " << width;
          EXPECT_LE (std::abs (l.channelFaces[ch].getWidth ()
                               - l.channelFaces[0].getWidth ()),
                     1)
              << "face " << ch << " at width " << width;
          EXPECT_EQ (l.channelFaces[ch].getY (), l.channelFaces[0].getY ());
        }

      // Left to right, in channel order.
      for (size_t ch = 1; ch < numChannelColumns; ++ch)
        EXPECT_GT (l.channelFaces[ch].getX (),
                   l.channelFaces[ch - 1].getRight () - 1)
            << "channel " << ch << " is out of order";
    }
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
  EXPECT_LT (l.tabClip.getRight (), l.tabAction.getX () + 1);

  EXPECT_GE (l.tabClip.getWidth (), fingertipSize);
  // Within a pixel, not exactly: the row's remainder is spread across the
  // tabs so it ends flush against the right edge -- see
  // TheTabsShareTheRemainderEvenly.
  EXPECT_LE (std::abs (l.tabClip.getWidth () - l.tabAction.getWidth ()), 1);
  EXPECT_EQ (l.tabClip.getY (), l.tabAction.getY ());
  EXPECT_EQ (l.tabClip.getHeight (), l.tabAction.getHeight ());

  EXPECT_TRUE (l.slotButtons[0].isEmpty ())
      << "the shared slot keys moved into the channel faces";
}

// The faces left the global strip on 2026-09-27 for a row of their own:
// across the whole bar, above the tabs and the global strip alike, between
// the settings and the sphere. As tall as their frame was.
TEST (ClipSettingsLayout, TheChannelRowSpansTheBarAboveEverything)
{
  for (int width : { 768, 1024, 1280 })
    {
      juce::Rectangle<int> const bounds{ 0, 0, width, 300 };
      auto const l = layOutClipSettings (bounds, 14.f, 12.f, 1.f);
      auto const &row = l.channelFacesFrame;

      ASSERT_FALSE (row.isEmpty ()) << "width " << width;
      EXPECT_EQ (row.getX (), bounds.getX ()) << "width " << width;
      EXPECT_EQ (row.getWidth (), bounds.getWidth ()) << "width " << width;
      EXPECT_EQ (row.getY (), bounds.getY ()) << "width " << width;
      EXPECT_GE (row.getHeight (), fingertipSize) << "width " << width;

      EXPECT_LE (row.getBottom (), l.clipBounds.getY ()) << "width " << width;
      EXPECT_LE (row.getBottom (), l.globalBounds.getY ())
          << "width " << width;
      EXPECT_FALSE (row.intersects (l.globalContent)) << "width " << width;

      for (size_t ch = 0; ch < numChannelColumns; ++ch)
        EXPECT_GT (l.channelFaces[ch].getWidth (), width / 8)
            << "a quarter of the row each, less the gaps; face " << ch;
    }
}

// Left to right in every face: the channel's meter, its 3D, FREQ and Q side
// by side, and the rest a bar the clip's progress fills, as a clip slot
// shows it in a DAW. The whole face selects the clip.
TEST (ClipSettingsLayout, EachFaceCarriesItsMeterPotsAndProgress)
{
  auto const l = defaultLayout ();

  for (size_t ch = 0; ch < numChannelColumns; ++ch)
    {
      auto const &face = l.channelFaces[ch];
      auto const &meter = l.channelFaceMeters[ch];
      auto const &pots = l.channelFacePots[ch];
      auto const &progress = l.channelFaceProgress[ch];

      ASSERT_FALSE (meter.isEmpty ()) << "channel " << ch;
      EXPECT_TRUE (face.contains (meter)) << "channel " << ch;
      EXPECT_LT (meter.getX () - face.getX (), face.getWidth () / 8)
          << "the meter stands at the left";
      EXPECT_GT (meter.getHeight (), meter.getWidth ())
          << "a channel's meter stands up";

      auto left = meter.getRight ();
      for (size_t p = 0; p < pots.size (); ++p)
        {
          ASSERT_FALSE (pots[p].isEmpty ()) << "channel " << ch << " pot " << p;
          EXPECT_TRUE (face.contains (pots[p])) << "channel " << ch;
          EXPECT_GE (pots[p].getX (), left) << "channel " << ch << " pot " << p;
          EXPECT_LE (pots[p].getX () - left, juce::jmax (4, face.getHeight () / 8))
              << "each pot right beside what stands before it";
          EXPECT_EQ (pots[p].getWidth (), pots[0].getWidth ());
          left = pots[p].getRight ();
        }

      ASSERT_FALSE (progress.isEmpty ()) << "channel " << ch;
      EXPECT_TRUE (face.contains (progress)) << "channel " << ch;
      EXPECT_GE (progress.getX (), left) << "the bar after the pots";
      EXPECT_GE (progress.getRight (), face.getRight () - face.getHeight () / 4)
          << "the bar fills the rest of the face";
    }
}

TEST (ClipSettingsLayout, AProgressBarFillsFromTheLeft)
{
  juce::Rectangle<int> const bar{ 10, 5, 100, 20 };

  EXPECT_TRUE (progressFill (bar, -1.f).isEmpty ()) << "not playing";
  EXPECT_EQ (progressFill (bar, 0.25f), (juce::Rectangle<int>{ 10, 5, 25, 20 }));
  EXPECT_EQ (progressFill (bar, 1.f), bar);
  EXPECT_EQ (progressFill (bar, 2.f), bar);
}

// FILES, MAINMIX and PADS lead the global strip (2026-09-27): a row of keys
// level with the clip's own tabs and as tall, over the elevation picture.
// They leave the clip, where the tabs are views of it.
TEST (ClipSettingsLayout, FilesMainmixAndPadsLeadTheGlobalStrip)
{
  for (int width : { 768, 1024, 1280 })
    {
      auto const l = layOutClipSettings (
          { 0, 0, width, 300 }, 14.f, 12.f, 1.f);
      auto const keys = globalKeys (l);

      int previousRight = l.globalBounds.getX ();
      for (size_t i = 0; i < keys.size (); ++i)
        {
          ASSERT_FALSE (keys[i].isEmpty ()) << "key " << i;
          EXPECT_TRUE (l.globalBounds.contains (keys[i])) << "key " << i;
          EXPECT_FALSE (l.clipBounds.intersects (keys[i])) << "key " << i;
          EXPECT_GE (keys[i].getX (), previousRight) << "key " << i;
          previousRight = keys[i].getRight ();

          EXPECT_EQ (keys[i].getY (), l.tabClip.getY ()) << "key " << i;
          EXPECT_EQ (keys[i].getHeight (), l.tabClip.getHeight ())
              << "key " << i;
          EXPECT_GE (keys[i].getWidth (), fingertipSize)
              << "key " << i << " at width " << width;
          EXPECT_LE (keys[i].getBottom (), l.elevationFrame.getY ())
              << "key " << i;
        }
    }
}

// Every channel has a face of its own, on every page: the row stands on all
// of them.
TEST (ClipSettingsLayout, EveryChannelHasAFaceOnEveryPage)
{
  for (auto const page : { BarPage::Clip, BarPage::Mixer,
                           BarPage::Action })
    {
      auto const l
          = layOutClipSettings ({ 0, 0, 768, 300 }, 14.f, 12.f, 1.f, page);

      for (size_t ch = 0; ch < numChannelColumns; ++ch)
        {
          ASSERT_FALSE (l.channelFaces[ch].isEmpty ()) << "channel " << ch;
          EXPECT_TRUE (l.channelFacesFrame.contains (l.channelFaces[ch]))
              << "channel " << ch;
        }
    }
}

// The five views are one size, to the pixel the row can afford: every edge
// is computed from the whole width, so the remainder is spread a pixel at a
// time rather than piled against the right edge.
TEST (ClipSettingsLayout, TheHeaderViewsAreOneSize)
{
  for (int width : { 768, 1024, 1280 })
    {
      auto const l
          = layOutClipSettings ({ 0, 0, width, 300 }, 14.f, 12.f, 1.f);

      auto const views = headerKeys (l);

      for (size_t i = 1; i < views.size (); ++i)
        {
          EXPECT_LE (std::abs (views[i].getWidth () - views[0].getWidth ()), 1)
              << "view " << i << " at width " << width;
          EXPECT_EQ (views[i].getHeight (), views[0].getHeight ())
              << "view " << i << " at width " << width;
        }
    }
}


// ── The global strip, rearranged ─────────────────────────────────────────

// Two by two under the faces, arranged as a clip's pads are on PADS: play
// over act on the left, stop on the right. Rec has no pad of its own and
// takes the corner the pads give to Settings. One arrangement in both places,
// so a hand that learnt one has learnt the other.
TEST (ClipSettingsLayout, TheTransportIsTwoByTwoLikeThePads)
{
  auto const l = defaultLayout ();

  auto const keyFor = [&l] (TransportKey key) {
    for (size_t i = 0; i < static_cast<size_t> (numTransportKeys); ++i)
      if (transportKeyOrder[i] == key)
        return l.transportButtons[i];
    return juce::Rectangle<int>{};
  };

  auto const play = keyFor (TransportKey::PlayPause);
  auto const stop = keyFor (TransportKey::Stop);
  auto const act = keyFor (TransportKey::Action);
  auto const rec = keyFor (TransportKey::Record);

  for (auto const &k : l.transportButtons)
    {
      ASSERT_FALSE (k.isEmpty ());
      EXPECT_TRUE (l.sectionCards[3].contains (k));
      EXPECT_TRUE (l.transportFrame.contains (k));
      EXPECT_GE (k.getY (), l.channelFacesFrame.getBottom ());
      EXPECT_EQ (k.getWidth (), play.getWidth ());
      EXPECT_EQ (k.getHeight (), play.getHeight ());
    }

  // Top row: play, stop. Bottom row: act, rec.
  EXPECT_EQ (play.getY (), stop.getY ());
  EXPECT_EQ (act.getY (), rec.getY ());
  EXPECT_GE (act.getY (), play.getBottom ());
  EXPECT_EQ (play.getX (), act.getX ());
  EXPECT_EQ (stop.getX (), rec.getX ());
  EXPECT_GE (stop.getX (), play.getRight ());
}

// Bigger than the strip's other keys: the grid's room went to them. What you
// do to a clip is what the hand goes to most, and it goes there mid-set.
TEST (ClipSettingsLayout, TheTransportIsBiggerThanTheOtherKeys)
{
  auto const l = defaultLayout ();

  EXPECT_GT (l.transportButtons[0].getHeight (), l.buttonHeight);
}


// The two blocks stand apart: the faces in one frame, the transport in
// another -- whose clip, then what to do to it.
TEST (ClipSettingsLayout, TheFacesAndTheTransportHaveTheirOwnFrames)
{
  auto const l = defaultLayout ();

  ASSERT_FALSE (l.channelFacesFrame.isEmpty ());
  ASSERT_FALSE (l.transportFrame.isEmpty ());

  EXPECT_FALSE (l.channelFacesFrame.intersects (l.transportFrame));
  EXPECT_LT (l.channelFacesFrame.getY (), l.transportFrame.getY ());
}

// The transport is on every page, its four keys one size, never under a
// fingertip.
TEST (ClipSettingsLayout, TheTransportIsFourEqualKeysOnEveryPage)
{
  for (auto const page : { BarPage::Clip,
                           BarPage::Mixer, BarPage::Action })
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
          EXPECT_GE (l.transportButtons[i].getHeight (), fingertipSize)
              << "key " << i;
        }

      EXPECT_TRUE (l.readout.isEmpty ());
    }
}



/** The base snaps to the poles as well as to ear height, and for a reason the
 *  ears do not have: a figure whose middle crosses the middle of the pad is
 *  torn there at every base but a pole, and "but a pole" means exactly nought
 *  or exactly one. A finger cannot land on exactly a float, so without this
 *  the one setting that closes the hole is the one setting a hand cannot ask
 *  for. */
TEST (ClipSettingsLayout, TheBaseSnapsToThePolesAsWellAsToEarHeight)
{
  EXPECT_FLOAT_EQ (snapElevationBase (0.012f), 0.f);
  EXPECT_FLOAT_EQ (snapElevationBase (0.988f), 1.f);
  EXPECT_FLOAT_EQ (snapElevationBase (0.495f), 0.5f);

  // And a value deliberately set just off one of them stays there.
  EXPECT_FLOAT_EQ (snapElevationBase (0.06f), 0.06f);
  EXPECT_FLOAT_EQ (snapElevationBase (0.94f), 0.94f);
}

// ── Lengths are counted in the ticks you can see ─────────────────────────────
//
// A key said "1" for "as long as it was recorded", which is a ratio and reads
// like a number of something. What a person counts is the indicator's ticks,
// so that is what the keys say now -- and because the key is still a ratio,
// the number depends on the take it is applied to. Same key, 4 on a four-beat
// take and 8 on an eight-beat one, which is the truth about what it does.

TEST (BeatsName, WholeBeatsAreJustTheNumber)
{
  EXPECT_EQ (beatsName (1.f), "1");
  EXPECT_EQ (beatsName (4.f), "4");
  EXPECT_EQ (beatsName (128.f), "128");
}

TEST (BeatsName, LessThanABeatIsAFraction)
{
  EXPECT_EQ (beatsName (0.5f), "1/2");
  EXPECT_EQ (beatsName (0.25f), "1/4");
  EXPECT_EQ (beatsName (0.0625f), "1/16");
}

// A take is not always a power of two long -- a six-beat one at a sixteenth of
// its length is three eighths of a beat, and "3/8" is the only honest way to
// write that. Rounding it to 1/2 or 1/4 would put a number on a key that the
// indicator then contradicts.
TEST (BeatsName, AnAwkwardLengthKeepsItsNumerator)
{
  EXPECT_EQ (beatsName (0.375f), "3/8");
  EXPECT_EQ (beatsName (1.5f), "3/2");
  EXPECT_EQ (beatsName (6.f), "6");
}

TEST (SpeedKeyName, TheKeyNamesWhatThisTakeWillRun)
{
  // Four-beat take: the key that plays it as recorded says four.
  EXPECT_EQ (speedKeyName (0, 4.f), "4");
  EXPECT_EQ (speedKeyName (-2, 4.f), "1");
  EXPECT_EQ (speedKeyName (2, 4.f), "16");

  // Eight-beat take, same keys, different numbers -- because the key is a
  // ratio and always was.
  EXPECT_EQ (speedKeyName (0, 8.f), "8");
  EXPECT_EQ (speedKeyName (-2, 8.f), "2");
}

// Without a clip there is nothing to be a ratio of, and a key showing a number
// it cannot honour is worse than one showing none.
TEST (SpeedKeyName, NoTakeMeansNoNumber)
{
  EXPECT_EQ (speedKeyName (0, 0.f), "--");
}

// The next take's length is not a ratio: it is the take being made, and it is
// read in the same ticks as everything else in the bar. beatsName() is what
// writes it into the clip field now -- recordLengthName() went with the eight
// keys, but what it spelled is still spelled here.
TEST (BeatsName, ALengthIsShownAsTheBeatsItHolds)
{
  EXPECT_EQ (beatsName (4.f), "4");
  EXPECT_EQ (beatsName (16.f), "16");
  EXPECT_EQ (beatsName (1.f), "1");
  EXPECT_EQ (beatsName (128.f), "128");

  // Three four, and a quarter of a bar in it.
  EXPECT_EQ (beatsName (3.f), "3");
  EXPECT_EQ (beatsName (0.75f), "3/4");

  // Nothing to say is said as nothing.
  EXPECT_EQ (beatsName (0.f), "--");
}

// ── Where the hand is after a row is thrown away ─────────────────────────────
//
// A delete used to put the selection back on row 0 -- the library's "Empty",
// which has no file -- and the list back at its top. In a library of seventy
// rows that loses your place, and the row you reach for next is one of the
// thirty-nine shipped ones, where the Delete key is correctly dark. It reads
// as the key having stopped working, which is how it was reported: *"clips
// löschen geht nur 2x. Nach neustart gehts dann wieder."* Measured at the
// device on 2026-09-13.

TEST (SelectionAfterRemoving, TheRowThatTookItsPlaceIsChosen)
{
  // Six rows, the fourth thrown away: five left, and row 3 is now what was
  // row 4. Staying on the number keeps the hand where it was.
  EXPECT_EQ (selectionAfterRemoving (3, 5), 3);
}

TEST (SelectionAfterRemoving, ThrowingAwayTheLastRowStepsBack)
{
  // There is no row 4 any more, so the new last one is the honest answer.
  EXPECT_EQ (selectionAfterRemoving (4, 4), 3);
}

// Row zero is the library's "Empty" and is always listed, so a list can never
// be shorter than one row -- but a caller that has just emptied a folder
// should not be handed a negative.
TEST (SelectionAfterRemoving, AnEmptiedListLandsOnRowZero)
{
  EXPECT_EQ (selectionAfterRemoving (2, 1), 0);
  EXPECT_EQ (selectionAfterRemoving (2, 0), 0);
}

TEST (SelectionAfterRemoving, ARowBeforeTheStartIsRowZero)
{
  EXPECT_EQ (selectionAfterRemoving (-1, 5), 0);
}

// MAINMIX is a tab like the others since 2026-09-26, not a toggle: while the
// big mixer is shown, MAINMIX is the one lit tab, and the page underneath
// does not also claim to be selected. With the mixer away, the shown page is.
// FILES lies over the sphere the same way since 2026-09-27, and takes the
// light the same way.
TEST (ClipSettingsLayout, OneTabIsLitAndAnOverlayTakesItWhileOpen)
{
  EXPECT_TRUE (
      pageTabIsLit (BarPage::Clip, BarPage::Clip, SphereOverlay::None));
  EXPECT_FALSE (
      pageTabIsLit (BarPage::Action, BarPage::Clip, SphereOverlay::None));

  EXPECT_FALSE (
      pageTabIsLit (BarPage::Clip, BarPage::Clip, SphereOverlay::MainMix))
      << "the page under the big mixer is lit beside MAINMIX";
  EXPECT_FALSE (
      pageTabIsLit (BarPage::Clip, BarPage::Clip, SphereOverlay::Files))
      << "the page under the browser is lit beside FILES";
}

// FILES, MAINMIX and PADS toggle (2026-09-27): a tap on the lit key takes it
// away again, a tap on another swaps it in. One value over the sphere, so only
// one key is ever on.
TEST (ClipSettingsLayout, TheKeysOverTheSphereToggleAndOnlyOneIsOn)
{
  EXPECT_EQ (overlayAfterTap (SphereOverlay::None, SphereOverlay::Pads),
             SphereOverlay::Pads);
  EXPECT_EQ (overlayAfterTap (SphereOverlay::Pads, SphereOverlay::Pads),
             SphereOverlay::None)
      << "a second tap leaves it up";
  EXPECT_EQ (overlayAfterTap (SphereOverlay::Files, SphereOverlay::Files),
             SphereOverlay::None);
  EXPECT_EQ (overlayAfterTap (SphereOverlay::MainMix, SphereOverlay::MainMix),
             SphereOverlay::None);
  EXPECT_EQ (overlayAfterTap (SphereOverlay::Files, SphereOverlay::Pads),
             SphereOverlay::Pads)
      << "two over the sphere at once";
  EXPECT_EQ (overlayAfterTap (SphereOverlay::Pads, SphereOverlay::MainMix),
             SphereOverlay::MainMix);

  EXPECT_FALSE (
      pageTabIsLit (BarPage::Clip, BarPage::Clip, SphereOverlay::Pads))
      << "the page under the pads is lit beside PADS";
}

// The readout names the key and which way it went.
TEST (ClipSettingsLayout, TheReadoutSaysWhichOverlayWentOnOrOff)
{
  EXPECT_STREQ (overlayReadout (SphereOverlay::Pads, true), "-- PADS ON");
  EXPECT_STREQ (overlayReadout (SphereOverlay::Pads, false), "-- PADS OFF");
  EXPECT_STREQ (overlayReadout (SphereOverlay::Files, true), "-- FILES ON");
  EXPECT_STREQ (overlayReadout (SphereOverlay::MainMix, false),
                "-- MIX OFF");
}

// ── The REC page (2026-09-26) ─────────────────────────────────────────────



// Motion keeps its first eight in its own card, on the MOTION page.
TEST (ClipSettingsLayout, MotionKeepsEightKnobsOnTheMotionPage)
{
  auto const l = defaultLayout ();

  for (size_t sub = 0; sub < motionSubsOnTheMotionPage; ++sub)
    EXPECT_TRUE (l.sectionCards[2].contains (l.controls[2][sub]))
        << "sub " << sub;
}

// The global strip is the faces and the transport: the six function keys are
// gone, and the transport runs to the strip's foot.
TEST (ClipSettingsLayout, TheTransportRunsToTheFootOfTheStrip)
{
  auto const l = defaultLayout ();

  EXPECT_EQ (l.transportFrame.getBottom (), l.globalContent.getBottom ());
}

// Which control a page shows. CLIP: Shape's four -- the picker, the picture,
// dir and end. MOTION: Motion's first eight and
// Elevation's four. REC: the picker and the picture, fade, bias and the rec
// mode. Nothing of the clip on a page that covers it.
TEST (ClipSettingsLayout, EachControlStandsOnItsOwnPage)
{
  constexpr int shape = 0, elevation = 1, motion = 2, global = 3;

  EXPECT_TRUE (controlIsOnPage (shape, 0, BarPage::Clip));
  EXPECT_TRUE (controlIsOnPage (shape, 1, BarPage::Clip));
  EXPECT_TRUE (controlIsOnPage (shape, 2, BarPage::Clip));
  EXPECT_TRUE (controlIsOnPage (shape, 3, BarPage::Clip));
  EXPECT_FALSE (controlIsOnPage (shape, 2, BarPage::Record));
  EXPECT_FALSE (controlIsOnPage (shape, 0, BarPage::Motion));
  EXPECT_TRUE (controlIsOnPage (shape, 0, BarPage::Record));

  EXPECT_TRUE (controlIsOnPage (elevation, 0, BarPage::Motion));
  EXPECT_FALSE (controlIsOnPage (elevation, 0, BarPage::Clip));
  EXPECT_FALSE (controlIsOnPage (elevation, 0, BarPage::Record));

  EXPECT_TRUE (controlIsOnPage (motion, 7, BarPage::Motion));
  EXPECT_FALSE (controlIsOnPage (motion, 7, BarPage::Clip));
  EXPECT_FALSE (controlIsOnPage (motion, 8, BarPage::Motion));
  EXPECT_TRUE (controlIsOnPage (motion, 8, BarPage::Record));
  EXPECT_TRUE (controlIsOnPage (motion, 9, BarPage::Record));

  EXPECT_TRUE (controlIsOnPage (global, 0, BarPage::Record));
  EXPECT_FALSE (controlIsOnPage (global, 0, BarPage::Clip));

  for (auto const page : { BarPage::Action, BarPage::Mixer })
    EXPECT_FALSE (controlIsOnPage (shape, 0, page));
}


// The elevation picture stands at the top of the global strip since
// 2026-09-26, over the transport since the faces left on 2026-09-27 -- on
// every page, since the strip is -- in a grey frame of its own like the
// transport. The Elevation card keeps its four knobs and nothing else.
TEST (ClipSettingsLayout, TheElevationPictureLeadsTheGlobalStrip)
{
  auto const l = defaultLayout ();

  ASSERT_FALSE (l.elevationFrame.isEmpty ());
  EXPECT_EQ (l.elevationFrame.getY (), l.globalContent.getY ());
  EXPECT_TRUE (l.globalContent.contains (l.elevationFrame));
  EXPECT_GE (l.elevationFrame.getY (), l.tabController.getBottom ())
      << "under FILES, MAINMIX and PADS";
  EXPECT_TRUE (l.elevationFrame.contains (l.elevationGraphic));
  EXPECT_LT (l.elevationGraphic.getWidth (), l.elevationFrame.getWidth ())
      << "the picture stands inside its frame, not on its edge";
  EXPECT_LE (l.elevationFrame.getBottom (), l.transportFrame.getY ());
  EXPECT_GE (l.elevationFrame.getY (), l.channelFacesFrame.getBottom ());
  EXPECT_FALSE (l.sectionCards[1].intersects (l.elevationFrame));

  // Big enough to read a line off: a circle across most of the strip.
  auto const circle = elevationCircleBounds (l.elevationGraphic);
  EXPECT_GE (circle.getWidth (), l.globalContent.getWidth () / 3);
}

// A small camera in the picture's top right corner says what touching the
// picture does: it selects the camera.
TEST (ClipSettingsLayout, ACameraMarkSitsInThePicturesCorner)
{
  auto const l = defaultLayout ();
  auto const mark = l.elevationCameraMark;

  ASSERT_FALSE (mark.isEmpty ());
  EXPECT_TRUE (l.elevationFrame.contains (mark));
  EXPECT_EQ (mark.getWidth (), mark.getHeight ());
  EXPECT_GT (mark.getCentreX (), l.elevationFrame.getCentreX ());
  EXPECT_LT (mark.getCentreY (), l.elevationFrame.getCentreY ());
  EXPECT_LE (mark.getWidth (), l.elevationFrame.getWidth () / 4)
      << "a mark, not a second picture";
}

// ── CLIP and MOTION (2026-09-26) ─────────────────────────────────────────



// ── CLIP as one area of eight fields (2026-09-27) ──────────────────────────

// One area, no headings, eight fields of one size in the encoders' four by
// two: clip, dir, two lengths over the shape, end, two lengths.
TEST (ClipSettingsLayout, TheClipPageIsEightEqualFields)
{
  for (int width : { 768, 1024, 1280 })
    {
      auto const l = layOutClipSettings (grownBar (defaultHeaderSize,
                                                   defaultBodySize,
                                                   defaultPotSize)
                                             .withWidth (width),
                                         defaultHeaderSize, defaultBodySize,
                                         defaultPotSize, BarPage::Clip);
      auto const &f = l.pageFields;

      std::array<juce::Rectangle<int>, 8> const expected{
        l.clipField,         l.directionButton,  l.speedButtons[0],
        l.speedButtons[1],   l.trajectoryIcon,   l.endActionButton,
        l.speedButtons[2],   l.speedButtons[3],
      };

      for (size_t i = 0; i < f.size (); ++i)
        {
          ASSERT_FALSE (f[i].isEmpty ()) << "field " << i;
          EXPECT_TRUE (l.clipContent.contains (f[i])) << "field " << i;
          EXPECT_LE (std::abs (f[i].getWidth () - f[0].getWidth ()), 1)
              << "field " << i << " at width " << width;
          EXPECT_LE (std::abs (f[i].getHeight () - f[0].getHeight ()), 1)
              << "field " << i << " at width " << width;
          EXPECT_EQ (expected[i], f[i]) << "field " << i;
          for (size_t j = 0; j < i; ++j)
            EXPECT_FALSE (f[i].intersects (f[j])) << i << " and " << j;
        }

      // Four across, two down, in reading order.
      for (size_t col = 1; col < 4; ++col)
        {
          EXPECT_GT (f[col].getX (), f[col - 1].getX ());
          EXPECT_EQ (f[col].getY (), f[0].getY ());
          EXPECT_EQ (f[col + 4].getX (), f[col].getX ());
        }
      EXPECT_GT (f[4].getY (), f[0].getBottom () - 1);
    }
}

TEST (ClipSettingsLayout, TheClipPageHasNoHeadings)
{
  auto const l = defaultLayout ();

  EXPECT_TRUE (l.sectionLabels[0].isEmpty ());
  EXPECT_TRUE (l.playLabel.isEmpty ());
  EXPECT_TRUE (l.lengthLabel.isEmpty ());
  EXPECT_EQ (l.sectionCards[0], l.clipContent) << "one area";
}


// ── MOTION as one area in the encoders' rows (2026-09-27) ──────────────────

namespace
{
ClipSettingsLayout
motionPage (int width = panelWidth)
{
  return layOutClipSettings (
      grownBar (defaultHeaderSize, defaultBodySize, defaultPotSize)
          .withWidth (width),
      defaultHeaderSize, defaultBodySize, defaultPotSize, BarPage::Motion);
}
}

// Eight fields, one per encoder, as CLIP and REC have (2026-09-27). Each
// holds what its encoder turns, the one it turns at rest on the left and the
// one a click gives on the right: spin|rot, swell|reach, strX|sqzX, strY|sqzY
// over sway|elv, clip-top|clip-bot, and two fields with nothing in them.
TEST (ClipSettingsLayout, TheMotionPageIsEightFieldsOnePerEncoder)
{
  for (int width : { 768, 1024, 1280 })
    {
      auto const l = motionPage (width);
      auto const &f = l.pageFields;
      auto const &m = l.controls[2];
      auto const &e = l.controls[1];

      EXPECT_TRUE (l.sectionLabels[1].isEmpty ());
      EXPECT_TRUE (l.sectionLabels[2].isEmpty ());

      // Motion: 0 rot 1 spin 2 reach 3 swell 4 sqzX 5 strX 6 sqzY 7 strY.
      // Elevation: 0 clip-bot 1 clip-top 2 sway 3 elv.
      std::vector<std::pair<juce::Rectangle<int>, juce::Rectangle<int> > > const
          pairs{ { m[1], m[0] },   { m[3], m[2] },   { m[5], m[4] },
                 { m[7], m[6] },   { e[2], e[3] },   { e[1], e[0] },
                 { m[11], m[10] }, { m[13], m[12] } }; // tswp|tilt, rswp|roll

      for (size_t i = 0; i < f.size (); ++i)
        {
          ASSERT_FALSE (f[i].isEmpty ()) << "field " << i;
          EXPECT_LE (std::abs (f[i].getWidth () - f[0].getWidth ()), 1);
          EXPECT_LE (std::abs (f[i].getHeight () - f[0].getHeight ()), 1);
        }

      for (size_t i = 0; i < pairs.size (); ++i)
        {
          auto const &[atRest, clicked] = pairs[i];
          EXPECT_TRUE (f[i].contains (atRest))
              << "field " << i << " at width " << width;
          EXPECT_TRUE (f[i].contains (clicked))
              << "field " << i << " at width " << width;
          EXPECT_LE (atRest.getRight (), clicked.getX ())
              << "at rest on the left, field " << i;
        }
    }
}


// ── REC as one area of eight fields (2026-09-27) ───────────────────────────

// Like CLIP, with the take's settings where CLIP has dir and end: clip,
// recmode and two lengths over the shape, fade|bias and the other two. Fade
// and bias share a field -- one encoder, a click between them.
TEST (ClipSettingsLayout, TheRecPageIsEightEqualFields)
{
  for (int width : { 768, 1024, 1280 })
    {
      auto const l = layOutClipSettings (grownBar (defaultHeaderSize,
                                                   defaultBodySize,
                                                   defaultPotSize)
                                             .withWidth (width),
                                         defaultHeaderSize, defaultBodySize,
                                         defaultPotSize, BarPage::Record);
      auto const &f = l.pageFields;
      auto const fade = l.controls[2][8];
      auto const bias = l.controls[2][9];

      std::array<juce::Rectangle<int>, 8> const expected{
        l.clipField,       l.recModeButton,   l.speedButtons[0],
        l.speedButtons[1], l.trajectoryIcon,  f[5],
        l.speedButtons[2], l.speedButtons[3],
      };
      for (size_t i = 0; i < f.size (); ++i)
        {
          ASSERT_FALSE (f[i].isEmpty ()) << "field " << i;
          EXPECT_EQ (expected[i], f[i]) << "field " << i;
          EXPECT_LE (std::abs (f[i].getWidth () - f[0].getWidth ()), 1);
          EXPECT_LE (std::abs (f[i].getHeight () - f[0].getHeight ()), 1);
        }

      EXPECT_TRUE (f[5].contains (fade)) << "width " << width;
      EXPECT_TRUE (f[5].contains (bias)) << "width " << width;
      EXPECT_LE (fade.getRight (), bias.getX ()) << "fade, then bias";
    }

  EXPECT_TRUE (lengthKeysStandOn (BarPage::Clip));
  EXPECT_TRUE (lengthKeysStandOn (BarPage::Record));
  EXPECT_FALSE (lengthKeysStandOn (BarPage::Motion));
}

TEST (ClipSettingsLayout, TheRecPageHasNoHeadings)
{
  auto const l = layOutClipSettings (
      grownBar (defaultHeaderSize, defaultBodySize, defaultPotSize),
      defaultHeaderSize, defaultBodySize, defaultPotSize, BarPage::Record);

  EXPECT_TRUE (l.sectionLabels[0].isEmpty ());
  EXPECT_TRUE (l.recordLabel.isEmpty ());
  EXPECT_TRUE (l.lengthLabel.isEmpty ());
  EXPECT_EQ (l.sectionCards[0], l.clipContent) << "one area";
}

// The picture in CLIP's and REC's shape field keeps off the field's edge: it
// sat against it, and dots drawn on the edge read as cut off (2026-09-27).
TEST (ClipSettingsLayout, TheShapePictureKeepsOffTheFieldsEdge)
{
  juce::Rectangle<int> const field{ 10, 20, 130, 120 };
  auto const area = shapeFieldIconArea (field);

  ASSERT_FALSE (area.isEmpty ());
  EXPECT_TRUE (field.contains (area));
  EXPECT_EQ (area.getWidth (), area.getHeight ()) << "a square";
  EXPECT_EQ (area.getCentre (), field.getCentre ());
  EXPECT_LE (area.getWidth (), field.getHeight () * 3 / 4)
      << "a quarter of the shorter side left as margin";
}

// A field's caption -- dir, end, recmode, shape -- stands small in its top
// left corner, out of the way of the value in the middle (2026-09-27).
TEST (ClipSettingsLayout, AFieldsCaptionStandsInItsTopLeftCorner)
{
  juce::Rectangle<int> const field{ 10, 20, 130, 120 };
  auto const caption = fieldCaptionArea (field, 12.f);

  ASSERT_FALSE (caption.isEmpty ());
  EXPECT_TRUE (field.contains (caption));
  EXPECT_LT (caption.getX () - field.getX (), 10);
  EXPECT_LT (caption.getY () - field.getY (), 10);
  EXPECT_LE (caption.getBottom (), field.getCentreY () - 20)
      << "clear of the value in the middle";
  EXPECT_GE (caption.getHeight (), 12);
}

// tilt and roll, each with its sweep, stand on MOTION (2026-09-27): subs 10
// tilt, 11 tswp, 12 roll, 13 rswp. fade and bias stay on REC.
TEST (ClipSettingsLayout, TiltAndRollStandOnMotion)
{
  for (int sub : { 10, 11, 12, 13 })
    {
      EXPECT_TRUE (controlIsOnPage (2, sub, BarPage::Motion)) << sub;
      EXPECT_FALSE (controlIsOnPage (2, sub, BarPage::Record)) << sub;
    }
  EXPECT_TRUE (controlIsOnPage (2, 8, BarPage::Record));
  EXPECT_TRUE (controlIsOnPage (2, 9, BarPage::Record));
  EXPECT_EQ (std::string (motionKnobSpec (10).label), "tilt");
  EXPECT_EQ (std::string (motionKnobSpec (11).label), "tswp");
  EXPECT_EQ (std::string (motionKnobSpec (12).label), "roll");
  EXPECT_EQ (std::string (motionKnobSpec (13).label), "rswp");
}
