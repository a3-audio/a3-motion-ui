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
#include <a3-motion-ui/components/ListScroll.hh>
#include <a3-motion-ui/theme/TransportLook.hh>
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
  for (int i = 0; i < numControls; ++i)
    {
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
      _listOpen = false;
    else
      openActionList ();

    repaint ();
  };
  addAndMakeVisible (*_actionTouch);

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

    caretFromPoint (at);

    if (!_editing)
      {
        _editing = true;
        grabKeyboardFocus ();
        if (onScriptEditingChanged)
          onScriptEditingChanged (true);
      }

    repaint ();
  };
  _scriptTouch->onDragIncrement = [this] (int, int, int increment) {
    // The page follows the finger, the way it does on a phone: upwards
    // carries the text up and brings later lines into view. The increment
    // goes in as it arrives -- it was negated here, which ran the editor
    // against the hand, the same way the overlay strips once did. See
    // ScriptBuffer::scrollByDrag(), which is where that sign is tested.
    //
    // The open list is a list like any other and follows the same rule: it
    // used to return here, which left every script past the sixth
    // unreachable.
    if (_listOpen)
      _listTop = a3::scrollBy (_listTop, increment,
                               actionListVisibleRows (_layout),
                               _choices.size ());
    else
      _buffer.scrollByDrag (increment, visibleScriptLines ());

    repaint ();
  };
  addAndMakeVisible (*_scriptTouch);

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

  _saveTouch = std::make_unique<TouchControl> ();
  _saveTouch->onTap = [this] (int, int) {
    if (!_buffer.isEdited ())
      return;

    _buffer.markSaved ();
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
  resized ();
  repaint ();
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
    _touch[static_cast<size_t> (i)]->setBounds (
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
  if (_cancelTouch)
    _cancelTouch->setBounds (_layout.cancelButton);

  _buffer.bringCaretIntoView (visibleScriptLines ());
}

void
ActionComponent::setTarget (int channel, int slot, juce::Colour channelColour)
{
  _channel = channel;
  _slot = slot;
  _channelColour = channelColour;
  repaint ();
}

void
ActionComponent::setEnvelope (int attackStep, int decayStep, float max)
{
  if (attackStep == _attack && decayStep == _decay && max == _max)
    return;

  _attack = attackStep;
  _decay = decayStep;
  _max = max;
  repaint ();
}

void
ActionComponent::setFreqEnvelope (int attackStep, int decayStep, float max)
{
  if (attackStep == _freqAttack && decayStep == _freqDecay && max == _freqMax)
    return;

  _freqAttack = attackStep;
  _freqDecay = decayStep;
  _freqMax = max;
  repaint ();
}

void
ActionComponent::setQEnvelope (int attackStep, int decayStep, float max)
{
  if (attackStep == _qAttack && decayStep == _qDecay && max == _qMax)
    return;

  _qAttack = attackStep;
  _qDecay = decayStep;
  _qMax = max;
  repaint ();
}

void
ActionComponent::setScript (juce::String const &script)
{
  // Never while it is being typed into, and otherwise only when it is
  // actually different: the page refreshes on a timer, and either would throw
  // away what is being written and put the caret back at the top.
  if (_editing || script == _buffer.text ())
    return;

  _buffer.setText (script);
  _buffer.bringCaretIntoView (visibleScriptLines ());
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
ActionComponent::stopEditingScript ()
{
  if (!_editing)
    return;

  _editing = false;
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
ActionComponent::caretFromPoint (juce::Point<int> point)
{
  auto const text = scriptTextArea ();
  auto const lineH = scriptLineHeight ();
  if (lineH <= 0)
    return;

  // The point comes in relative to the script's own control, which stands on
  // scriptField -- so the inset between the two has to come off before it
  // means a line.
  auto const inX = point.x - (text.getX () - _layout.scriptField.getX ());
  auto const inY = point.y - (text.getY () - _layout.scriptField.getY ());

  auto const line = _buffer.firstVisibleLine () + inY / lineH;
  auto const column = static_cast<int> (
      std::lround (inX / juce::jmax (1.f, scriptCharacterWidth ())));

  _buffer.placeCaret (line, column);
  _buffer.bringCaretIntoView (visibleScriptLines ());
}

void
ActionComponent::openActionList ()
{
  _listOpen = true;

  // Opened onto whatever is already chosen, moved as little as possible: a
  // list that always opens at the top makes you scroll back to where you were
  // every single time.
  _listTop = a3::scrollToShow (_listTop, _choices.indexOf (_actionName),
                               actionListVisibleRows (_layout),
                               _choices.size ());
}

void
ActionComponent::chooseFromActionList (juce::Point<int> point)
{
  _listOpen = false;

  auto const rowH = juce::jmax (1, _layout.actionListRowHeight);
  auto const inY = point.y
                   - (_layout.actionListArea.getY ()
                      - _layout.scriptField.getY ());

  auto const row = _listTop + inY / rowH;
  if (juce::isPositiveAndBelow (row, _choices.size ()) && onActionChosen)
    onActionChosen (_choices[row]);

  repaint ();
}

bool
ActionComponent::keyPressed (juce::KeyPress const &key)
{
  if (!_editing)
    return false;

  // Only a repaint: writing happens on Save. The editor's edge says there is
  // something unsaved, which is what the two keys are for.
  auto const changed = [this] {
    _buffer.bringCaretIntoView (visibleScriptLines ());
    repaint ();
  };

  if (key == juce::KeyPress::escapeKey)
    {
      stopEditingScript ();
      return true;
    }

  if (key == juce::KeyPress::backspaceKey)
    {
      _buffer.backspace ();
      changed ();
      return true;
    }

  if (key == juce::KeyPress::returnKey)
    {
      _buffer.type ('\n');
      changed ();
      return true;
    }

  // Moving is not an edit, so it does not go through changed().
  auto const move = [this] (int lines, int columns) {
    _buffer.moveCaret (lines, columns);
    _buffer.bringCaretIntoView (visibleScriptLines ());
    repaint ();
    return true;
  };

  if (key == juce::KeyPress::leftKey)
    return move (0, -1);
  if (key == juce::KeyPress::rightKey)
    return move (0, 1);
  if (key == juce::KeyPress::upKey)
    return move (-1, 0);
  if (key == juce::KeyPress::downKey)
    return move (1, 0);

  auto const character = key.getTextCharacter ();
  if (character == 0)
    return false;

  _buffer.type (character);
  changed ();
  return true;
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
  g.setColour (named ? _channelColour.withAlpha (0.28f)
                     : toColour (theme ().textPrimary, 0.06f));
  g.fillRoundedRectangle (bounds.toFloat (), 3.f);
  g.setColour (named ? _channelColour.withAlpha (0.6f)
                     : toColour (theme ().textPrimary, 0.15f));
  g.drawRoundedRectangle (bounds.toFloat (), 3.f, 1.f);

  // The channel's colour where it can be read on this ground, the theme's
  // text where it cannot -- channel four's blue vanished into the bar. See
  // readableInk().
  auto const ground = toColour (theme ().background);
  g.setColour (named ? readableInk (_channelColour, ground,
                                    toColour (theme ().textPrimary))
                     : toColour (theme ().textMuted, 0.5f));
  g.setFont (juce::Font (juce::FontOptions (
      juce::jmin (24.f, bounds.getHeight () * 0.45f))));
  g.drawText (named ? _actionName : juce::String ("no action"),
              bounds.reduced (bounds.getHeight () / 3, 0),
              juce::Justification::centredLeft);

  g.setColour (toColour (theme ().textMuted, 0.7f));
  g.setFont (juce::Font (juce::FontOptions (
      juce::jmin (12.f, bounds.getHeight () / 4.f))));
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
  g.setColour (_buffer.isEdited () ? toColour (theme ().warning)
               : _editing         ? _channelColour
                                  : toColour (theme ().textPrimary, 0.15f));
  g.drawRect (bounds, _editing || _buffer.isEdited () ? 2 : 1);

  auto const text = scriptTextArea ();
  auto const lineH = scriptLineHeight ();
  auto const charW = scriptCharacterWidth ();

  g.setFont (scriptFont ());

  if (_buffer.numLines () == 1 && _buffer.line (0).isEmpty () && !_editing)
    {
      g.setColour (toColour (theme ().textMuted, 0.5f));
      g.drawText ("-- no script --", text, juce::Justification::topLeft);
      return;
    }

  auto const first = _buffer.firstVisibleLine ();
  auto const rows = visibleScriptLines ();

  for (int row = 0; row < rows; ++row)
    {
      auto const index = first + row;
      if (index >= _buffer.numLines ())
        break;

      auto const line = _buffer.line (index);
      auto const at = text.withY (text.getY () + row * lineH)
                          .withHeight (lineH);

      // Comments in the muted colour, the one thing worth colouring: it is
      // what tells a written-out script from one somebody explained.
      g.setColour (line.trimStart ().startsWith ("//")
                       ? toColour (theme ().textMuted, 0.6f)
                       : toColour (theme ().textPrimary, 0.85f));
      g.drawText (line, at, juce::Justification::centredLeft);
    }

  if (!_editing)
    return;

  // The caret, where the next character goes.
  auto const caretRow = _buffer.caretLine () - first;
  if (!juce::isPositiveAndBelow (caretRow, rows))
    return;

  auto const x = text.getX ()
                 + juce::roundToInt (_buffer.caretColumn () * charW);

  g.setColour (_channelColour);
  g.fillRect (x, text.getY () + caretRow * lineH, 2, lineH);
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

  auto const edited = _buffer.isEdited ();

  auto const key = [&g, this] (juce::Rectangle<int> at, char const *word,
                               juce::Colour ink) {
    g.setColour (toColour (theme ().textPrimary, 0.08f));
    g.fillRoundedRectangle (at.toFloat (), 3.f);
    g.setColour (toColour (theme ().textPrimary, 0.15f));
    g.drawRoundedRectangle (at.toFloat (), 3.f, 1.f);

    g.setColour (ink);
    g.setFont (juce::Font (juce::FontOptions (
        juce::jmin (16.f, at.getHeight () * 0.4f))));
    g.drawText (word, at, juce::Justification::centred);
  };

  // Lit only while there is something to keep or to lose: a key offering to
  // save nothing is a key you have to stop and think about.
  key (_layout.saveButton, "save",
       edited ? readableInk (_channelColour, toColour (theme ().background),
                             toColour (theme ().textPrimary))
              : toColour (theme ().textMuted, 0.4f));
  key (_layout.cancelButton, "cancel",
       edited ? toColour (theme ().textPrimary, 0.85f)
              : toColour (theme ().textMuted, 0.4f));
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
  g.setColour (toColour (theme ().textPrimary, 0.08f));
  g.fillRect (area);
  g.setColour (_channelColour);
  g.drawRect (area, 1);

  g.setFont (juce::Font (juce::FontOptions (
      juce::jmin (18.f, rowH * 0.45f))));

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
          g.fillRect (at.reduced (2, 1));
        }

      g.setColour (chosen ? _channelColour
                          : toColour (theme ().textPrimary, 0.85f));
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
  g.setColour (toColour (theme ().textPrimary, 0.04f));
  g.fillRoundedRectangle (_layout.card.toFloat (), 8.f);

  paintActionField (g);
  paintScriptField (g);
  paintScriptErrors (g);
  paintScriptKeys (g);

  auto const &metrics = _layout.metrics;

  // Three envelopes, one row each, all the same shape: atk over atk over atk.
  // The accent is read first because it is what ACT has always done, then the
  // cutoff, then the resonance.
  int const steps[]
      = { _attack, _decay, 0, _freqAttack, _freqDecay, 0, _qAttack, _qDecay, 0 };
  float const ceilings[] = { _max, _freqMax, _qMax };

  for (int row = 0; row < ActionLayout::numRows; ++row)
    {
      auto const base = row * 3;
      paintBarKnob (g, _layout.controls[static_cast<size_t> (base)], metrics,
                    _channelColour, caption::attack,
                    envFrac (steps[base]), false, false, true);
      paintBarKnob (g, _layout.controls[static_cast<size_t> (base + 1)],
                    metrics, _channelColour, caption::decay,
                    envFrac (steps[base + 1]), false, false, true);
      paintBarKnob (g, _layout.controls[static_cast<size_t> (base + 2)],
                    metrics, _channelColour, caption::envelopeMax,
                    ceilings[row] * 2.f - 1.f, false, false, true);
    }

  // Which row is which, said once each rather than on every knob.
  // "3d", not "accent": what the row drives is the channel's 3d, and naming
  // it after the thing it moves puts it in the same words as the global
  // strip's rows -- which is where the eye has already learned them.
  char const *const rowNames[] = { "3d", caption::frequency, "q" };
  g.setColour (toColour (theme ().textMuted, 0.8f));
  g.setFont (juce::Font (juce::FontOptions (
      juce::jmin (14.f, _layout.rowLabels[0].getHeight () * 0.4f))));
  for (int row = 0; row < ActionLayout::numRows; ++row)
    g.drawText (rowNames[row], _layout.rowLabels[static_cast<size_t> (row)]
                                   .withTrimmedRight (4),
                juce::Justification::centredRight);

  // Not a knob: it is one of two words, and a knob that can only be at one of
  // two places is a knob that lies about what it can do. It stands beside the
  // action's name because it says what a press does to all three envelopes,
  // so it belongs to none of their rows.
  auto const modeBounds = _layout.actModeField;
  g.setColour (_channelColour.withAlpha (0.28f));
  g.fillRoundedRectangle (modeBounds.toFloat (), 3.f);
  g.setColour (_channelColour.withAlpha (0.6f));
  g.drawRoundedRectangle (modeBounds.toFloat (), 3.f, 1.f);

  g.setColour (readableInk (_channelColour, toColour (theme ().background),
                            toColour (theme ().textPrimary)));
  g.setFont (juce::Font (juce::FontOptions (
      juce::jmin (20.f, modeBounds.getHeight () / 3.f))));
  g.drawText (value::actModeNames[juce::jlimit (0, value::numActModes - 1,
                                                _actMode)],
              modeBounds, juce::Justification::centred);

  g.setColour (toColour (theme ().textMuted, 0.7f));
  g.setFont (juce::Font (juce::FontOptions (
      juce::jmin (12.f, modeBounds.getHeight () / 5.f))));
  g.drawText (caption::actMode,
              modeBounds.withTrimmedTop (modeBounds.getHeight () * 2 / 3),
              juce::Justification::centred);

  // What the card of knobs is: the accent and the two filters, which is the
  // channel's audio. Nine unnamed knobs beside a script is a card you have to
  // work out.
  if (!_layout.cardCaption.isEmpty ())
    {
      g.setColour (toColour (theme ().textMuted, 0.8f));
      g.setFont (juce::Font (juce::FontOptions (juce::jmin (
          16.f, _layout.cardCaption.getHeight () * 0.7f))));
      g.drawText ("Audio", _layout.cardCaption,
                  juce::Justification::centred);
    }

  // And the key that fires it, under the knobs it sets. Filled rather than
  // outlined: it is the one thing on this page that happens now, and it is
  // pressed with one hand while the other is on the crossfader.
  if (!_layout.fireButton.isEmpty ())
    {
      auto const at = _layout.fireButton.toFloat ();

      g.setColour (_channelColour.withAlpha (_firing ? 1.f : 0.75f));
      g.fillRoundedRectangle (at, 4.f);
      g.setColour (_channelColour);
      g.drawRoundedRectangle (at, 4.f, 2.f);

      g.setColour (readableInk (toColour (theme ().textPrimary),
                                _channelColour,
                                toColour (theme ().background)));
      g.setFont (juce::Font (juce::FontOptions (
          juce::jmin (26.f, at.getHeight () * 0.5f), juce::Font::bold)));
      g.drawText ("ACT", _layout.fireButton, juce::Justification::centred);
    }

  // Last, so it covers what it opens over.
  paintActionList (g);
}

}
