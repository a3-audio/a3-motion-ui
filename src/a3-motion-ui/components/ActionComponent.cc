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

#include "ActionComponent.hh"

#include <a3-motion-engine/Envelope.hh>

#include <a3-motion-ui/components/BarKnob.hh>
#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/components/FittedFont.hh>
#include <a3-motion-ui/components/ListScroll.hh>
#include <a3-motion-ui/theme/TransportLook.hh>
#include <a3-motion-ui/components/ActionKnobs.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
/** Where a step sits on its scale, as the knob speaks it. */
float
envFrac (int step)
{
  return static_cast<float> (step) / static_cast<float> (envelopeMaxStep) * 2.f
         - 1.f;
}
}

ActionComponent::ActionComponent ()
{
  // The nine envelope controls are knobs of their own -- sliders, drawn by
  // the LookAndFeel as this device's knob (PotKnob). The mode beside the
  // action's name is a key and keeps its hit area.
  for (int i = 0; i < ActMode; ++i)
    {
      auto const spec = actionKnobSpec (i);

      auto knob = std::make_unique<PotKnob> ();
      knob->setLabel (actionKnobIsACeiling (i) ? caption::envelopeMax
                      : i % 3 == 0             ? caption::attack
                                               : caption::decay);
      knob->setRange (0.0, spec.max, spec.interval);
      knob->setDoubleClickReturnValue (true, spec.resetTo);

      knob->onValueChange = [this, i, k = knob.get ()] {
        if (onControlSet)
          onControlSet (i, k->getValue ());
      };

      addAndMakeVisible (*knob);
      _knob[static_cast<size_t> (i)] = std::move (knob);
    }

  for (int i = 0; i < numControls; ++i)
    {
      if (i < ActMode)
        continue;

      auto touch = std::make_unique<TouchControl> ();
      touch->setIdentity (i);

      touch->onDragIncrement = [this] (int control, int, int increment) {
        if (onControlDragged)
          onControlDragged (control, increment);
      };
      touch->onDoubleTap = [this] (int control, int) {
        if (onControlDoubleTapped)
          onControlDoubleTapped (control);
      };
      touch->onTap = [this] (int control, int) {
        if (onControlTapped)
          onControlTapped (control);
      };

      addAndMakeVisible (*touch);
      _touch[static_cast<size_t> (i)] = std::move (touch);
    }

  // The keys have to land here rather than in the void: Onboard types into
  // whatever has the focus, and a page that never asked for it gets nothing.
  setWantsKeyboardFocus (true);

  _actionTouch = std::make_unique<TouchControl> ();
  _actionTouch->onTap = [this] (int, int) {
    // Reaching for the list is leaving the editor, so the keyboard goes with
    // it -- otherwise it stays up over the list it is covering.
    stopEditingScript ();

    // Tapping the name is how the list opens and how it closes again -- a
    // control that only ever goes one way leaves you tapping elsewhere to
    // undo what it did.
    if (_listOpen)
      closeActionList ();
    else
      openActionList ();

    repaint ();
  };
  addAndMakeVisible (*_actionTouch);

  // The script is JUCE's editor, read-only until it is touched -- see
  // ScriptEditor for the three things a finger needs on top of it.
  _editor = std::make_unique<ScriptEditor> (_document, &_tokeniser);
  _editor->onStartEditing = [this] {
    if (_actionName.isEmpty ())
      {
        openActionList ();
        repaint ();
        return;
      }

    if (!_editing)
      {
        _editing = true;
        _editor->setReadOnly (false);
        _editor->grabKeyboardFocus ();
        if (onScriptEditingChanged)
          onScriptEditingChanged (true);
      }

    repaint ();
  };
  _editor->onEscape = [this] { stopEditingScript (); };
  addAndMakeVisible (*_editor);
  dressEditor ();

  _scriptTouch = std::make_unique<TouchControl> ();
  _scriptTouch->onTapAt = [this] (int, int, juce::Point<int> at) {
    if (_listOpen)
      {
        chooseFromActionList (at);
        return;
      }

    // With no action on the slot there is nothing to type into and nowhere to
    // put it, so the tap opens the list instead -- which is what you would be
    // reaching for next anyway.
    if (_actionName.isEmpty ())
      {
        openActionList ();
        repaint ();
        return;
      }

    // The editor stands over this area and answers a touch itself; what is
    // left here is the list, which lies over the editor when it is open.
    repaint ();
  };
  _scriptTouch->onDragIncrement = [this] (int, int, int increment) {
    // Only ever the open list: this area is shown while the list lies over the
    // editor and hidden otherwise, and the editor scrolls itself. The list is
    // a list like any other and follows the same rule -- the page goes the
    // finger's way, which it used to return out of, leaving every script past
    // the sixth unreachable.
    _listTop = a3::scrollBy (_listTop, increment,
                             actionListVisibleRows (_layout),
                             _choices.size ());
    repaint ();
  };
  // Added, not shown: the editor stands in this area and answers touches
  // itself. This one comes to the front only while the action list lies over
  // it -- in front of the editor, because it is added after it.
  addChildComponent (*_scriptTouch);

  // Held, not tapped: it stands for the ACT pad, and that pad is held.
  _fireTouch = std::make_unique<TouchControl> ();
  _fireTouch->onPress = [this] (int, int) {
    _firing = true;
    if (onFireHeld)
      onFireHeld (true);
    repaint ();
  };
  _fireTouch->onRelease = [this] (int, int) {
    _firing = false;
    if (onFireHeld)
      onFireHeld (false);
    repaint ();
  };
  addAndMakeVisible (*_fireTouch);

  _saveAsTouch = std::make_unique<TouchControl> ();
  _saveAsTouch->onTap = [this] (int, int) {
    if (!_document.hasChangedSinceSavePoint ())
      return;

    // The text goes to a file of the performer's own; the page is told what
    // it is called when the slot comes back with it. Saved here as well as in
    // the slot, because setScript() returns early on text it already holds --
    // so the save point would never be reached from outside and the edge would
    // stay marked on a script that is safely on disk.
    _document.setSavePoint ();
    stopEditingScript ();
    if (onScriptSavedAs)
      onScriptSavedAs ();

    repaint ();
  };
  addAndMakeVisible (*_saveAsTouch);

  _saveTouch = std::make_unique<TouchControl> ();
  _saveTouch->onTap = [this] (int, int) {
    if (!_document.hasChangedSinceSavePoint () || _shipped)
      return;

    _document.setSavePoint ();
    stopEditingScript ();
    if (onScriptSaved)
      onScriptSaved ();

    repaint ();
  };
  addAndMakeVisible (*_saveTouch);

  _cancelTouch = std::make_unique<TouchControl> ();
  _cancelTouch->onTap = [this] (int, int) {
    stopEditingScript ();
    if (onScriptCancelled)
      onScriptCancelled ();

    repaint ();
  };
  addAndMakeVisible (*_cancelTouch);
}

ActionComponent::~ActionComponent () = default;

void
ActionComponent::applyTheme ()
{
  dressEditor ();
  resized ();
  repaint ();
}

void
ActionComponent::dressEditor ()
{
  if (!_editor)
    return;

  // The skin's colours, through the editor's own ids -- the same way a slider
  // gets its channel colour. The field behind it is already drawn darker than
  // the page, so the editor itself stays transparent to it.
  _editor->setColour (juce::CodeEditorComponent::backgroundColourId,
                      juce::Colours::transparentBlack);
  _editor->setColour (juce::CodeEditorComponent::defaultTextColourId,
                      toColour (theme ().textPrimary,
                                theme ().alphaTextStrong));
  _editor->setColour (juce::CodeEditorComponent::lineNumberBackgroundId,
                      juce::Colours::transparentBlack);
  _editor->setColour (juce::CodeEditorComponent::lineNumberTextId,
                      toColour (theme ().textMuted, theme ().alphaMuted));
  _editor->setColour (juce::CodeEditorComponent::highlightColourId,
                      _channelColour.withAlpha (theme ().alphaFillEmphasis));

  // What the tokeniser names, in the skin's words: a comment is what tells a
  // written-out script from one somebody explained, which is why it was the
  // one thing the hand-drawn editor coloured at all.
  juce::CodeEditorComponent::ColourScheme scheme;
  scheme.set ("Comment", toColour (theme ().textMuted, theme ().alphaInactive));
  scheme.set ("String", toColour (theme ().accent));
  scheme.set ("Integer", toColour (theme ().accent));
  scheme.set ("Float", toColour (theme ().accent));
  scheme.set ("Keyword", toColour (theme ().highlight));
  scheme.set ("Operator", toColour (theme ().textPrimary, theme ().alphaSecondary));
  scheme.set ("Bracket", toColour (theme ().textPrimary, theme ().alphaSecondary));
  scheme.set ("Punctuation", toColour (theme ().textPrimary, theme ().alphaSecondary));
  scheme.set ("Identifier", toColour (theme ().textPrimary, theme ().alphaTextStrong));
  scheme.set ("Error", toColour (theme ().danger));
  _editor->setColourScheme (scheme);

  _editor->setFont (scriptFont ());

  // The bars that come with it: the skin's grey, and as wide as a line is
  // tall rather than JUCE's sixteen pixels -- everything here is measured in
  // what it stands next to.
  _editor->setColour (juce::ScrollBar::backgroundColourId,
                      juce::Colours::transparentBlack);
  _editor->setColour (juce::ScrollBar::thumbColourId,
                      toColour (theme ().textMuted, theme ().alphaGuide));
  _editor->setColour (juce::ScrollBar::trackColourId,
                      juce::Colours::transparentBlack);
  _editor->setScrollbarThickness (
      juce::jmax (1, juce::roundToInt (scriptFont ().getHeight () / 2.f)));
}

void
ActionComponent::resized ()
{
  _layout = layOutActionPage (
      getLocalBounds (), theme ().fontSize (FontRole::Header),
      theme ().fontSize (FontRole::Body), theme ().potSize,
      // The strip's rows arrive in the bar's coordinates; this page is a
      // child of the bar placed at clipContent, so they have to come back to
      // its own origin before they mean anything here.
      _gridReference.isEmpty ()
          ? _gridReference
          : _gridReference - getBounds ().getPosition ());

  // Every knob comes out of the rows; the mode does not stand in one.
  for (int i = 0; i < ActMode; ++i)
    _knob[static_cast<size_t> (i)]->setBounds (
        _layout.controls[static_cast<size_t> (i)]);

  _touch[ActMode]->setBounds (_layout.actModeField);

  if (_actionTouch)
    _actionTouch->setBounds (_layout.actionField);
  if (_scriptTouch)
    _scriptTouch->setBounds (_layout.scriptTextField);
  if (_fireTouch)
    _fireTouch->setBounds (_layout.fireButton);
  if (_saveTouch)
    _saveTouch->setBounds (_layout.saveButton);
  if (_saveAsTouch)
    _saveAsTouch->setBounds (_layout.saveAsButton);
  if (_cancelTouch)
    _cancelTouch->setBounds (_layout.cancelButton);

  if (_editor)
    _editor->setBounds (scriptTextArea ());

  updateScriptLayers ();
}

void
ActionComponent::putColourOnKnobs ()
{
  for (auto &knob : _knob)
    if (knob)
      knob->setKnobColour (_channelColour);
}

void
ActionComponent::setTarget (int channel, int slot, juce::Colour channelColour)
{
  _channel = channel;
  _slot = slot;
  _channelColour = channelColour;
  putColourOnKnobs ();
  repaint ();
}

void
ActionComponent::putOnKnobs (int first, int attackStep, int decayStep,
                             float max)
{
  // Not while a finger is on one: writing a value back into the knob that is
  // being turned is the page arguing with the hand.
  auto const put = [this] (int control, double value) {
    auto &knob = _knob[static_cast<size_t> (control)];
    if (knob && !knob->isMouseButtonDown ())
      knob->setValue (value, juce::dontSendNotification);
  };

  put (first, attackStep);
  put (first + 1, decayStep);
  put (first + 2, max);
}

void
ActionComponent::setEnvelope (int attackStep, int decayStep, float max)
{
  if (attackStep == _attack && decayStep == _decay && max == _max)
    return;

  _attack = attackStep;
  _decay = decayStep;
  _max = max;
  putOnKnobs (Attack, attackStep, decayStep, max);
}

void
ActionComponent::setFreqEnvelope (int attackStep, int decayStep, float max)
{
  if (attackStep == _freqAttack && decayStep == _freqDecay && max == _freqMax)
    return;

  _freqAttack = attackStep;
  _freqDecay = decayStep;
  _freqMax = max;
  putOnKnobs (FreqAttack, attackStep, decayStep, max);
}

void
ActionComponent::setQEnvelope (int attackStep, int decayStep, float max)
{
  if (attackStep == _qAttack && decayStep == _qDecay && max == _qMax)
    return;

  _qAttack = attackStep;
  _qDecay = decayStep;
  _qMax = max;
  putOnKnobs (QAttack, attackStep, decayStep, max);
}

void
ActionComponent::setScript (juce::String const &script)
{
  // Never while it is being typed into, and otherwise only when it is
  // actually different: the page refreshes on a timer, and either would throw
  // away what is being written and put the caret back at the top.
  if (_editing || script == _document.getAllContent ())
    return;

  _document.replaceAllContent (script);
  _document.clearUndoHistory ();
  _document.setSavePoint ();
  repaint ();
}

void
ActionComponent::setScriptErrors (juce::StringArray const &errors)
{
  if (errors == _scriptErrors)
    return;

  _scriptErrors = errors;
  repaint ();
}

void
ActionComponent::setActionChoices (juce::StringArray const &names)
{
  if (names == _choices)
    return;

  _choices = names;
  repaint ();
}

void
ActionComponent::setScriptIsShipped (bool shipped)
{
  if (_shipped == shipped)
    return;

  _shipped = shipped;
  repaint ();
}

void
ActionComponent::stopEditingScript ()
{
  if (!_editing)
    return;

  _editing = false;
  _editor->setReadOnly (true);
  if (onScriptEditingChanged)
    onScriptEditingChanged (false);

  repaint ();
}

void
ActionComponent::focusLost (FocusChangeType)
{
  // The keyboard follows the focus, so losing it is the end of the edit
  // whatever took it away.
  stopEditingScript ();
}

int
ActionComponent::visibleScriptLines () const
{
  auto const lineH = scriptLineHeight ();
  if (lineH <= 0)
    return 1;

  return juce::jmax (1, scriptTextArea ().getHeight () / lineH);
}


void
ActionComponent::openActionList ()
{
  // Reaching for the list is leaving the editor wherever the tap came from,
  // and it is what takes the keyboard away with it.
  stopEditingScript ();

  _listOpen = true;
  updateScriptLayers ();

  // Opened onto whatever is already chosen, moved as little as possible: a
  // list that always opens at the top makes you scroll back to where you were
  // every single time.
  _listTop = a3::scrollToShow (_listTop, _choices.indexOf (_actionName),
                               actionListVisibleRows (_layout),
                               _choices.size ());
}

void
ActionComponent::closeActionList ()
{
  _listOpen = false;
  updateScriptLayers ();
}

void
ActionComponent::updateScriptLayers ()
{
  // The list and the editor stand in one area, and one of the two is on
  // screen at a time. Drawing the list opaque is not enough: the editor is a
  // child, a child is painted after its parent by construction, and its own
  // ground is transparent so the field behind it can show -- so the script
  // was drawn over the list whatever the list did, and the two were read at
  // once. No toFront() helps; the editor has to go.
  if (_editor)
    _editor->setVisible (!_listOpen);

  // The touch area goes the other way: it lies over the editor only while the
  // list does, or it would answer every touch meant for the text.
  if (_scriptTouch)
    _scriptTouch->setVisible (_listOpen);
}

void
ActionComponent::chooseFromActionList (juce::Point<int> point)
{
  closeActionList ();

  auto const rowH = juce::jmax (1, _layout.actionListRowHeight);
  auto const inY = point.y
                   - (_layout.actionListArea.getY ()
                      - _layout.scriptField.getY ());

  auto const row = _listTop + inY / rowH;
  if (juce::isPositiveAndBelow (row, _choices.size ()) && onActionChosen)
    onActionChosen (_choices[row]);

  repaint ();
}


void
ActionComponent::setGridReference (juce::Rectangle<int> barCoordinates)
{
  if (barCoordinates == _gridReference)
    return;

  // Geometry, so a re-layout rather than a repaint.
  _gridReference = barCoordinates;
  resized ();
  repaint ();
}

void
ActionComponent::setActionName (juce::String const &name)
{
  if (name == _actionName)
    return;

  _actionName = name;
  repaint ();
}

void
ActionComponent::setActMode (int mode)
{
  if (mode == _actMode)
    return;

  _actMode = mode;
  repaint ();
}

void
ActionComponent::paintActionField (juce::Graphics &g)
{
  auto const bounds = _layout.actionField;
  if (bounds.isEmpty ())
    return;

  auto const named = _actionName.isNotEmpty ();

  // A field that carries something stands in the channel's colour, the way
  // everything else on the device says whose it is. It was one grey for every
  // channel, which reads as furniture rather than as a thing that belongs to
  // the deck you are on.
  g.setColour (named ? _channelColour.withAlpha (theme ().alphaFillEmphasis)
                     : toColour (theme ().textPrimary, theme ().alphaFill));
  g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);
  g.setColour (named ? _channelColour.withAlpha (theme ().alphaInactive)
                     : toColour (theme ().textPrimary, theme ().alphaOutline));
  g.drawRoundedRectangle (bounds.toFloat (), theme ().radiusControl,
                          theme ().strokeThin);

  // The channel's colour where it can be read on this ground, the theme's
  // text where it cannot -- channel four's blue vanished into the bar. See
  // readableInk().
  auto const ground = toColour (theme ().background);
  g.setColour (named ? readableInk (_channelColour, ground,
                                    toColour (theme ().textPrimary))
                     : toColour (theme ().textMuted, theme ().alphaMuted));
  // What the action's own name may cost.
  constexpr float actionNameCap = 24.f;
  g.setFont (juce::Font (juce::FontOptions (
      fittedFontHeight (bounds.getHeight () * 0.45f, actionNameCap))));
  g.drawText (named ? _actionName : juce::String ("no action"),
              bounds.reduced (bounds.getHeight () / 3, 0),
              juce::Justification::centredLeft);

  g.setColour (toColour (theme ().textMuted, theme ().alphaSecondary));
  // What the "action" caption beside it may cost.
  constexpr float actionCaptionCap = 12.f;
  g.setFont (juce::Font (juce::FontOptions (
      fittedFontHeight (bounds.getHeight () / 4.f, actionCaptionCap))));
  g.drawText ("action", bounds.reduced (bounds.getHeight () / 3, 0),
              juce::Justification::centredRight);
}

juce::Font
ActionComponent::scriptFont () const
{
  // Monospaced, because a script is read by column as much as by line: what
  // lines up under what is half of how you find your way in one.
  auto const size = juce::jlimit (
      9.f, 15.f, theme ().fontSize (FontRole::Body) * 0.8f);

  return juce::Font (juce::FontOptions (
      juce::Font::getDefaultMonospacedFontName (), size, juce::Font::plain));
}

juce::Rectangle<int>
ActionComponent::scriptTextArea () const
{
  auto area = _layout.scriptTextField.reduced (
      juce::jmax (6, _layout.scriptField.getHeight () / 40));

  // The errors take their room off the text rather than being drawn over it:
  // a message on top of the line it is about hides the line it is about.
  if (!_scriptErrors.isEmpty ())
    area.removeFromBottom (
        juce::jmin (area.getHeight () / 2,
                    _scriptErrors.size () * scriptLineHeight ()));

  return area;
}

int
ActionComponent::scriptLineHeight () const
{
  return juce::jmax (1, juce::roundToInt (scriptFont ().getHeight () * 1.25f));
}

float
ActionComponent::scriptCharacterWidth () const
{
  // Every character is the same width in a monospaced face, so one of them
  // measures all of them.
  return juce::jmax (1.f, juce::GlyphArrangement::getStringWidth (
                              scriptFont (), "M"));
}

void
ActionComponent::paintScriptField (juce::Graphics &g)
{
  auto const bounds = _layout.scriptField;
  if (bounds.isEmpty ())
    return;

  // Darker than the page and squared off, because this is a terminal and
  // reads as one: a script is text you scan line by line, not a control.
  g.setColour (toColour (theme ().background).darker (0.4f));
  g.fillRect (bounds);

  // The edge says whether it is being typed into and whether what is in it
  // has been written -- three states, one line, no words spent on any of it.
  auto const edited = _document.hasChangedSinceSavePoint ();
  g.setColour (edited     ? toColour (theme ().warning)
               : _editing ? _channelColour
                          : toColour (theme ().textPrimary,
                                      theme ().alphaOutline));
  g.drawRect (bounds, juce::roundToInt (_editing || edited
                                            ? theme ().strokeThick
                                            : theme ().strokeThin));

  // The text itself is the editor's (ScriptEditor), which stands inside this
  // frame and draws its own lines, numbers and caret.
  if (_document.getNumCharacters () == 0 && !_editing)
    {
      g.setColour (toColour (theme ().textMuted, theme ().alphaMuted));
      g.setFont (scriptFont ());
      g.drawText ("-- no script --", scriptTextArea (),
                  juce::Justification::topLeft);
    }
}

void
ActionComponent::paintScriptErrors (juce::Graphics &g)
{
  if (_scriptErrors.isEmpty ())
    return;

  auto const lineH = scriptLineHeight ();
  auto at = _layout.scriptTextField
                .reduced (juce::jmax (6, _layout.scriptField.getHeight () / 40))
                .removeFromBottom (_scriptErrors.size () * lineH);

  g.setFont (scriptFont ());
  g.setColour (toColour (theme ().danger));

  for (auto const &error : _scriptErrors)
    {
      g.drawText (error, at.removeFromTop (lineH),
                  juce::Justification::centredLeft);
      if (at.isEmpty ())
        break;
    }
}

void
ActionComponent::paintScriptKeys (juce::Graphics &g)
{
  if (_layout.saveButton.isEmpty ())
    return;

  auto const edited = _document.hasChangedSinceSavePoint ();

  auto const key = [&g, this] (juce::Rectangle<int> at, char const *word,
                               juce::Colour ink) {
    g.setColour (toColour (theme ().textPrimary, theme ().alphaFill));
    g.fillRoundedRectangle (at.toFloat (), theme ().radiusControl);
    g.setColour (toColour (theme ().textPrimary, theme ().alphaOutline));
    g.drawRoundedRectangle (at.toFloat (), theme ().radiusControl,
                            theme ().strokeThin);

    g.setColour (ink);
    // What "save"/"cancel" may cost.
    constexpr float scriptKeyCap = 16.f;
    g.setFont (juce::Font (juce::FontOptions (
        fittedFontHeight (at.getHeight () * 0.4f, scriptKeyCap))));
    g.drawText (word, at, juce::Justification::centred);
  };

  auto const lit = readableInk (_channelColour, toColour (theme ().background),
                                toColour (theme ().textPrimary));
  auto const dark = toColour (theme ().textMuted, theme ().alphaDisabled);

  // Lit only while there is something to keep or to lose: a key offering to
  // save nothing is a key you have to stop and think about.
  //
  // Save stays dark on a shipped action however much has been typed: writing
  // over one of those would take it from every clip that uses it, and there
  // is no getting it back. Save as is the way out, which is why it is lit in
  // exactly that case.
  key (_layout.saveButton, "save", edited && !_shipped ? lit : dark);
  key (_layout.saveAsButton, "save as", edited ? lit : dark);
  key (_layout.cancelButton, "cancel",
       edited ? toColour (theme ().textPrimary, theme ().alphaTextStrong)
              : dark);
}

void
ActionComponent::paintActionList (juce::Graphics &g)
{
  if (!_listOpen)
    return;

  auto const area = _layout.actionListArea;
  auto const rowH = _layout.actionListRowHeight;

  // Opaque before the wash: the card colour is translucent by design, and on
  // its own the list and the script under it were drawn through each other.
  g.setColour (toColour (theme ().background));
  g.fillRect (area);
  g.setColour (toColour (theme ().textPrimary, theme ().alphaFill));
  g.fillRect (area);
  g.setColour (_channelColour);
  g.drawRect (area, juce::roundToInt (theme ().strokeThin));

  // What a row of the action list may cost.
  constexpr float actionListRowCap = 18.f;
  g.setFont (juce::Font (juce::FontOptions (
      fittedFontHeight (rowH * 0.45f, actionListRowCap))));

  for (int row = 0; row * rowH < area.getHeight (); ++row)
    {
      auto const index = _listTop + row;
      if (index >= _choices.size ())
        break;

      auto const at = area.withY (area.getY () + row * rowH).withHeight (rowH);
      auto const name = _choices[index];
      auto const chosen = name == _actionName;

      if (chosen)
        {
          g.setColour (_channelColour.withAlpha (theme ().alphaDisabled));
          g.fillRect (at.reduced (juce::roundToInt (theme ().paddingTight),
                                  juce::roundToInt (theme ().paddingHair)));
        }

      g.setColour (chosen ? _channelColour
                          : toColour (theme ().textPrimary,
                                     theme ().alphaTextStrong));
      g.drawText (name.isEmpty () ? juce::String ("no action") : name,
                  at.reduced (rowH / 3, 0), juce::Justification::centredLeft);
    }
}

void
ActionComponent::paint (juce::Graphics &g)
{
  // Solid, because this covers the clip bar's sections rather than sitting
  // beside them: a page that does not fill every pixel shows them through its
  // own gaps.
  g.fillAll (toColour (theme ().background));

  // The knobs stand on a card like every other block of controls in the bar.
  // Its colour is the bar's own resting card wash, so the page reads as part
  // of the same furniture rather than as a panel of its own.
  g.setColour (toColour (theme ().textPrimary, theme ().alphaFill));
  g.fillRoundedRectangle (_layout.card.toFloat (), theme ().radiusCard);

  paintActionField (g);
  paintScriptField (g);
  paintScriptErrors (g);
  paintScriptKeys (g);

  // Three envelopes, one row each, all the same shape: atk over atk over atk.
  // The accent is read first because it is what ACT has always done, then the
  // cutoff, then the resonance.
  // The nine knobs draw themselves (PotKnob); the page draws what stands
  // between them.

  // Which row is which, said once each rather than on every knob.
  // "3d", not "accent": what the row drives is the channel's 3d, and naming
  // it after the thing it moves puts it in the same words as the global
  // strip's rows -- which is where the eye has already learned them.
  char const *const rowNames[] = { "3d", caption::frequency, "q" };
  g.setColour (toColour (theme ().textMuted, theme ().alphaTextStrong));
  // What a row's name ("3d", "freq", "q") may cost.
  constexpr float rowNameCap = 14.f;
  g.setFont (juce::Font (juce::FontOptions (fittedFontHeight (
      _layout.rowLabels[0].getHeight () * 0.4f, rowNameCap))));
  for (int row = 0; row < ActionLayout::numRows; ++row)
    g.drawText (rowNames[row],
                _layout.rowLabels[static_cast<size_t> (row)].withTrimmedRight (
                    juce::roundToInt (theme ().paddingSmall)),
                juce::Justification::centredRight);

  // Not a knob: it is one of two words, and a knob that can only be at one of
  // two places is a knob that lies about what it can do. It stands beside the
  // action's name because it says what a press does to all three envelopes,
  // so it belongs to none of their rows.
  auto const modeBounds = _layout.actModeField;
  g.setColour (_channelColour.withAlpha (theme ().alphaFillEmphasis));
  g.fillRoundedRectangle (modeBounds.toFloat (), theme ().radiusControl);
  g.setColour (_channelColour.withAlpha (theme ().alphaInactive));
  g.drawRoundedRectangle (modeBounds.toFloat (), theme ().radiusControl,
                          theme ().strokeThin);

  g.setColour (readableInk (_channelColour, toColour (theme ().background),
                            toColour (theme ().textPrimary)));
  // What the act-mode's own word ("1shot"/"Hold") may cost.
  constexpr float actModeCap = 20.f;
  g.setFont (juce::Font (juce::FontOptions (
      fittedFontHeight (modeBounds.getHeight () / 3.f, actModeCap))));
  g.drawText (value::actModeNames[juce::jlimit (0, value::numActModes - 1,
                                                _actMode)],
              modeBounds, juce::Justification::centred);

  g.setColour (toColour (theme ().textMuted, theme ().alphaSecondary));
  // What the act-mode caption beneath it may cost.
  constexpr float actModeCaptionCap = 12.f;
  g.setFont (juce::Font (juce::FontOptions (
      fittedFontHeight (modeBounds.getHeight () / 5.f, actModeCaptionCap))));
  g.drawText (caption::actMode,
              modeBounds.withTrimmedTop (modeBounds.getHeight () * 2 / 3),
              juce::Justification::centred);

  // What the card of knobs is: the accent and the two filters, which is the
  // channel's audio. Nine unnamed knobs beside a script is a card you have to
  // work out.
  if (!_layout.cardCaption.isEmpty ())
    {
      g.setColour (toColour (theme ().textMuted, theme ().alphaTextStrong));
      // What the "Audio" caption over the knob card may cost.
      constexpr float audioCaptionCap = 16.f;
      g.setFont (juce::Font (juce::FontOptions (fittedFontHeight (
          _layout.cardCaption.getHeight () * 0.7f, audioCaptionCap))));
      g.drawText ("Audio", _layout.cardCaption,
                  juce::Justification::centred);
    }

  // And the key that fires it, under the knobs it sets. Filled rather than
  // outlined: it is the one thing on this page that happens now, and it is
  // pressed with one hand while the other is on the crossfader.
  if (!_layout.fireButton.isEmpty ())
    {
      auto const at = _layout.fireButton.toFloat ();

      // Full opacity while firing rather than an alpha rung: firing has
      // always meant no dimming at all, which the alpha-less colour already
      // says. This used to be `_firing ? 1.f : 0.75f`; 1.f fits no rung, and
      // full opacity is the absence of an emphasis decision rather than one
      // of its rungs, so it deliberately gets no role of its own. See
      // issues/a3-motion-ui-metric-role-deviations.md (Task 16). The resting
      // branch is a second, separate deviation: 0.75f itself fits no rung
      // either -- see theme ().alphaSecondary above.
      g.setColour (_firing ? _channelColour
                           : _channelColour.withAlpha (
                                 theme ().alphaSecondary));
      g.fillRoundedRectangle (at, theme ().radiusControl);
      g.setColour (_channelColour);
      g.drawRoundedRectangle (at, theme ().radiusControl,
                              theme ().strokeThick);

      g.setColour (readableInk (toColour (theme ().textPrimary),
                                _channelColour,
                                toColour (theme ().background)));
      // What "ACT" itself may cost -- the loudest label on this page.
      constexpr float fireButtonLabelCap = 26.f;
      g.setFont (juce::Font (juce::FontOptions (
          fittedFontHeight (at.getHeight () * 0.5f, fireButtonLabelCap),
          juce::Font::bold)));
      g.drawText ("ACT", _layout.fireButton, juce::Justification::centred);
    }

  // Last, so it covers what it opens over.
  paintActionList (g);
}

}
