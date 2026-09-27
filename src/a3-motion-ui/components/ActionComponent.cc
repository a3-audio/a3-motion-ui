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

  // The list to assign from, open all the time under the name since the
  // editor it took turns with went to FILES (2026-09-27). A tap assigns the
  // row under the finger; a drag scrolls it -- the page goes the finger's way.
  _listTouch = std::make_unique<TouchControl> ();
  _listTouch->onTapAt = [this] (int, int, juce::Point<int> at) {
    chooseFromActionList (at);
  };
  _listTouch->onDragIncrement = [this] (int, int, int increment) {
    _listTop = a3::scrollBy (_listTop, increment,
                             actionListVisibleRows (_layout),
                             _choices.size ());
    repaint ();
  };
  addAndMakeVisible (*_listTouch);

  // The six buttons are the six action pads on the screen (2026-09-28):
  // down chooses one and fires it, up lets a Hold action go.
  for (size_t button = 0; button < _fieldTouch.size (); ++button)
    {
      auto touch = std::make_unique<TouchControl> ();
      touch->onPress = [this, button] (int, int) {
        if (onButtonHeld)
          onButtonHeld (static_cast<int> (button), true);
      };
      touch->onRelease = [this, button] (int, int) {
        if (onButtonHeld)
          onButtonHeld (static_cast<int> (button), false);
      };
      addAndMakeVisible (*touch);
      _fieldTouch[button] = std::move (touch);
    }

  // Opens this action's script in FILES, beside the list there.
  _editTouch = std::make_unique<TouchControl> ();
  _editTouch->onTap = [this] (int, int) {
    if (onEditPressed)
      onEditPressed ();
  };
  addAndMakeVisible (*_editTouch);


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
      // No reference rows: the global strip's grid they lined up with went
      // into the mixer strips on 2026-09-26.
      {});

  // Every knob comes out of the rows; the mode does not stand in one.
  for (int i = 0; i < ActMode; ++i)
    _knob[static_cast<size_t> (i)]->setBounds (
        _layout.controls[static_cast<size_t> (i)]);

  _touch[ActMode]->setBounds (_layout.actModeField);

  for (size_t button = 0; button < _fieldTouch.size (); ++button)
    if (_fieldTouch[button])
      _fieldTouch[button]->setBounds (_layout.actionFields[button]);

  if (_listTouch)
    _listTouch->setBounds (_layout.actionListArea);
  if (_editTouch)
    _editTouch->setBounds (_layout.editButton);
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
ActionComponent::setActionChoices (juce::StringArray const &names)
{
  if (names == _choices)
    return;

  _choices = names;
  repaint ();
}

void
ActionComponent::chooseFromActionList (juce::Point<int> point)
{
  // The touch covers the list exactly, so its own y is the list's.
  auto const rowH = juce::jmax (1, _layout.actionListRowHeight);
  auto const inY = point.y;

  auto const row = _listTop + inY / rowH;
  if (juce::isPositiveAndBelow (row, _choices.size ()) && onActionChosen)
    onActionChosen (_choices[row]);

  repaint ();
}


void
ActionComponent::setActionName (juce::String const &name)
{
  if (name == _actionName)
    return;

  _actionName = name;
  // The chosen one in view, moved as little as possible: a list that jumps
  // to the top on every change makes you scroll back to where you were.
  _listTop = a3::scrollToShow (_listTop, _choices.indexOf (_actionName),
                               actionListVisibleRows (_layout),
                               _choices.size ());
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
ActionComponent::setActionButtons (std::array<juce::String, 6> const &names,
                                   int chosen)
{
  if (names == _buttonNames && chosen == _chosenButton)
    return;

  _buttonNames = names;
  _chosenButton = chosen;
  repaint ();
}

void
ActionComponent::setRunningButton (int button)
{
  if (button == _runningButton)
    return;

  _runningButton = button;
  repaint ();
}

void
ActionComponent::paintActionFields (juce::Graphics &g)
{
  auto const ground = toColour (theme ().background);
  auto const text = toColour (theme ().textPrimary);

  for (size_t button = 0; button < _layout.actionFields.size (); ++button)
    {
      auto const bounds = _layout.actionFields[button];
      if (bounds.isEmpty ())
        continue;

      auto const &name = _buttonNames[button];
      auto const named = name.isNotEmpty ();
      auto const chosen = static_cast<int> (button) == _chosenButton;
      auto const running = static_cast<int> (button) == _runningButton;

      // White while its action runs, as its pad goes white; otherwise the
      // channel's colour when it carries an action, and a grey when it does
      // not -- the same three states the pad shows.
      auto const fill
          = running ? text
                    : named ? _channelColour.withAlpha (
                                  chosen ? theme ().alphaInactive
                                         : theme ().alphaFillEmphasis)
                            : toColour (theme ().textPrimary,
                                        theme ().alphaFill);
      g.setColour (fill);
      g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);

      // The chosen one is the one everything right of it acts on, so it
      // carries the thick outline the shown channel's face carries.
      g.setColour (chosen ? _channelColour
                          : _channelColour.withAlpha (theme ().alphaDisabled));
      g.drawRoundedRectangle (bounds.toFloat (), theme ().radiusControl,
                              chosen ? theme ().strokeThick
                                     : theme ().strokeThin);

      auto const ink
          = running ? padGlyphInk (text)
                    : named ? readableInk (_channelColour, ground, text)
                            : toColour (theme ().textMuted,
                                        theme ().alphaMuted);
      auto inner = bounds.reduced (bounds.getHeight () / 8);
      auto const numberRow = inner.removeFromTop (inner.getHeight () / 2);

      // What the button's number (A1..A6) may cost.
      constexpr float buttonNumberCap = 20.f;
      g.setColour (ink);
      g.setFont (juce::Font (juce::FontOptions (
          fittedFontHeight (numberRow.getHeight () * 0.8f, buttonNumberCap),
          juce::Font::bold)));
      g.drawText ("A" + juce::String (button + 1), numberRow,
                  juce::Justification::centredLeft);

      // What an action's name in its field may cost.
      constexpr float buttonNameCap = 14.f;
      g.setFont (juce::Font (juce::FontOptions (
          fittedFontHeight (inner.getHeight () * 0.7f, buttonNameCap))));
      g.drawText (named ? name : juce::String ("--"), inner,
                  juce::Justification::centredLeft, true);
    }
}

void
ActionComponent::paintEditKey (juce::Graphics &g)
{
  auto const at = _layout.editButton;
  if (at.isEmpty ())
    return;

  // A key like the script's own were: it goes somewhere rather than setting
  // anything, so it wears no channel fill.
  g.setColour (toColour (theme ().textPrimary, theme ().alphaFill));
  g.fillRoundedRectangle (at.toFloat (), theme ().radiusControl);
  g.setColour (toColour (theme ().textPrimary, theme ().alphaOutline));
  g.drawRoundedRectangle (at.toFloat (), theme ().radiusControl,
                          theme ().strokeThin);

  g.setColour (readableInk (_channelColour, toColour (theme ().background),
                            toColour (theme ().textPrimary)));
  // What "EDIT" may cost.
  constexpr float editKeyCap = 18.f;
  g.setFont (juce::Font (juce::FontOptions (
      fittedFontHeight (at.getHeight () * 0.4f, editKeyCap))));
  g.drawText ("EDIT", at, juce::Justification::centred);
}

void
ActionComponent::paintActionList (juce::Graphics &g)
{
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

  paintActionFields (g);
  paintEditKey (g);

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
  // two places is a knob that lies about what it can do. It stands under
  // EDIT because it says what a press does to all three envelopes, so it
  // belongs to none of their rows.
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
      fittedFontHeight (modeBounds.getHeight () * 0.4f, actModeCap))));
  g.drawText (value::actModeNames[juce::jlimit (0, value::numActModes - 1,
                                                _actMode)],
              modeBounds, juce::Justification::centred);


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

  // Last, so it covers what it opens over.
  paintActionList (g);
}

}
