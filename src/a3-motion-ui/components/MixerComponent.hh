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
#include <a3-motion-ui/components/PotKnob.hh>
#include <a3-motion-ui/components/VuFader.hh>
#include <a3-motion-ui/components/VuMeterView.hh>
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

/** The metrics both mixer pages lay themselves out with.
 *
 *  The knob's diameter is the skin's pot size and the layout's floor for a row
 *  is that diameter, so a skin change moves every rectangle on either page and
 *  not only its colours. One size for both the caption and the value: the bar
 *  fits those to a section it shares with two others, where a mixer control
 *  stands in a cell of its own with a four-character word in it, so the body
 *  size the skin asks for is what it gets.
 *
 *  Shared because a page laid out on other metrics than its sibling is the
 *  same control at two sizes on one screen. */
ControlMetrics mixerControlMetrics ();

/** The gestures one channel control answers, wired onto its hit area.
 *
 *  The accumulator is TouchControl's own, fed from the skin's
 *  touchDragPixelsPerStep, so how far a finger travels for one step stays one
 *  screw in the skin editor rather than becoming a second one per page.
 *
 *  Only the two-valued controls answer a tap. A continuous value is dragged
 *  and never tapped -- a tap has no direction, so there is nothing for it to
 *  say about a level. That rule is the reason this is shared: it was written
 *  out on both pages, and a special case changed in one of them would have
 *  left the *same channel* behaving differently in the overlay and in the MIX
 *  tab.
 *
 *  Two taps are a different question and are answered by a different rule:
 *  they mean "put that back", which needs no direction either, and they are
 *  wired for exactly the controls that have somewhere to go back to. Today
 *  that is SEND alone -- see mixerControlRestPosition for why GAIN and VOL
 *  deliberately do not.
 *
 *  The control is bound here rather than read out of the hit area's identity,
 *  because the two pages number their identities differently -- the overlay
 *  needs a channel in it, the strip's channel is the component's and is read
 *  at the moment of the gesture. Which channel a gesture belongs to is
 *  therefore the caller's to close over. */
void wireMixerChannelTouch (
    TouchControl &touch, MixerControl control,
    std::function<void (MixerControl, int steps)> dragged,
    std::function<void (MixerControl)> tapped,
    std::function<void (MixerControl)> doubleTapped = {});

/** The meter timer, which runs only while its page is on screen.
 *
 *  A timer behind a closed overlay, or behind a tab that is not the one on
 *  show, is a repaint of something nobody is looking at on a device whose
 *  sphere wants the machine. */
void runMeterTimerWhileVisible (bool isVisible, juce::Timer &timer);


/** The one line a page draws where it cannot lay itself out.
 *
 *  A sentence rather than targets nobody can hit: a mixer that cannot be
 *  operated is worse than a sentence saying the room is too small, because
 *  the sentence can be acted on. `what` names the page -- "the mixer", "the
 *  strip" -- and is the only part of this the two do not share. */
void paintMixerHasNoRoom (juce::Graphics &g, juce::Rectangle<int> bounds,
                          juce::String const &what);

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

  /** Redraw one meter and nothing else.
   *
   *  A drag on a fader used to repaint the whole page for every pixel the
   *  finger moved -- 660x491 at about 2.8 ms a time, measured on the rig,
   *  which is what made the master's long fader feel like it was catching.
   *  The handle never leaves its meter, so its meter is all that changes. */
  /** Put the faders and knobs where the state says, without repainting the
   *  page: a slider redraws itself when its value changes. */
  void syncControls ();

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

  /** Two taps: put this channel's control back where mixerControlRestPosition
   *  says. Fires only for a control that has one. */
  std::function<void (int channel, MixerControl)> onChannelDoubleTapped;
  /** A knob was turned: where it stands now. The pages own no values; the
   *  state does, and a slider tells it where the finger left it. */
  std::function<void (int channel, MixerControl, float value)>
      onChannelValueChanged;
  std::function<void (MasterControl, float value)> onMasterValueChanged;
  std::function<void (FilterControl, float value)> onFilterValueChanged;

  /** A drag on the output meters: the master volume, where the finger has
   *  taken it. No double tap there -- see MixerComponent's constructor. */
  std::function<void (float value)> onMasterMeterDraggedTo;
  /** Two taps on a channel's meter: put that channel at full volume. */
  std::function<void (int channel)> onMeterDoubleTapped;
  /** A drag on a channel's meter: VOL, where the finger has taken it. */
  std::function<void (int channel, float value)> onMeterDraggedTo;
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

  std::array<std::array<std::unique_ptr<TouchControl>, numMixerFaceControls>,
             static_cast<std::size_t> (numChannelsInitial)>
      _channelTouch;
  /** One per channel and one per output: the meters, each painting itself.
   *  Added before the faders, so a fader stands over its meter. */
  std::array<std::unique_ptr<VuMeterView>,
             static_cast<std::size_t> (numChannelsInitial)>
      _channelMeterView;
  std::array<std::unique_ptr<VuMeterView>, numOutputMeters> _outputMeterView;

  /** One per channel, over its meter: the fader that VOL is dragged on. */
  std::array<std::unique_ptr<VuFader>,
             static_cast<std::size_t> (numChannelsInitial)>
      _channelFader;
  /** The knobs: one per control that is turned. The two keys of a channel
   *  and the filter's mode key keep their hit areas -- they are pressed. */
  std::array<std::array<std::unique_ptr<PotKnob>, numMixerFaceControls>,
             static_cast<std::size_t> (numChannelsInitial)>
      _channelKnob;
  std::array<std::unique_ptr<PotKnob>, numMasterFaceControls> _masterKnob;
  std::array<std::unique_ptr<PotKnob>, numFilterControls> _filterKnob;

  std::array<std::unique_ptr<TouchControl>, numMasterFaceControls> _masterTouch;
  /** Over the output meters: the master volume's fader. */
  std::unique_ptr<VuFader> _masterFader;
  std::array<std::unique_ptr<TouchControl>, numFilterControls> _filterTouch;
};

}
