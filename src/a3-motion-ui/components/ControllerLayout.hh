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
 *  Laid out as a **box per clip**: channels across, slots down, and where the
 *  two meet sits one clip with its four pads. The pads inside a box keep the
 *  panel's own arrangement — play and action above, stop and settings below —
 *  and their identity comes from `padFunctionByPadIndex` / `slotForPadIndex`,
 *  the same tables the hardware is read with, so the screen cannot quietly
 *  come to mean something else.
 */
struct ControllerLayout
{
  /** The box a clip's four pads share. [channel][slot]. */
  std::array<std::array<juce::Rectangle<int>, numPadSlots>, numChannelColumns>
      clipBoxes;

  /** Where each pad is drawn, indexed the way the hardware indexes it:
   *  `pads[channel][pad]`, `pad` running 0..numPadsPerChannel-1. */
  std::array<std::array<juce::Rectangle<int>, numPadsPerChannel>,
             numChannelColumns>
      pads;
};

/** The smallest thing a hand can find without looking. Nothing hit in a hurry
 *  — a pad, a page tab — is drawn narrower or shorter than this: in the dark,
 *  by a hand that is also doing something else, a target under a fingertip is
 *  not a compromise but a fault. */
constexpr int fingertipSize = 34;

/** The height at which the pads first reach `fingertipSize`.
 *
 *  The bar is one area and both pages share it, so its height has to satisfy
 *  the hungrier of the two — see clipSettingsPreferredHeight(), which is the
 *  same idea for the clip settings.
 *
 *  `buttonHeight` is unused since the modifiers moved to the global strip;
 *  kept so the two preferred-height calls read alike at their call sites. */
int controllerPreferredHeight (float headerSize, int buttonHeight);

/** Every rectangle of the controller page, from one calculation — the same
 *  rule the clip settings bar follows, for the same reason.
 *
 *  `contentArea` is the clip part **under its header row**
 *  (ClipSettingsLayout::clipContent), not the whole of it. `headerSize` and
 *  `buttonHeight` are unused since the header became the bar's business and
 *  the modifiers moved to the global strip; kept so this reads like
 *  layOutClipSettings() at its call site. */
ControllerLayout layOutController (juce::Rectangle<int> contentArea,
                                   float headerSize, int buttonHeight);

}
