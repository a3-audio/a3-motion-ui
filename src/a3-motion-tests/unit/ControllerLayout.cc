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

#include <algorithm>
#include <cstdlib>
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

// What the page is given on the device: the area over the sphere, the whole
// width of the 768 x 1024 screen between the status bar and the channel row.
juce::Rectangle<int> const sphereArea{ 0, 0, 768, 586 };

// A window taller than it is wide, and one far wider than tall: the page has
// to keep the panel's proportions in both rather than stretch to fill them.
juce::Rectangle<int> const tallArea{ 0, 0, 768, 1400 };
juce::Rectangle<int> const wideArea{ 0, 0, 1800, 400 };

std::vector<juce::Rectangle<int> >
everyTarget (ControllerLayout const &l)
{
  std::vector<juce::Rectangle<int> > all;
  for (auto const &channel : l.pads)
    for (auto const &pad : channel)
      all.push_back (pad);
  for (auto const &scene : l.scenes[0])
    all.push_back (scene);
  for (auto const &key : l.keys)
    all.push_back (key);
  return all;
}

/** The keys that stand at one end of the page, by the panel row they are. */
std::vector<std::pair<int, juce::Rectangle<int> > >
keysAt (ControllerLayout const &l, PanelSide side)
{
  std::vector<std::pair<int, juce::Rectangle<int> > > out;
  for (std::size_t i = 0; i < numPanelKeys; ++i)
    if (panelKeyPlaces[i].side == side)
      out.emplace_back (panelKeyPlaces[i].row, l.keys[i]);
  return out;
}

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


// The page stands over the sphere (2026-09-27). At the size that area has on
// the device, every pad and every key is a target a hand can find without
// looking.
TEST (ControllerLayout, OverTheSphereEveryTargetIsAFingertipAcross)
{
  auto const l = layOutController (sphereArea, headerSize, buttonHeight);

  for (auto const &r : everyTarget (l))
    {
      EXPECT_GE (r.getWidth (), fingertipSize) << r.toString ();
      EXPECT_GE (r.getHeight (), fingertipSize) << r.toString ();
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

// The scene column is the panel's left column (2026-09-30: "in PADS muss die
// zweite reihe von links weg"): one column, the left half of a channel -- Play
// all and A1, A3, A5 across every channel -- where the panel's col0 stands,
// under TAP and the clock key.
TEST (ControllerLayout, TheScenesAreThePanelsLeftColumn)
{
  auto const l = defaultLayout ();
  auto const left = keysAt (l, PanelSide::Left);
  ASSERT_FALSE (left.empty ());

  ASSERT_EQ (numSceneRows, 4u);
  for (std::size_t pad = 0; pad < numSceneRows; ++pad)
    {
      auto const scene = l.scenes[0][pad];
      ASSERT_FALSE (scene.isEmpty ()) << pad;
      EXPECT_EQ (scene.getX (), left.front ().second.getX ()) << pad;
      EXPECT_EQ (scene.getY (), l.pads[0][pad].getY ()) << pad;
      // The pad it fires is in a channel's left column.
      EXPECT_EQ (l.pads[0][pad].getX (), l.clipBoxes[0][0].getX ()) << pad;
    }
}

// The page is the panel, key for key: 44 targets, ten columns across -- the
// scene column, four channels of two, the right-hand keys -- so a key on the
// screen stands where its key on the panel does.
TEST (ControllerLayout, ThePageHoldsThePanelsFortyFourKeys)
{
  auto const l = defaultLayout ();
  EXPECT_EQ (everyTarget (l).size (), 44u);

  auto const cell = l.pads[0][0].getWidth ();
  auto const step = l.pads[0][4].getX () - l.pads[0][0].getX ();
  auto const right = keysAt (l, PanelSide::Right);
  ASSERT_FALSE (right.empty ());
  EXPECT_EQ (right.front ().second.getX () - l.scenes[0][0].getX (), 9 * step);
  EXPECT_GT (step, cell);
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


// -- The panel, not a grid stretched to fill ---------------------------------
//
// Asked for on 2026-09-28: "die PADS sind gestretcht". A pad is square on the
// panel, so it is square here, and the page keeps the panel's proportions in
// whatever area it is given instead of pulling the pads tall.

TEST (ControllerLayout, EveryPadSceneAndKeyIsSquare)
{
  for (auto const &given : { area, sphereArea, tallArea, wideArea })
    for (auto const &r : everyTarget (layOutController (given, headerSize,
                                                        buttonHeight)))
      {
        ASSERT_FALSE (r.isEmpty ()) << given.toString ();
        EXPECT_EQ (r.getWidth (), r.getHeight ())
            << r.toString () << " in " << given.toString ();
      }
}

// Not stretched, and not shrunk into a corner either: the panel stands in the
// middle of its area and fills it in the direction that runs out first --
// what is left over there is less than half a pad.
TEST (ControllerLayout, ThePanelStandsCentredAndFillsOneWay)
{
  for (auto const &given : { area, sphereArea, tallArea, wideArea })
    {
      auto const l = layOutController (given, headerSize, buttonHeight);
      auto const targets = everyTarget (l);
      auto whole = targets.front ();
      for (auto const &r : targets)
        whole = whole.getUnion (r);

      auto const left = whole.getX () - given.getX ();
      auto const right = given.getRight () - whole.getRight ();
      auto const top = whole.getY () - given.getY ();
      auto const bottom = given.getBottom () - whole.getBottom ();
      EXPECT_LE (std::abs (left - right), 1) << given.toString ();
      EXPECT_LE (std::abs (top - bottom), 1) << given.toString ();

      auto const pad = l.pads[0][0].getWidth ();
      EXPECT_LT (std::min (left + right, top + bottom), pad / 2)
          << given.toString ();
    }
}

// -- The panel's function keys -----------------------------------------------
//
// Asked for on 2026-09-28: "rechts fehlt noch eine reihe mit 6 vertikalen und
// links über den grauen fehlen noch 2 buttons". On the panel the six keys are
// a column at each end, rows 0-5, and the pads stand in rows 2-5 between them
// (InputOutputAdapterV3.hh). A key does what its row does on the panel --
// functionKeyOrder -- wherever it stands.

TEST (ControllerLayout, TheRightHandColumnIsAllSixKeysTopToBottom)
{
  auto const l = defaultLayout ();
  auto const right = keysAt (l, PanelSide::Right);

  ASSERT_EQ (right.size (), static_cast<std::size_t> (numFunctionKeys));
  for (std::size_t i = 0; i < right.size (); ++i)
    {
      EXPECT_EQ (right[i].first, static_cast<int> (i));
      EXPECT_EQ (right[i].second.getX (), right[0].second.getX ());
      EXPECT_EQ (right[i].second.getWidth (), l.pads[0][0].getWidth ());
      if (i > 0)
        {
          EXPECT_GE (right[i].second.getY (),
                     right[i - 1].second.getBottom ());
        }
    }

  EXPECT_GE (right[0].second.getX (),
             l.clipBoxes[numChannelColumns - 1][0].getRight ());
}

TEST (ControllerLayout, TheKeysStandInThePanelsRows)
{
  auto const l = defaultLayout ();

  // Rows 2-5 beside the pads, level with them; rows 0 and 1 above them.
  for (auto const &[row, key] : keysAt (l, PanelSide::Right))
    {
      if (row < 2)
        {
          EXPECT_LE (key.getBottom (), l.pads[0][0].getY ()) << row;
          continue;
        }
      EXPECT_EQ (key.getY (),
                 l.pads[0][static_cast<index_t> (row - 2)].getY ())
          << row;
    }
}

TEST (ControllerLayout, TwoKeysStandOverTheSceneBlockAtTheOuterEdge)
{
  auto const l = defaultLayout ();
  auto const left = keysAt (l, PanelSide::Left);
  auto const right = keysAt (l, PanelSide::Right);

  ASSERT_EQ (left.size (), 2u);
  for (auto const &[row, key] : left)
    {
      EXPECT_EQ (key.getX (), l.scenes[0][0].getX ()) << row;
      EXPECT_LE (key.getBottom (), l.scenes[0][0].getY ()) << row;
      EXPECT_EQ (key.getY (), right[static_cast<std::size_t> (row)].second.getY ())
          << row;
    }
}

// The top two rows of the panel's left column: TAP and clock.
TEST (ControllerLayout, TheLeftKeysAreTapAndClock)
{
  std::vector<FunctionKey> left;
  for (std::size_t i = 0; i < numPanelKeys; ++i)
    if (panelKeyPlaces[i].side == PanelSide::Left)
      left.push_back (panelKeyFunction (i));

  EXPECT_EQ (left, (std::vector<FunctionKey>{ FunctionKey::Tap,
                                              FunctionKey::ClockMode }));
}

TEST (ControllerLayout, NoTargetOverlapsAnotherOrLeavesTheArea)
{
  for (auto const &given : { area, sphereArea, tallArea, wideArea })
    {
      auto const all
          = everyTarget (layOutController (given, headerSize, buttonHeight));
      for (std::size_t i = 0; i < all.size (); ++i)
        {
          EXPECT_TRUE (given.contains (all[i])) << all[i].toString ();
          for (std::size_t j = i + 1; j < all.size (); ++j)
            EXPECT_FALSE (all[i].intersects (all[j]))
                << all[i].toString () << " overlaps " << all[j].toString ();
        }
    }
}
