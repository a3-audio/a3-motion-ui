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

#include <a3-motion-ui/theme/ThemedComponent.hh>

#include <a3-motion-engine/tempo/TempoClock.hh>

#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/StatusBarLayout.hh>
#include <a3-motion-ui/components/TickIndicator.hh>
#include <a3-motion-ui/components/VuMeter.hh>

namespace a3
{

class StatusBar : public juce::Component, public ThemedComponent, public juce::Value::Listener
{
public:
  StatusBar (juce::Value &valueBPM);
  ~StatusBar ();

  void resized () override;
  void paint (juce::Graphics &g) override;

  void valueChanged (juce::Value &value) override;
  void beatCallback (Measure measure);

  // Update from external OSC data
  void setExternalBPM (float bpm);

  /** Re-read everything this bar caches from the theme: its labels take
   *  their colour and their font once rather than per paint, so a skin or
   *  size change has to be pushed rather than repainted. */
  void applyTheme () override;

private:
  /** What the two clock readouts are drawn in — one answer for both, so
   *  they cannot end up different colours. EXT is a warning: the tempo is
   *  somebody else's. */
  juce::Colour clockReadoutColour () const;
  /** Composes the one reading — "EXT BPM 123.0" — and colours it. Every
   *  writer goes through here, so mode and tempo cannot end up saying
   *  different things or wearing different colours. */
  void refreshClockReadout ();

public:

  /** Just the font half of applyTheme(), for a size change. */
  void refreshFonts ();

  /** Tapped when the little keyboard icon at the far right is touched.
   *  The bar owns no keyboard — it only says the icon was hit. */
  std::function<void ()> onKeyboardIconTapped;

  /** How the keyboard icon reads: there is nothing to type into, there is
   *  and it is hidden, or it is up. A tap that does nothing has to look
   *  like one. */
  enum class KeyboardState
  {
    Unavailable,
    Available,
    Shown,
  };

  void setKeyboardState (KeyboardState state);

  /** Tapped when the MIX key left of the keyboard icon is touched. The bar
   *  owns no mixer — it only says the key was hit. */
  std::function<void ()> onMixIconTapped;

  /** Whether the mixer overlay is open, so the key can wear it. Every other
   *  key on this device that stands for a state wears it. */
  void setMixOpen (bool open);

  /** How far the running take has got, as a thin line under the tick
   *  indicator, in the recording channel's own colour. A negative fraction
   *  means no take is running and nothing is drawn.
   *
   *  Laid over the indicator, translucent, so the beats stay readable through
   *  it: the indicator is the widest thing on this bar and sits over the
   *  sphere, where the eye already is while recording. */
  void setRecordingProgress (float fraction, juce::Colour colour);

  /** The take is asked for but has not begun: it starts on the next downbeat.
   *  Told separately from the progress because until then there is no
   *  progress to tell -- and because the window has its own job, which is
   *  counting the hand in. See components/RecordingIndicator.hh. */
  void setCountingIn (bool countingIn, juce::Colour colour);

  /** One beat of the count-in. Driven from A3MotionUIComponent's beat
   *  handler rather than from this class's own beatCallback, which returns
   *  early in every clock mode but internal -- a take armed on an external
   *  clock would have counted in silence. */
  void pulseCountInOnBeat ();

  /** How far each channel's clip has got through its own loop, as a narrow
   *  mark in that channel's colour. A negative fraction means the channel is
   *  not playing and no mark is drawn.
   *
   *  One mark per channel rather than one fill for all of them: up to four
   *  clips run at once, and a single fill could only ever show one of them —
   *  it showed whichever clip the settings bar happened to be displaying,
   *  which is not the question anybody is asking mid-set.
   *
   *  Recording keeps its fill. Two states that must not be confused are told
   *  apart by their shape rather than by counting marks: a take writes over
   *  something that does not come back, and that has to be legible at a
   *  glance in a dark room. */
  void setChannelPlayheads (
      std::array<float, numChannelsInitial> const &positions,
      std::array<juce::Colour, numChannelsInitial> const &colours);

  /** The nine small meters beside the beat display: the four inputs left of
   *  it, the five outputs right of it.
   *
   *  **Pushed in, not pulled.** They come from A3MotionUIComponent's timer,
   *  read off the one VuLevels the mixer's own meters read — the same route
   *  the channel playheads take. This bar has no repeating timer of its own
   *  and must not grow one: it is the component that is always on screen, and
   *  a clock here would be a clock that never stops.
   *
   *  Why they exist at all, given the mixer already draws these: the mixer
   *  has to be opened. These say that something is arriving while the screen
   *  is on CLIP, PADS or FILES, which is where it is for most of a set.
   *
   *  The repaint is clipped to the two blocks, and only when a level has
   *  actually moved — see StatusBarLayout::inputBlock. */
  void setVuLevels (std::array<VuLevel, numChannelsInitial> const &inputs,
                    std::array<VuLevel, numOutputMeters> const &outputs);

  /** The take's progress is laid over the tick indicator, which is a child —
   *  so it has to be drawn after the children rather than in paint(). */
  void paintOverChildren (juce::Graphics &g) override;

  void mouseUp (juce::MouseEvent const &event) override;

  /** The header size this bar can actually show — the theme's, unless the
   *  height it was given is the smaller of the two. */
  float headerFontSize () const;

  /** How tall the bar wants to be for the current header size. Never below
   *  getMinimumHeight(), so a small header setting does not shrink the bar
   *  below the layout it was drawn for. */
  int preferredHeight () const;
  void setBeatClock (int beat, int bar);
  
  // Clock mode status: 0 = INT, 1 = EXT, 2 = PIO
  void setClockMode (int mode);

  /** What was last turned, and to what -- "reach 0.42", "fade 0.25".
   *
   *  It used to sit in the band above the global strip, which is the band the
   *  transport keys now stand in. Here it is beside the readings it belongs
   *  with: the bar is where the device says what it is doing. */
  void setControlReadout (juce::String const &text);

  static constexpr int
  getMinimumHeight ()
  {
    return minimumRowHeight;
  }

private:
  /** The four marks, drawn after the recording fill so neither hides the
   *  other. Not part of paintOverChildren's body only because that function
   *  already carries the fill's reasoning and two ideas in one function is
   *  how the next reader loses both. */
  void paintPlayheads (juce::Graphics &g, juce::Rectangle<float> tick);

  /** The nine bars. Its own function for the reason paintMixKey() is: paint()
   *  already carries the keyboard icon's reasoning. */
  void paintVuMeters (juce::Graphics &g);

  /** The MIX key. Its own function rather than four lines in paint(), which
   *  already carries the keyboard icon's reasoning — two keys' worth of
   *  drawing in one body is how the next reader loses both. */
  void paintMixKey (juce::Graphics &g);

  /** Every rectangle on this bar, from the one pure calculation the test
   *  checks — so paint() draws into what resized() placed. */
  StatusBarLayout _layout;

  std::array<VuLevel, numChannelsInitial> _inputLevels;
  std::array<VuLevel, numOutputMeters> _outputLevels;

  juce::Rectangle<int> _keyboardIconArea;
  KeyboardState _keyboardState = KeyboardState::Unavailable;
  juce::Rectangle<int> _mixIconArea;
  bool _mixOpen = false;
  TickIndicator _tickIndicator;
  float _recordingProgress = -1.f;
  juce::Colour _recordingColour;

  bool _countingIn = false;
  bool _countInLit = false;
  std::array<float, numChannelsInitial> _playheads{ -1.f, -1.f, -1.f, -1.f };
  std::array<juce::Colour, numChannelsInitial> _playheadColours;

  juce::Label _labelBPM;
  juce::Label _labelReadout;
  juce::Value &_valueBPM;
  
  std::atomic<float> _externalBPM{ 0.f };
  std::atomic<int> _beatClockBeat{ 0 };
  std::atomic<int> _beatClockBar{ 0 };
  std::atomic<int> _clockMode{ 0 };
};

}
