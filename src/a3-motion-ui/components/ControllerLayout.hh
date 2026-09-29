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

#pragma once

#include <JuceHeader.h>

#include <a3-motion-ui/components/ClipSettingsLayout.hh>
#include <a3-motion-ui/io/FunctionKeys.hh>
#include <a3-motion-ui/io/PadFunctions.hh>

namespace a3
{

/** The controller page: the panel's pads, on the screen.
 *
 *  The device cannot be played without pads, and a plain build has no panel
 *  (`HARDWARE_INTERFACE_ENABLED` is off by default), so without these the
 *  standard build cannot start a single clip. See
 *  issues/a3-motion-ui-pads-not-reachable-from-the-gui.md.
 *
 *  Laid out as a **box per channel**, each holding its clip's eight pads in
 *  the panel's own arrangement — two columns of four, Play top left, Page top
 *  right, the six actions below — and their identity comes from
 *  `padFunctionByPadIndex` / `actionButtonForPad`,
 *  the same tables the hardware is read with, so the screen cannot quietly
 *  come to mean something else.
 */
/** The scene block is shaped like a channel (2026-09-27): one scene pad per
 *  pad, `scenes[0][pad]`, firing that pad across every channel -- Stop all
 *  where a channel has Page. */
constexpr std::size_t numSceneRows = numPadsPerChannel;

/** Which end of the panel a function key stands at. */
enum class PanelSide
{
  Left,
  Right,
};

/** A function key's place on the PADS page, said in the panel's own terms:
 *  which end column, and which row -- 0 at the top, the pads standing in rows
 *  2-5 between the two columns (InputOutputAdapterV3.hh). What the key does
 *  comes from its row through `functionKeyOrder`, the way the panel is wired.
 */
struct PanelKeyPlace
{
  PanelSide side;
  int row;
};

/** Every function key the PADS page shows, and where (2026-09-28).
 *
 *  The right-hand column is the panel's col9, all six. On the left the scene
 *  block -- the screen's own, the panel has none -- stands where col0's rows
 *  2-5 would be, so only col0's top two rows, TAP and clock, stand over it.
 *  Every key is reachable on the right; the left pair is the reach for the
 *  left hand, as on the panel. The one table to change if that is wrong. */
constexpr std::array<PanelKeyPlace, 8> panelKeyPlaces{ {
    { PanelSide::Right, 0 },
    { PanelSide::Right, 1 },
    { PanelSide::Right, 2 },
    { PanelSide::Right, 3 },
    { PanelSide::Right, 4 },
    { PanelSide::Right, 5 },
    { PanelSide::Left, 0 },
    { PanelSide::Left, 1 },
} };

constexpr std::size_t numPanelKeys = panelKeyPlaces.size ();

/** What the key at `panelKeyPlaces[i]` is. */
constexpr FunctionKey
panelKeyFunction (std::size_t i)
{
  return functionKeyOrder[static_cast<std::size_t> (panelKeyPlaces[i].row)];
}

struct ControllerLayout
{
  /** The box a channel's eight pads share. [channel][slot], one slot. */
  std::array<std::array<juce::Rectangle<int>, numPadSlots>, numChannelColumns>
      clipBoxes;

  /** Where each pad is drawn, indexed the way the hardware indexes it:
   *  `pads[channel][pad]`, `pad` running 0..numPadsPerChannel-1. */
  std::array<std::array<juce::Rectangle<int>, numPadsPerChannel>,
             numChannelColumns>
      pads;
  /** The block left of the channels: `scenes[0][pad]`, lined up with the pad
   *  it fires and as wide as one, so it reads as one more pad rather than a
   *  margin. */
  std::array<std::array<juce::Rectangle<int>, numSceneRows>, numPadSlots>
      scenes;
  /** The function keys, `keys[i]` standing at `panelKeyPlaces[i]`. */
  std::array<juce::Rectangle<int>, numPanelKeys> keys;
};

/** The smallest thing a hand can find without looking. Nothing hit in a hurry
 *  — a pad, a page tab — is drawn narrower or shorter than this: in the dark,
 *  by a hand that is also doing something else, a target under a fingertip is
 *  not a compromise but a fault. */
constexpr int fingertipSize = 34;

/** How small a row of text may get before it stops being readable, and how
 *  little room the channel strips and the sphere may be left with.
 *
 *  Not skin values: a skin decides how the device looks, these decide whether
 *  the window can be laid out at all. They used to live beside the padding --
 *  a skin value -- in one struct that held three unrelated kinds of thing,
 *  which is why that struct was dissolved rather than extended. */
constexpr float minimumRowHeight = 35.f;
constexpr float minimumChannelWidth = 100.f;
constexpr float minimumMotionHeight = 100.f;

/** Every rectangle of the controller page, from one calculation — the same
 *  rule the clip settings bar follows, for the same reason.
 *
 *  `contentArea` is the area over the sphere (2026-09-27). The page is the
 *  panel on screen and keeps its proportions: square cells in six rows, the
 *  pads in the bottom four, centred in whatever area it is given
 *  (2026-09-28). `headerSize` and `buttonHeight` are unused; kept so this
 *  reads like layOutClipSettings() at its call site. */
ControllerLayout layOutController (juce::Rectangle<int> contentArea,
                                   float headerSize, int buttonHeight);

}
