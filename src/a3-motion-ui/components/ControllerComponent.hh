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

#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/TouchControl.hh>
#include <a3-motion-ui/theme/ThemedComponent.hh>

namespace a3
{

/** The panel's pads, on the screen — the bar's second page.
 *
 *  It decides nothing. A press goes out as (channel, pad) and lands in the
 *  same `handlePadPress()` the hardware reaches; a colour comes in already
 *  worked out by the same loop that writes the panel's LEDs. What the screen
 *  shows is therefore what the panel shows, not a second reading of the same
 *  state that agrees with it most of the time.
 */
class ControllerComponent : public juce::Component, public ThemedComponent
{
public:
  ControllerComponent ();
  ~ControllerComponent () override;

  void paint (juce::Graphics &g) override;
  void resized () override;
  /** The page's geometry is worked out from the theme's header size, so a
   *  skin change has to re-lay it out, not only repaint it. */
  void applyTheme () override;

  /** What this pad looks like right now — empty, idle, armed, running. Pushed
   *  from padLEDCallback(), which computes it once for both displays. */
  void setPadColour (index_t channel, index_t pad, juce::Colour colour);

  /** Whether the modifiers are down, from wherever they were pressed: the
   *  panel's keys and these two feed one state. */
  void setShiftHeld (bool held);
  void setRecordHeld (bool held);

  /** A pad went down or came up. Both matter: Shift+Action runs a preview for
   *  as long as it is held, so a press without its release would leave the
   *  channel previewing forever. */
  std::function<void (index_t channel, index_t pad)> onPadPressed;
  std::function<void (index_t channel, index_t pad)> onPadReleased;

  /** Held, not latched. Shift+Action has no meaning without a release. */
  std::function<void (bool held)> onShiftHeld;
  std::function<void (bool held)> onRecordHeld;

private:
  void paintPad (juce::Graphics &g, juce::Rectangle<int> bounds,
                 index_t channel, index_t pad);
  void paintModifier (juce::Graphics &g, juce::Rectangle<int> bounds,
                      juce::String const &label, bool held,
                      juce::Colour heldColour);

  ControllerLayout _layout;

  std::array<std::array<juce::Colour, numPadsPerChannel>, numChannelColumns>
      _padColours;

  bool _shiftHeld = false;
  bool _recordHeld = false;

  std::array<std::array<std::unique_ptr<TouchControl>, numPadsPerChannel>,
             numChannelColumns>
      _padTouch;
  std::unique_ptr<TouchControl> _shiftTouch;
  std::unique_ptr<TouchControl> _recordTouch;
};

}
