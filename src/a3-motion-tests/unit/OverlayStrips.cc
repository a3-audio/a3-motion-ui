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

#include <a3-motion-ui/components/FingerLatch.hh>
#include <a3-motion-ui/components/OverlaySideStrips.hh>
#include <a3-motion-ui/components/TouchControl.hh>

using namespace a3;

// The two overlays that are a list: the strips walk it and turn the value of
// the row they are on.
TEST (OverlayStrips, TheMenuAndTheSkinEditorGetTheStrips)
{
  EXPECT_TRUE (sideStripsHaveAList (true, false, false, SphereOverlay::None));
  EXPECT_TRUE (sideStripsHaveAList (false, true, false, SphereOverlay::None));
}

// The skin panel stands in front of the menu without being a list: its bars
// are dragged sideways, and a strip beside it would be a fifth of the window
// taking drags for a menu nobody can see. The full list, opened from the
// panel, is a list again.
TEST (OverlayStrips, TheSkinPanelIsNotAList)
{
  EXPECT_TRUE (menuListIsInFront (true, false));
  EXPECT_FALSE (menuListIsInFront (true, true));
  EXPECT_FALSE (menuListIsInFront (false, false));

  EXPECT_FALSE (sideStripsHaveAList (menuListIsInFront (true, true), false,
                                     false, SphereOverlay::None));
  EXPECT_TRUE (sideStripsHaveAList (menuListIsInFront (true, true), true,
                                    false, SphereOverlay::None));
}

// Nothing open, nothing to walk.
TEST (OverlayStrips, WithNoListOpenThereAreNoStrips)
{
  EXPECT_FALSE (sideStripsHaveAList (false, false, false, SphereOverlay::None));
}

// The one this exists for. MAINMIX is reachable whatever else is up, so
// the mixer can stand over an open menu -- and it is then the overlay in
// front. The strips are a fifth of the window each: the master column on the
// right and the first channel on the left. Left up, they took every touch
// meant for those two columns and gave it to the menu's highlighted row,
// which changed and was applied on release.
TEST (OverlayStrips, TheMixerInFrontTakesTheStripsAwayFromTheMenu)
{
  EXPECT_FALSE (sideStripsHaveAList (true, false, false, SphereOverlay::MainMix));
  EXPECT_FALSE (sideStripsHaveAList (false, true, false, SphereOverlay::MainMix));
  EXPECT_FALSE (sideStripsHaveAList (true, true, false, SphereOverlay::MainMix));
}

// The mixer is opened from outside the overlay chain, so it can also be the
// only thing on screen. No list either way.
TEST (OverlayStrips, TheMixerAloneHasNoListEither)
{
  EXPECT_FALSE (sideStripsHaveAList (false, false, false, SphereOverlay::MainMix));
}

// The same fault one level down, and the reason this grew a fourth flag.
// openColourPicker() hides the skin editor but leaves _skinEditorOpen true --
// it is still the page underneath -- so a predicate that did not ask about the
// picker went on answering for the editor. The strips then came to the front
// over the picker, sized to the hidden editor's panel, and a drag in the outer
// fifths scrolled a list nobody could see instead of moving hue.
TEST (OverlayStrips, TheColourPickerInFrontTakesTheStripsFromTheSkinEditor)
{
  EXPECT_FALSE (sideStripsHaveAList (true, true, true, SphereOverlay::None));
  EXPECT_FALSE (sideStripsHaveAList (false, true, true, SphereOverlay::None));
  EXPECT_FALSE (sideStripsHaveAList (true, false, true, SphereOverlay::None));
}

// And with both of the two in front, the mixer is the innermost room --
// toggleGlobalSettings() closes it first for the same reason. Neither of them
// is a list, so the answer is the same whichever is nearer the eye.
TEST (OverlayStrips, TheMixerOverTheColourPickerIsStillNoList)
{
  EXPECT_FALSE (sideStripsHaveAList (false, true, true, SphereOverlay::MainMix));
  EXPECT_FALSE (sideStripsHaveAList (false, false, true, SphereOverlay::MainMix));
}

// FILES stands where the mixer does since 2026-09-27, over whatever else is
// open, and it is a list that scrolls under the finger by itself. Its edges
// are its tabs, its rows and its keys -- a strip over them would take those
// touches and scroll the menu behind it.
TEST (OverlayStrips, TheBrowserInFrontTakesTheStripsAwayFromTheMenu)
{
  EXPECT_FALSE (sideStripsHaveAList (true, false, false, SphereOverlay::Files));
  EXPECT_FALSE (sideStripsHaveAList (false, true, false, SphereOverlay::Files));
  EXPECT_FALSE (sideStripsHaveAList (false, false, false, SphereOverlay::Files));
}

// Both strips scroll, and neither changes a value: "kein edit ohne
// eingabemaske, das kollidiert mit scroll." The right strip used to turn the
// highlighted row and apply it on release -- an edit without a mask, one drag
// away from a scroll.
TEST (OverlayStrips, BothStripsScrollAndNeitherEdits)
{
  OverlaySideStrips strips;
  strips.setBounds (0, 0, 768, 600);
  strips.setPanel ({ 150, 0, 468, 600 });

  int scrolled = 0;
  strips.onBrowse = [&scrolled] (int) { ++scrolled; };

  int zones = 0;
  for (auto *child : strips.getChildren ())
    if (auto *zone = dynamic_cast<TouchControl *> (child); zone != nullptr)
      {
        ++zones;
        ASSERT_TRUE (zone->onDragIncrement);
        zone->onDragIncrement (0, -1, 1);
        if (zone->onDragEnd)
          zone->onDragEnd (0, -1);
      }

  EXPECT_EQ (zones, 2);
  EXPECT_EQ (scrolled, 2);
}

TEST (OverlayStrips, TheStripsShareTheMenuListsLatch)
{
  OverlaySideStrips strips;
  for (auto *child : strips.getChildren ())
    if (auto *zone = dynamic_cast<TouchControl *> (child); zone != nullptr)
      EXPECT_EQ (zone->fingerLatch (),
                 &FingerLatch::forGroup (FingerLatch::menuList));
}

// Beside the list the finger moves the page as far as it would on it: the
// page it sits around says how tall a row is.
TEST (OverlayStrips, TheStripsTakeTheirStepFromThePage)
{
  OverlaySideStrips strips;
  strips.setPixelsPerStep (34);
  for (auto *child : strips.getChildren ())
    if (auto *zone = dynamic_cast<TouchControl *> (child); zone != nullptr)
      EXPECT_EQ (zone->pixelsPerStep (), 34);
}

// Back and close stand only on the main menu and what it opens (the skin
// editor, the colour picker). FILES, MIXER and PADS over the sphere close with
// their own key, a second tap -- the pair was redundant there and took the
// band FILES now puts its keys in (maintainer, 2026-09-27).
TEST (OverlayStrips, BackAndCloseStandOnlyOnTheMenu)
{
  auto const none = SphereOverlay::None;
  EXPECT_TRUE (overlayKeysAreShown (true, false, false, none));
  EXPECT_TRUE (overlayKeysAreShown (false, true, false, none));
  EXPECT_TRUE (overlayKeysAreShown (false, false, true, none));
  EXPECT_FALSE (overlayKeysAreShown (false, false, false, none));
  EXPECT_FALSE (overlayKeysAreShown (false, false, false, SphereOverlay::Files))
      << "FILES alone";
  EXPECT_FALSE (overlayKeysAreShown (true, false, false, SphereOverlay::Files))
      << "FILES in front of the menu: its keys would sit on FILES' own";
}
