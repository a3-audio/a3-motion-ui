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

#include <a3-motion-ui/components/LayoutHints.hh>
#include <a3-motion-ui/components/TickIndicator.hh>

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

  /** How far the running take has got, as a thin line under the tick
   *  indicator, in the recording channel's own colour. A negative fraction
   *  means no take is running and nothing is drawn.
   *
   *  Under the indicator rather than in it: the beat display keeps its own
   *  job, and this sits where the eye already is while recording — over the
   *  sphere rather than at the bottom edge. */
  void setRecordingProgress (float fraction, juce::Colour colour);

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

  static constexpr int
  getMinimumHeight ()
  {
    return LayoutHints::lineHeight;
  }

private:
  juce::Rectangle<int> _keyboardIconArea;
  KeyboardState _keyboardState = KeyboardState::Unavailable;
  TickIndicator _tickIndicator;
  float _recordingProgress = -1.f;
  juce::Colour _recordingColour;

  juce::Label _labelBPM;
  juce::Value &_valueBPM;
  
  juce::Label _labelClockMode;
  std::atomic<float> _externalBPM{ 0.f };
  std::atomic<int> _beatClockBeat{ 0 };
  std::atomic<int> _beatClockBar{ 0 };
  std::atomic<int> _clockMode{ 0 };
};

}
