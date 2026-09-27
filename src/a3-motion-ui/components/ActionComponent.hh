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

#include <a3-motion-ui/components/ActionLayout.hh>
#include <a3-motion-ui/components/PotKnob.hh>
#include <a3-motion-ui/components/TouchControl.hh>
#include <a3-motion-ui/theme/ThemedComponent.hh>

namespace a3
{

/** What the ACT key does, on a page of its own.
 *
 *  Which action clip the slot fires, and the envelope that fires it. The
 *  action itself is chosen in the file menu beside the clips -- one place
 *  where things are picked, rather than a second one here.
 *
 *  It decides nothing. A turn goes out as (control, increment) and lands in
 *  the same handler the clip bar's own controls reach.
 */
class ActionComponent : public juce::Component, public ThemedComponent
{
public:
  /** In the order they are laid out, which is the order the handler expects. */
  enum Control
  {
    Attack = 0,
    Decay,
    EnvelopeMax,
    FreqAttack,
    FreqDecay,
    FreqMax,
    QAttack,
    QDecay,
    QMax,
    /** Not a knob and not in a row -- it stands beside the action's name. */
    ActMode,
    numControls
  };

  ActionComponent ();
  ~ActionComponent () override;

  void paint (juce::Graphics &g) override;
  void resized () override;
  /** The page's geometry comes from the theme's header size, so a skin change
   *  has to re-lay it out rather than only repaint it. */
  void applyTheme () override;


  /** Which clip this page is showing, and the colour it wears. */
  void setTarget (int channel, int slot, juce::Colour channelColour);

  /** Puts a row's three values on its three knobs, without fighting a finger
   *  that is on one of them. */
  void putOnKnobs (int first, int attackStep, int decayStep, float max);
  /** The channel's colour, on all nine. */
  void putColourOnKnobs ();

public:
  /** Steps 0..envelopeMaxStep, as the engine counts them. */
  void setEnvelope (int attackStep, int decayStep, float max);
  /** The two filter envelopes: the cutoff's, then the resonance's. */
  void setFreqEnvelope (int attackStep, int decayStep, float max);
  void setQEnvelope (int attackStep, int decayStep, float max);
  /** 0 = one-shot, 1 = hold. */
  void setActMode (int mode);

  /** The chosen button's action, which the list marks; empty for none. */
  void setActionName (juce::String const &name);

  /** The six buttons' action names (empty = nothing assigned) and which one
   *  is chosen -- the one the list, the keys and the card act on. */
  void setActionButtons (std::array<juce::String, 6> const &names,
                         int chosen);
  /** The button whose action runs now, -1 for none: that field goes white,
   *  as its pad does. */
  void setRunningButton (int button);


  /** What the action field's list offers. The empty string is "no action",
   *  the way entry 0 of the library is "no clip". */
  void setActionChoices (juce::StringArray const &names);



  /** A knob was turned: where it stands now. The nine envelope knobs are
   *  sliders, so the value is theirs and the page only passes it on -- the
   *  increments this used to count were the slider's job. */
  std::function<void (int control, double value)> onControlSet;
  std::function<void (int control, int increment)> onControlDragged;
  std::function<void (int control)> onControlDoubleTapped;
  std::function<void (int control)> onControlTapped;

  /** A button's field was tapped: it becomes the chosen one. */
  std::function<void (int button)> onButtonChosen;

  /** A name was picked out of the action list; empty means "fire nothing". */
  std::function<void (juce::String const &name)> onActionChosen;
  /** The fat key under the knobs: fires this slot's action for as long as it
   *  is held, the way the ACT pad does. Held rather than tapped, because that
   *  is what the pad it stands for does. */
  std::function<void (bool held)> onFireHeld;
  /** EDIT: open this action's script in FILES, beside the list there. */
  std::function<void ()> onEditPressed;


private:
  void paintActionFields (juce::Graphics &g);
  void paintFireKey (juce::Graphics &g);
  void paintActionList (juce::Graphics &g);


  void paintEditKey (juce::Graphics &g);
  void chooseFromActionList (juce::Point<int> point);

  ActionLayout _layout;

  int _channel = 0;
  int _slot = 0;
  juce::Colour _channelColour;

  int _attack = 2;
  int _decay = 3;
  float _max = 1.f;
  int _freqAttack = 2;
  int _freqDecay = 3;
  float _freqMax = 0.f;
  int _qAttack = 2;
  int _qDecay = 3;
  float _qMax = 0.f;
  int _actMode = 0;
  juce::String _actionName;
  std::array<juce::String, 6> _buttonNames;
  int _chosenButton = 0;
  int _runningButton = -1;
  std::array<std::unique_ptr<TouchControl>, 6> _fieldTouch;
  juce::StringArray _choices;
  /** The list's window, kept apart from which script is chosen -- see
   *  ListScroll. Twenty scripts do not fit in a field a few fingertips tall,
   *  and a list drawn from row zero with no window is one whose last entries
   *  cannot be reached at all. */
  int _listTop = 0;
  std::array<std::unique_ptr<TouchControl>, numControls> _touch;
  /** One per envelope control; the mode beside the name stays a key. */
  std::array<std::unique_ptr<PotKnob>, numControls> _knob;
  /** The list to assign from, and EDIT. Neither is a knob, so neither is in
   *  `controls`. */
  std::unique_ptr<TouchControl> _listTouch;
  std::unique_ptr<TouchControl> _editTouch;
  std::unique_ptr<TouchControl> _fireTouch;
  bool _firing = false;
};

}
