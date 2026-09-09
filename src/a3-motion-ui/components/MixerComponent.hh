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
#include <a3-motion-ui/components/VuMeter.hh>
#include <a3-motion-ui/theme/ThemedComponent.hh>

namespace a3
{

/** One of a channel's seven controls, drawn into `bounds`.
 *
 *  A free function beside paintBarKnob, and for the same reason: the overlay
 *  stands four of these strips upright and the bar's MIX tab lays one across,
 *  and the same control has to be the same picture in both or a hand learns
 *  two mixers. Every continuous control here is a knob and the two keys are
 *  keys — nothing measures the cell to decide which.
 *
 *  `value` is 0..1 and `isOn` answers for the two-valued controls; the caller
 *  reads them off MixerState, so this stays a painter and knows no state. */
void paintMixerChannelControl (juce::Graphics &g, juce::Rectangle<int> bounds,
                               ControlMetrics metrics, juce::Colour colour,
                               MixerControl control, float value, bool isOn);

/** The software mixer, over the sphere.
 *
 *  Built the way the controller page is: a pure layout says where everything
 *  sits, paint() draws into it and resized() lays the TouchControls on the
 *  same rectangles, so the picture and the target cannot disagree.
 *
 *  It decides nothing about values. Every gesture leaves as a callback and
 *  comes back as a repaint once MixerState has been told — which is what
 *  keeps the one place that knows a value has been *touched* (and therefore
 *  has to go out on the wire) from being spread across a component.
 */
class MixerComponent : public juce::Component,
                       public ThemedComponent,
                       private juce::Timer
{
public:
  /** `levels` is read, never written: the meters are the one part of this
   *  page that says something the device was told rather than something a
   *  finger did. */
  MixerComponent (MixerState &state, VuLevels const &levels);
  ~MixerComponent () override;

  void paint (juce::Graphics &g) override;
  void resized () override;
  /** The whole geometry is worked out from the skin's pot size and fonts, so
   *  a skin change has to re-lay this out, not merely repaint it. */
  void applyTheme () override;

  /** The meters' timer runs only while the page is on screen. A timer behind
   *  a closed overlay is a repaint of something nobody is looking at, on a
   *  device whose sphere wants the machine. */
  void visibilityChanged () override;

  /** A control was dragged, by whole increments. What one increment is worth
   *  is the caller's to say: it is the same step the encoders and the bar's
   *  channel grid take, and it lives with them rather than here. */
  std::function<void (int channel, MixerControl, int steps)> onChannelDragged;
  /** A key with two values was tapped — PFL and FX. tapTogglesValue's rule:
   *  a tap flips it, because stepping a boolean is direction-tied and a tap
   *  has no direction. */
  std::function<void (int channel, MixerControl)> onChannelTapped;
  std::function<void (MasterControl, int steps)> onMasterDragged;
  std::function<void (FilterControl, int steps)> onFilterDragged;
  std::function<void (FilterControl)> onFilterTapped;

  /** Which panel the overlay's side strips would sit around. Answered even
   *  though this overlay does not want them — see updateOverlayButtons(),
   *  which asks whether the open overlay has a list rather than asking which
   *  overlay it is. */
  juce::Rectangle<int> panelBounds () const;

private:
  /** A TouchControl's identity is two ints, and this overlay has three kinds
   *  of control. The primary is the column a control belongs to: a channel by
   *  its own number, then the summing section and the filter after them, so
   *  one number answers "whose is this" and the secondary is the position in
   *  that group's order. */
  static constexpr int masterGroup = numChannelsInitial;
  static constexpr int filterGroup = numChannelsInitial + 1;

  /** Redraws the meters and nothing else -- see vuMeterRefreshHz for what
   *  sets the rate. The knobs and keys around them change when a finger
   *  changes them, so asking for the whole page here would redraw a mixer
   *  twenty-five times a second to move nine bars. */
  void timerCallback () override;

  void paintStrip (juce::Graphics &g, int channel);
  /** The nine meters: each channel's input beside its level, and the output
   *  block in the master column. Drawn after the strips, from one reading of
   *  the clock, so the whole page shows one moment rather than nine. */
  void paintMeters (juce::Graphics &g);
  /** The fifth column, drawn as a strip like the four beside it. */
  void paintMasterColumn (juce::Graphics &g);
  /** The row across the foot, which belongs to neither. */
  void paintFilterRow (juce::Graphics &g);
  MixerState &_state;
  VuLevels const &_levels;
  MixerLayout _layout;
  ControlMetrics _metrics{};

  std::array<std::array<std::unique_ptr<TouchControl>, numMixerControls>,
             static_cast<std::size_t> (numChannelsInitial)>
      _channelTouch;
  std::array<std::unique_ptr<TouchControl>, numMasterControls> _masterTouch;
  std::array<std::unique_ptr<TouchControl>, numFilterControls> _filterTouch;
};

}
