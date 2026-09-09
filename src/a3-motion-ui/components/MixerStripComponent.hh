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

#include <array>
#include <functional>
#include <memory>

#include <JuceHeader.h>

#include <a3-motion-ui/components/MixerLayout.hh>
#include <a3-motion-ui/components/MixerState.hh>
#include <a3-motion-ui/components/TouchControl.hh>
#include <a3-motion-ui/theme/ThemedComponent.hh>

namespace a3
{

/** One channel's mixer strip, as the bar's MIX page.
 *
 *  The overlay is the whole mixer when you want it; this is the one-handed
 *  reach to the channel you are already looking at, without laying anything
 *  over the sphere. So it carries one strip and no summing section — those
 *  are one tap away on the status bar's MIX key.
 *
 *  MixerComponent's smaller sibling, and deliberately built the same way: a
 *  pure layout (`layOutMixerStrip`) says where the seven controls sit,
 *  paint() draws into it and resized() lays the TouchControls on the same
 *  rectangles, and paintMixerChannelControl() draws each one, so a control is
 *  the same picture on both pages.
 *
 *  It decides nothing about values. A gesture leaves as a callback and comes
 *  back as a repaint once MixerState has been told — the same route the
 *  overlay's gestures take, into the same handlers.
 */
class MixerStripComponent : public juce::Component, public ThemedComponent
{
public:
  explicit MixerStripComponent (MixerState &state);
  ~MixerStripComponent () override;

  void paint (juce::Graphics &g) override;
  void resized () override;
  /** The geometry is worked out from the skin's pot size and fonts, so a skin
   *  change has to re-lay this out, not merely repaint it. */
  void applyTheme () override;

  /** Whose strip is on show: the channel of the clip the bar describes.
   *
   *  Pushed in rather than read from anywhere, because the one place that
   *  knows which clip the bar is on is A3MotionUIComponent — and a page that
   *  worked it out for itself would be a second answer to a question that
   *  already has one. */
  void setChannel (int channel);
  int getChannel () const;

  /** The same two callbacks the overlay offers, so both pages land in one
   *  pair of handlers. What one increment is worth is the caller's to say. */
  std::function<void (int channel, MixerControl, int steps)> onChannelDragged;
  std::function<void (int channel, MixerControl)> onChannelTapped;

private:
  MixerState &_state;
  MixerLayout _layout;
  ControlMetrics _metrics{};

  /** The shown channel is the component's, not the hit area's: a face tapped
   *  in the header swaps the strip under a finger that is already on the
   *  page, and an identity baked into seven TouchControls would then be a
   *  second thing to keep in step. The identity carries only where a control
   *  sits in mixerControlOrder. */
  int _channel = 0;

  std::array<std::unique_ptr<TouchControl>, numMixerControls> _touch;
};

}
