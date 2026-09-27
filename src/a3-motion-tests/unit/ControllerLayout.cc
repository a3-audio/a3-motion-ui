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

#include <set>
#include <vector>

#include <JuceHeader.h>

#include <a3-motion-ui/components/ControllerLayout.hh>

using namespace a3;

namespace
{
// Three quarters of the device's width, the height the bar asks for at the
// shipped skin — the same area the clip settings occupy, since the controller
// page replaces them and leaves the global strip standing.
constexpr int barWidth = 768 * 3 / 4;
constexpr int barHeight = 265;
constexpr float headerSize = 18.f;
constexpr int buttonHeight = 34;

juce::Rectangle<int> const area{ 0, 0, barWidth, barHeight };

ControllerLayout
defaultLayout ()
{
  return layOutController (area, headerSize, buttonHeight);
}
}

// The one that matters. A pad on the screen means what the same pad on the
// panel means, because both read it out of padFunctionByPadIndex — not because
// somebody arranged the picture to match and will keep it matching. The
// grouping the maintainer asked for falls out of it: channel across, slot
// down, and every pad inside the box of the clip it fires.
TEST (ControllerLayout, EveryPadSitsInTheBoxOfTheClipItFires)
{
  auto const l = defaultLayout ();

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
      {
        index_t const slot = 0; // one clip per channel since 2026-09-27
        auto const box = l.clipBoxes[channel][slot];

        // Asked first, because an empty rectangle is contained in an empty
        // rectangle and a containment test on nothing proves nothing.
        ASSERT_FALSE (l.pads[channel][pad].isEmpty ())
            << "channel " << channel << ", pad " << pad << " has no area";

        EXPECT_TRUE (box.contains (l.pads[channel][pad]))
            << "channel " << channel << ", pad " << pad << ": "
            << l.pads[channel][pad].toString () << " outside slot " << slot
            << "'s box " << box.toString ();
      }
}


// The bar is one area and both pages share it, so the page that needs more
// room has to be able to say so. At the height it asks for, every pad is a
// target a hand can find without looking.
TEST (ControllerLayout, AtItsOwnHeightEveryPadIsAFingertipAcross)
{
  auto const height = controllerPreferredHeight (headerSize, buttonHeight);
  auto const l
      = layOutController ({ 0, 0, barWidth, height }, headerSize, buttonHeight);

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
      {
        auto const p = l.pads[channel][pad];
        EXPECT_GE (p.getWidth (), fingertipSize)
            << "channel " << channel << ", pad " << pad;
        EXPECT_GE (p.getHeight (), fingertipSize)
            << "channel " << channel << ", pad " << pad;
      }
}


// A bar smaller than the page wants is a layout bug elsewhere, but it must not
// turn into rectangles with negative width. Those are not small hit areas,
// they are hit areas that behave unpredictably — JUCE will happily hand you a
// component whose right edge is left of its left one.
TEST (ControllerLayout, TooLittleRoomMakesSmallRectanglesNotBrokenOnes)
{
  for (int height : { 0, 20, 60, 120, barHeight })
    {
      auto const l
          = layOutController ({ 0, 0, barWidth, height }, headerSize,
                              buttonHeight);

      auto const sane = [height] (juce::Rectangle<int> r, char const *what) {
        EXPECT_GE (r.getWidth (), 0) << what << " at height " << height;
        EXPECT_GE (r.getHeight (), 0) << what << " at height " << height;
      };

      for (index_t channel = 0; channel < numChannelColumns; ++channel)
        {
          for (index_t slot = 0; slot < numPadSlots; ++slot)
            sane (l.clipBoxes[channel][slot], "a clip box");
          for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
            sane (l.pads[channel][pad], "a pad");
        }

    }
}


// ── Guards ────────────────────────────────────────────────────────────────
// The three above drove the code out. These hold it there: each one names the
// change that would break it, because a test nobody can break is decoration.

// Breaks if the grid is transposed, or if a channel's column is computed from
// anything but its index. Colour identifies a channel on this device; its
// position has to agree with the four channel strips above the bar.
TEST (ControllerLayout, ChannelsRunAcrossAndSlotsRunDown)
{
  auto const l = defaultLayout ();

  for (index_t slot = 0; slot < numPadSlots; ++slot)
    for (index_t channel = 1; channel < numChannelColumns; ++channel)
      EXPECT_GE (l.clipBoxes[channel][slot].getX (),
                 l.clipBoxes[channel - 1][slot].getRight ())
          << "channel " << channel << " is not right of " << (channel - 1);

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t slot = 1; slot < numPadSlots; ++slot)
      EXPECT_GE (l.clipBoxes[channel][slot].getY (),
                 l.clipBoxes[channel][slot - 1].getBottom ())
          << "slot " << slot << " is not below " << (slot - 1);
}

// Breaks on an off-by-one in the cell arithmetic — the kind that leaves two
// pads sharing an edge pixel, where a fingertip lands on whichever JUCE asks
// first and the wrong clip fires.
TEST (ControllerLayout, NoTwoPadsOverlap)
{
  auto const l = defaultLayout ();

  std::vector<juce::Rectangle<int> > all;
  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
      all.push_back (l.pads[channel][pad]);

  for (size_t i = 0; i < all.size (); ++i)
    for (size_t j = i + 1; j < all.size (); ++j)
      EXPECT_FALSE (all[i].intersects (all[j]))
          << all[i].toString () << " overlaps " << all[j].toString ();
}

// Breaks if padCellInBox() ever maps two pads to one cell -- then one of the
// eight would be drawn on top of another and the channel would be missing a
// function with nothing to show for it.
TEST (ControllerLayout, AChannelsEightPadsTakeEightCells)
{
  auto const l = defaultLayout ();

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    {
      std::set<std::pair<int, int> > cells;
      for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
        cells.insert ({ l.pads[channel][pad].getX (),
                        l.pads[channel][pad].getY () });
      EXPECT_EQ (cells.size (), 8u) << "channel " << channel;
    }
}

// As the panel stands (2026-09-27): two columns of four, pads 0..3 down the
// left, 4..7 down the right -- Play top left, Page top right, the six below.
TEST (ControllerLayout, ThePadsStandAsThePanelDoes)
{
  auto const l = defaultLayout ();
  auto const &p = l.pads[0];

  for (index_t pad = 1; pad < 4; ++pad)
    {
      EXPECT_EQ (p[pad].getX (), p[0].getX ()) << pad;
      EXPECT_GT (p[pad].getY (), p[pad - 1].getY ()) << pad;
      EXPECT_EQ (p[pad + 4].getY (), p[pad].getY ()) << pad;
      EXPECT_GT (p[pad + 4].getX (), p[pad].getX ()) << pad;
    }
}

// The scene block is shaped like a channel, left of the four: a pad in it
// fires the same pad on every channel (Stop all where a channel has Page).
TEST (ControllerLayout, TheScenesAreAChannelOfTheirOwn)
{
  auto const l = defaultLayout ();
  for (std::size_t pad = 0; pad < numSceneRows; ++pad)
    {
      auto const scene = l.scenes[0][pad];
      ASSERT_FALSE (scene.isEmpty ()) << pad;
      EXPECT_LT (scene.getRight (), l.clipBoxes[0][0].getX () + 1) << pad;
      EXPECT_EQ (scene.getY (), l.pads[0][pad].getY ()) << pad;
      EXPECT_EQ (scene.getWidth (), l.pads[0][pad].getWidth ()) << pad;
    }
}

// Breaks if any of it grows past the area it was handed — which on this bar
// means drawing over the sphere or off the bottom of the screen.
TEST (ControllerLayout, EverythingStaysInsideTheBar)
{
  auto const l = defaultLayout ();

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    {
      for (index_t slot = 0; slot < numPadSlots; ++slot)
        EXPECT_TRUE (area.contains (l.clipBoxes[channel][slot]));
      for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
        EXPECT_TRUE (area.contains (l.pads[channel][pad]));
    }

}


// -- The scene column --------------------------------------------------------
//
// A block shaped like a channel, left of the four: each of its pads fires the
// same pad across every channel -- Play all, Stop all where a channel has
// Page, and each action on every channel. Asked for on 2026-09-22 as a column,
// the way a scene is launched on a deck-side controller; a block since one clip
// per channel (2026-09-27).

TEST (ControllerLayout, TheScenePadsStandLeftOfEveryChannel)
{
  auto const layout = defaultLayout ();
  for (index_t slot = 0; slot < numPadSlots; ++slot)
    for (auto const &scene : layout.scenes[slot])
      {
        EXPECT_FALSE (scene.isEmpty ());
        EXPECT_LE (scene.getRight (), layout.clipBoxes[0][slot].getX ());
      }
}

TEST (ControllerLayout, EachScenePadLinesUpWithThePadRowItFires)
{
  auto const layout = defaultLayout ();
  for (index_t slot = 0; slot < numPadSlots; ++slot)
    for (std::size_t row = 0; row < numSceneRows; ++row)
      {
        // The scene block is shaped like a channel: its pad `row` stands
        // level with every channel's pad `row` (2026-09-27).
        auto const pad = layout.pads[0][row];
        EXPECT_EQ (layout.scenes[slot][row].getY (), pad.getY ());
        EXPECT_EQ (layout.scenes[slot][row].getHeight (), pad.getHeight ());
      }
}

// It reads as one more pad, not as a margin or a heading.
TEST (ControllerLayout, AScenePadIsAsWideAsAPad)
{
  auto const layout = defaultLayout ();
  EXPECT_EQ (layout.scenes[0][0].getWidth (), layout.pads[0][0].getWidth ());
}

TEST (ControllerLayout, TheScenePadsStayInsideTheBarAndOffThePads)
{
  auto const layout = defaultLayout ();
  for (index_t slot = 0; slot < numPadSlots; ++slot)
    for (auto const &scene : layout.scenes[slot])
      {
        EXPECT_TRUE (area.contains (scene));
        for (index_t channel = 0; channel < numChannelColumns; ++channel)
          for (auto const &pad : layout.pads[channel])
            EXPECT_FALSE (scene.intersects (pad));
      }
}
