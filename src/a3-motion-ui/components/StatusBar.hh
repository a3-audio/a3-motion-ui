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

#include <a3-motion-ui/components/AppView.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/StatusBarLayout.hh>
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

  /** Tapped when the keyboard key, left of MENU, is touched. The bar owns
   *  no keyboard — it only says the key was hit. */
  std::function<void ()> onKeyboardIconTapped;

  /** The clock key, left of the tempo: step to the next clock mode. */
  std::function<void ()> onClockKeyTapped;
  /** The FULL/FPV key, right of CLOCK. */
  std::function<void ()> onViewKeyTapped;
  /** STEMDECK, at the right end: over to StemDeck's workspace. */
  std::function<void ()> onDeckKeyTapped;
  /** The arrow beside it: list the rig's workspaces. */
  std::function<void ()> onWorkspacesKeyTapped;
  /** STEMDECK and the arrow, in this bar's coordinates. */
  juce::Rectangle<int> workspacesAnchor () const;

  /** MENU, at the right end: open or close the menu. */
  std::function<void ()> onMenuKeyTapped;
  /** The beat display was touched: a tap for the tempo. On the finger's
   *  way down, not up -- a tap is a moment, and the release comes a
   *  variable time after it. The TAP key in the bar's global strip was
   *  this until 2026-09-26. */
  std::function<void ()> onTickTapped;

  /** Whether the menu is open, so MENU can wear it. */
  void setMenuOpen (bool open);

  /** How the KEYS key reads: there is nothing to type into, there is
   *  and it is hidden, or it is up. A tap that does nothing has to look
   *  like one. */
  enum class KeyboardState
  {
    Unavailable,
    Available,
    Shown,
  };

  void setKeyboardState (KeyboardState state);

  /** Tapped when the CLEAN key left of KEYS is touched. The bar
   *  owns no skins
   *  -- it only says the key was hit. See theme/CleanSkin.hh. */
  std::function<void ()> onCleanIconTapped;

  /** Whether there is a clean skin to go to, and whether it is up. A key
   *  with nothing to switch to is greyed out, like KEYS with
   *  nothing to type into. */
  void setCleanState (bool available, bool active);

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



  /** The take's progress is laid over the tick indicator, which is a child —
   *  so it has to be drawn after the children rather than in paint(). */
  void paintOverChildren (juce::Graphics &g) override;

  void mouseDown (juce::MouseEvent const &event) override;
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
  /** Names the view that is shown; FPV lights the key. */
  void setView (AppView view);

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

  /** The ground every key on this bar stands on -- CLOCK, CLEAN, the
   *  keyboard and MENU look alike (asked for on 2026-09-26): a framed key,
   *  washed in the accent while what it stands for is on. */
  void paintKeyGround (juce::Graphics &g, juce::Rectangle<int> area,
                       bool on) const;
  /** The colour of what is written or drawn on a key. */
  juce::Colour keyInk (bool available, bool on) const;
  void paintWordKey (juce::Graphics &g, juce::Rectangle<int> area,
                     juce::String const &word, bool available, bool on);
  void paintKeyboardKey (juce::Graphics &g);

  /** Every rectangle on this bar, from the one pure calculation the test
   *  checks — so paint() draws into what resized() placed. */
  StatusBarLayout _layout;


  KeyboardState _keyboardState = KeyboardState::Unavailable;
  bool _menuOpen = false;
  bool _cleanAvailable = false;
  bool _cleanActive = false;
  TickIndicator _tickIndicator;
  float _recordingProgress = -1.f;
  juce::Colour _recordingColour;

  bool _countingIn = false;
  bool _countInLit = false;

  juce::Label _labelBPM;
  juce::Label _labelReadout;
  juce::Value &_valueBPM;
  
  std::atomic<float> _externalBPM{ 0.f };
  std::atomic<int> _beatClockBeat{ 0 };
  std::atomic<int> _beatClockBar{ 0 };
  std::atomic<int> _clockMode{ 0 };
  bool _fpv{ false }; // message thread only
};

}
