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
#include <a3-motion-ui/io/PanelGrid.hh>
#include <a3-motion-ui/io/PadFunctions.hh>

namespace a3
{

/** The controller page: the panel's pads and end keys, on the screen.
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
/** Every end-column key the PADS page shows, and where: both columns, all
 *  six rows, as on the panel -- left column top to bottom, then the right.
 *  Derived from endKeyTable (io/FunctionKeys.hh), which says what stands
 *  there; this only fixes the order the page's keys are counted in. */
constexpr std::size_t numPanelKeys = 2 * numEndRows;

constexpr std::array<EndPlace, numPanelKeys>
everyEndPlace ()
{
  std::array<EndPlace, numPanelKeys> places{};
  std::size_t i = 0;
  for (auto const side : { PanelSide::Left, PanelSide::Right })
    for (int row = 0; row < numEndRows; ++row)
      places[i++] = { side, row };
  return places;
}

constexpr std::array<EndPlace, numPanelKeys> panelKeyPlaces = everyEndPlace ();

/** What the key at `panelKeyPlaces[i]` is. */
constexpr EndKey
endKeyOnPage (std::size_t i)
{
  return endKeyAt (panelKeyPlaces[i].side, panelKeyPlaces[i].row);
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
  /** The function keys, `keys[i]` standing at `panelKeyPlaces[i]`. */
  std::array<juce::Rectangle<int>, numPanelKeys> keys;
};

/** A fingertip on the glass. What a hand can find without looking is a
 *  physical size, not a number of pixels: the same 34 px were 9 mm on a desk
 *  monitor and 4.4 mm on the device's panel (#65). */
constexpr float fingertipMillimetres = 9.f;

/** The density assumed where the display does not say: the desktop's 96 dpi,
 *  which is also what JUCE reports when X knows no physical size. 9 mm there
 *  is the 34 px this floor was as a constant. */
constexpr double unknownDisplayDpi = 96.0;

/** A fingertip in logical pixels on a display of `dpi` physical pixels per
 *  inch shown at `scale` (juce::Displays::Display::dpi and ::scale). A dpi or
 *  scale that is not a positive number is unknown and falls back to
 *  unknownDisplayDpi at scale 1. */
constexpr int
fingertipForDisplay (double dpi, double scale)
{
  auto const known = dpi > 0.0 && scale > 0.0;
  auto const logicalDpi = known ? dpi / scale : unknownDisplayDpi;
  return static_cast<int> (fingertipMillimetres / 25.4 * logicalDpi + 0.5);
}

/** The smallest thing a hand can find without looking. Nothing hit in a hurry
 *  — a pad, a page tab — is drawn narrower or shorter than this: in the dark,
 *  by a hand that is also doing something else, a target under a fingertip is
 *  not a compromise but a fault.
 *
 *  A fingertip at the unknown display's density: 34 px, which is 4.4 mm on the
 *  device's panel. Kept as the floor of every layout but the channel pots
 *  (#65): at the panel's real 9 mm (69 px) the mixer overlay has no room for
 *  four strips and the global strip's three keys overrun it, so moving the
 *  rest onto displayFingertip() is a decision of its own. */
constexpr int fingertipSize = fingertipForDisplay (unknownDisplayDpi, 1.0);

/** A fingertip on the display the window is on, in logical pixels: 9 mm,
 *  once useDisplayForFingertip() has been told the display; fingertipSize
 *  until then -- which is what every test runs against. The channel pots
 *  are sized and turned by it (#65). */
int displayFingertip ();

/** Take the fingertip from `dpi` and `scale`. True when that changed it, so
 *  the caller knows the layouts have to be worked out again. */
bool useDisplayForFingertip (double dpi, double scale);

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
/** Where panel cell `cell` stands on the page laid out in `contentArea` --
 *  the one grid PADS and the keyboard share. */
juce::Rectangle<int> panelCellBounds (juce::Rectangle<int> contentArea,
                                      PanelCell cell);

ControllerLayout layOutController (juce::Rectangle<int> contentArea,
                                   float headerSize, int buttonHeight);

}
