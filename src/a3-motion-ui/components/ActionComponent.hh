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
#include <a3-motion-ui/components/ScriptEditor.hh>
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

private:
  /** Puts the skin on the editor: its colour ids, the tokeniser's scheme and
   *  the script font. Called whenever the skin changes. */
  void dressEditor ();

public:

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

  /** Which action clip this slot fires; empty for none. */
  void setActionName (juce::String const &name);

  /** The script that action carries, shown under its name. Empty for none.
   *  Loading, not editing: whatever was being typed is replaced. */
  void setScript (juce::String const &script);
  /** What is in the editor now, for whoever writes the file. */
  juce::String script () const { return _document.getAllContent (); }
  bool scriptIsEdited () const
  {
    return _document.hasChangedSinceSavePoint ();
  }
  void markScriptSaved () { _document.setSavePoint (); repaint (); }

  /** What the script got wrong when it last ran, shown along the bottom of
   *  the editor. Kept apart from the text: an error is about the script, not
   *  part of it, and a message that had to be deleted before typing could go
   *  on would be a message in the way. */
  void setScriptErrors (juce::StringArray const &errors);

  /** What the action field's list offers. The empty string is "no action",
   *  the way entry 0 of the library is "no clip". */
  void setActionChoices (juce::StringArray const &names);

  /** Whether the page is taking keys. Told rather than worked out, because
   *  what shows the keyboard is the page above this one. */
  bool isEditingScript () const { return _editing; }

  /** Whether the action on this slot is one the device ships with. Those are
   *  read-only: writing over one takes it from every clip that uses it, and
   *  there is no getting it back -- Save as is the way to keep an edit. */
  void setScriptIsShipped (bool shipped);
  void stopEditingScript ();

  /** Where the global strip's three channel rows stand, in the bar's own
   *  coordinates. The page puts its own rows on those so the two blocks of
   *  knobs read across at one height; an empty rectangle means lay out
   *  freely. */
  void setGridReference (juce::Rectangle<int> barCoordinates);

  /** A knob was turned: where it stands now. The nine envelope knobs are
   *  sliders, so the value is theirs and the page only passes it on -- the
   *  increments this used to count were the slider's job. */
  std::function<void (int control, double value)> onControlSet;
  std::function<void (int control, int increment)> onControlDragged;
  std::function<void (int control)> onControlDoubleTapped;
  std::function<void (int control)> onControlTapped;

  /** A name was picked out of the action list; empty means "fire nothing". */
  std::function<void (juce::String const &name)> onActionChosen;
  /** The editor was opened or closed — the page above shows and hides the
   *  system keyboard on it. */
  std::function<void (bool editing)> onScriptEditingChanged;
  /** Keep what is in the editor. Only then is anything written -- typing
   *  used to save on every keystroke, which left no way to try a line and
   *  take it back. */
  /** The fat key under the knobs: fires this slot's action for as long as it
   *  is held, the way the ACT pad does. Held rather than tapped, because that
   *  is what the pad it stands for does. */
  std::function<void (bool held)> onFireHeld;

  std::function<void ()> onScriptSaved;
  /** Save as: the text goes to a file of the performer's own. The one way to
   *  keep an edit to a shipped action. */
  std::function<void ()> onScriptSavedAs;
  /** Throw the edit away and put the file's text back. */
  std::function<void ()> onScriptCancelled;

private:
  void paintActionField (juce::Graphics &g);
  void paintScriptField (juce::Graphics &g);
  void paintScriptErrors (juce::Graphics &g);
  void paintScriptKeys (juce::Graphics &g);
  void paintActionList (juce::Graphics &g);

  void focusLost (FocusChangeType cause) override;

  /** One font for the script, and the three measurements everything else
   *  reads off it. Drawing and hit-testing take the same numbers from the
   *  same place, or a tap lands on a different line than the one under it. */
  juce::Font scriptFont () const;
  juce::Rectangle<int> scriptTextArea () const;
  int scriptLineHeight () const;
  float scriptCharacterWidth () const;

  /** How many lines the script area can show at the current size. */
  int visibleScriptLines () const;
  /** Where a tap in the script area lands, as a line and a column. */

  void openActionList ();
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
  /** The script itself, and the editor over it -- JUCE's, see ScriptEditor.
   *  The C++ tokeniser rather than one of our own: SuperCollider's comments,
   *  strings, numbers and brackets are close enough to read by, and a
   *  tokeniser for the rest is a job of its own. */
  juce::CodeDocument _document;
  juce::CPlusPlusCodeTokeniser _tokeniser;
  std::unique_ptr<ScriptEditor> _editor;
  juce::StringArray _choices;
  juce::StringArray _scriptErrors;
  bool _listOpen = false;
  /** The list's window, kept apart from which script is chosen -- see
   *  ListScroll. Twenty scripts do not fit in a field a few fingertips tall,
   *  and a list drawn from row zero with no window is one whose last entries
   *  cannot be reached at all. */
  int _listTop = 0;
  bool _editing = false;
  bool _shipped = false;
  juce::Rectangle<int> _gridReference;

  std::array<std::unique_ptr<TouchControl>, numControls> _touch;
  /** One per envelope control; the mode beside the name stays a key. */
  std::array<std::unique_ptr<PotKnob>, numControls> _knob;
  /** The name field, which opens the list, and the script, which takes the
   *  caret. Neither is a knob, so neither is in `controls`. */
  std::unique_ptr<TouchControl> _actionTouch;
  std::unique_ptr<TouchControl> _scriptTouch;
  std::unique_ptr<TouchControl> _saveAsTouch;
  std::unique_ptr<TouchControl> _fireTouch;
  bool _firing = false;
  std::unique_ptr<TouchControl> _saveTouch;
  std::unique_ptr<TouchControl> _cancelTouch;
};

}
