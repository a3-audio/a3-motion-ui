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
#include <a3-motion-ui/components/ActionChain.hh>
#include <a3-motion-ui/components/ActionKnobs.hh>
#include <a3-motion-ui/components/ActionMotionKnobs.hh>

#include <cmath>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

#include <cmath>

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

  // The six buttons choose (2026-09-28): what the rest of the page shows
  // is the chosen one's. They fire nothing -- the pads do that, on the
  // panel and the PADS page, and a push there brings this page up.
  for (size_t button = 0; button < _fieldTouch.size (); ++button)
    {
      auto touch = std::make_unique<TouchControl> ();
      touch->onPress = [this, button] (int, int) {
        if (onButtonChosen)
          onButtonChosen (static_cast<int> (button));
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

  // What comes after the accent: a tap steps --, A1 .. A6 and round, two
  // taps put it back to nothing. Seven places are few enough to tap
  // through, and a list over the page would cover what it is chosen for.
  _afterTouch = std::make_unique<TouchControl> ();
  _afterTouch->onTap = [this] (int, int) {
    if (onAfterStepped)
      onAfterStepped (1);
  };
  _afterTouch->onDoubleTap = [this] (int, int) {
    if (onAfterCleared)
      onAfterCleared ();
  };
  addAndMakeVisible (*_afterTouch);

  // Which tile the card shows. A tap, not a toggle: each key names its tile.
  _audioKeyTouch = std::make_unique<TouchControl> ();
  _audioKeyTouch->onTap = [this] (int, int) { setTile (ActionTile::Audio); };
  addAndMakeVisible (*_audioKeyTouch);
  _motionKeyTouch = std::make_unique<TouchControl> ();
  _motionKeyTouch->onTap = [this] (int, int) { setTile (ActionTile::Motion); };
  addAndMakeVisible (*_motionKeyTouch);

  buildMotionControls ();
  showTheTile ();
}

void
ActionComponent::buildMotionControls ()
{
  for (auto const param : motionParamOrder)
    {
      auto const at = static_cast<size_t> (param);

      if (motionParamIsAField (param))
        {
          // As on CLIP: a drag steps the speed, a tap brings direction and
          // end round. Two taps, on any of them, give it back to the script.
          auto field = std::make_unique<TouchControl> ();
          auto const step = [this, param] (int increment) {
            if (onMotionSet)
              onMotionSet (param,
                           steppedMotionValue (
                               param, _motionShown[static_cast<size_t> (param)].value,
                               increment));
          };
          field->onDragIncrement = [step] (int, int, int increment) {
            step (increment);
          };
          if (param != MotionParam::Speed)
            field->onTap = [step] (int, int) { step (1); };
          field->onDoubleTap = [this, param] (int, int) {
            if (onMotionUnset)
              onMotionUnset (param);
          };
          addChildComponent (*field);
          _motionField[at] = std::move (field);
          continue;
        }

      auto const spec = motionTileKnobSpec (param);
      auto knob = std::make_unique<PotKnob> ();
      knob->setLabel (spec.label);
      knob->setRange (spec.min, spec.max, spec.interval);
      knob->setFillsFromTheMiddle (spec.bipolar);
      knob->setWraps (spec.wraps);

      knob->onValueChange = [this, param, k = knob.get ()] {
        if (!onMotionSet)
          return;
        auto const &top = _motionShown[static_cast<size_t> (MotionParam::ClipTop)];
        auto const &bottom
            = _motionShown[static_cast<size_t> (MotionParam::ClipBottom)];
        onMotionSet (param, motionValueForKnob (param, k->getValue (),
                                                top.value, bottom.value));
      };
      // Two taps are not the scale's middle here: they give the value back to
      // the script, so the knob's own return value stays off.
      knob->onDoubleTapped = [this, param] {
        if (onMotionUnset)
          onMotionUnset (param);
      };

      addChildComponent (*knob);
      _motionKnob[at] = std::move (knob);
    }
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
  if (_afterTouch)
    _afterTouch->setBounds (_layout.afterKey);
  if (_audioKeyTouch)
    _audioKeyTouch->setBounds (_layout.audioKey);
  if (_motionKeyTouch)
    _motionKeyTouch->setBounds (_layout.motionKey);

  for (auto const param : motionParamOrder)
    {
      auto const at = static_cast<size_t> (param);
      auto const cell = _layout.motionControls[at];
      if (_motionKnob[at])
        _motionKnob[at]->setBounds (cell);
      if (_motionField[at])
        _motionField[at]->setBounds (cell);
    }
}

void
ActionComponent::putColourOnKnobs ()
{
  for (auto &knob : _knob)
    if (knob)
      knob->setKnobColour (_channelColour);
  for (auto &knob : _motionKnob)
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

  // The highlight just assigned, or another button's script: the walk starts
  // again from what the button holds. Not on every call -- the page is told
  // the name on every update, and the walk would never get past one row.
  _listCursor = -1;
  _actionName = name;
  // The chosen one in view, moved as little as possible: a list that jumps
  // to the top on every change makes you scroll back to where you were.
  _listTop = a3::scrollToShow (_listTop, _choices.indexOf (_actionName),
                               actionListVisibleRows (_layout),
                               _choices.size ());
  repaint ();
}

juce::String
ActionComponent::moveListCursor (int increment)
{
  if (_choices.isEmpty ())
    return {};

  auto const from = _listCursor >= 0
                        ? _listCursor
                        : juce::jmax (0, _choices.indexOf (_actionName));
  _listCursor = juce::jlimit (0, _choices.size () - 1, from + increment);
  _listTop = a3::scrollToShow (_listTop, _listCursor,
                               actionListVisibleRows (_layout),
                               _choices.size ());
  repaint ();
  return _choices[_listCursor];
}

void
ActionComponent::chooseListCursor ()
{
  if (!juce::isPositiveAndBelow (_listCursor, _choices.size ())
      || !onActionChosen)
    return;
  onActionChosen (_choices[_listCursor]);
}

ActionKey
ActionComponent::moveKeyRing (int increment)
{
  constexpr int numKeys = 3;
  auto const at = juce::jlimit (0, numKeys - 1,
                                static_cast<int> (_keyRing) + increment);
  _keyRing = static_cast<ActionKey> (at);
  _keyRingShown = true;
  repaint ();
  return _keyRing;
}

void
ActionComponent::pressKeyRing ()
{
  // The tap's own callbacks, so a press and a finger cannot mean two things.
  switch (_keyRing)
    {
    case ActionKey::Edit:
      if (onEditPressed)
        onEditPressed ();
      return;
    case ActionKey::Mode:
      if (onControlTapped)
        onControlTapped (ActMode);
      return;
    case ActionKey::After:
      if (onAfterStepped)
        onAfterStepped (1);
      return;
    }
}

void
ActionComponent::switchTile ()
{
  setTile (_tile == ActionTile::Audio ? ActionTile::Motion
                                      : ActionTile::Audio);
}

int
ActionComponent::valueRowsOfTile () const
{
  return _tile == ActionTile::Audio ? ActionLayout::numRows
                                    : ActionLayout::motionRows;
}

int
ActionComponent::markedValueRow () const
{
  return _valueRow;
}

void
ActionComponent::stepValueRow ()
{
  _valueRow = (_valueRow + 1) % valueRowsOfTile ();
  _valueRowShown = true;
  repaint ();
}

void
ActionComponent::turnMarkedValue (int column, int increment)
{
  _valueRowShown = true;
  repaint ();
  switch (_tile)
    {
    case ActionTile::Audio: turnAudioValue (column, increment); return;
    case ActionTile::Motion: turnMotionValue (column, increment); return;
    }
}

void
ActionComponent::turnAudioValue (int column, int increment)
{
  // Attack, decay, ceiling across; the fourth encoder has no column here.
  if (!juce::isPositiveAndBelow (column, 3) || !onControlDragged)
    return;
  onControlDragged (_valueRow * 3 + column, increment);
}

void
ActionComponent::turnMotionValue (int column, int increment)
{
  auto const at = _valueRow * ActionLayout::motionColumns + column;
  if (!_motionHasAction || !juce::isPositiveAndBelow (column, 4)
      || !juce::isPositiveAndBelow (at, numMotionParams) || !onMotionSet)
    return;

  auto const param = motionParamOrder[static_cast<size_t> (at)];
  auto const &shown = _motionShown[static_cast<size_t> (param)];

  if (motionParamIsAField (param))
    {
      onMotionSet (param, steppedMotionValue (param, shown.value, increment));
      return;
    }

  // A knob, stepped on its own scale: whole steps where it counts them,
  // otherwise the fiftieth the panel's encoders take everywhere else.
  auto const spec = motionTileKnobSpec (param);
  auto const step
      = spec.interval > 0.0 ? spec.interval : (spec.max - spec.min) / 50.0;
  auto knob = motionKnobValue (param, shown.value) + increment * step;
  if (spec.wraps)
    {
      auto const span = spec.max - spec.min;
      knob = spec.min + std::fmod (std::fmod (knob - spec.min, span) + span, span);
    }
  else
    {
      knob = juce::jlimit (spec.min, spec.max, knob);
    }

  auto const &top = _motionShown[static_cast<size_t> (MotionParam::ClipTop)];
  auto const &bottom
      = _motionShown[static_cast<size_t> (MotionParam::ClipBottom)];
  onMotionSet (param,
               motionValueForKnob (param, knob, top.value, bottom.value));
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

  if (chosen != _chosenButton)
    _listCursor = -1;
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
ActionComponent::setTile (ActionTile tile)
{
  if (tile == _tile)
    return;

  _tile = tile;
  // The other tile's rows are other rows: the lower encoders start again at
  // its top.
  _valueRow = 0;
  showTheTile ();
  repaint ();
}

void
ActionComponent::showTheTile ()
{
  auto const audio = _tile == ActionTile::Audio;
  for (int i = 0; i < ActMode; ++i)
    _knob[static_cast<size_t> (i)]->setVisible (audio);

  auto const motion = _tile == ActionTile::Motion && _motionHasAction;
  for (auto &knob : _motionKnob)
    if (knob)
      knob->setVisible (motion);
  for (auto &field : _motionField)
    if (field)
      field->setVisible (motion);
}

void
ActionComponent::setMotionTile (
    std::array<MotionShown, numMotionParams> const &shown, bool hasAction,
    float patternLengthBeats)
{
  _motionShown = shown;
  _motionHasAction = hasAction;
  _patternLengthBeats = patternLengthBeats;

  for (auto const param : motionParamOrder)
    {
      auto const &at = _motionShown[static_cast<size_t> (param)];
      auto &knob = _motionKnob[static_cast<size_t> (param)];
      if (!knob)
        continue;

      // Grey where the button leaves the value alone -- the clip's own is
      // what it shows then -- the channel's colour where the script sets it,
      // and on a wash where it was turned here and is the button's own.
      knob->setSelected (at.source != MotionSource::Clip);
      knob->setActive (at.source == MotionSource::Button);
      // Not while a finger is on it: the page would argue with the hand.
      if (!knob->isMouseButtonDown ())
        knob->setValue (motionKnobValue (param, at.value),
                        juce::dontSendNotification);
    }

  showTheTile ();
  repaint ();
}

void
ActionComponent::setAfter (std::optional<int> after)
{
  if (after == _after)
    return;

  _after = after;
  repaint ();
}

void
ActionComponent::setActionButtonModes (std::array<bool, 6> const &holds)
{
  if (holds == _buttonHolds)
    return;

  _buttonHolds = holds;
  repaint ();
}

juce::Component *
ActionComponent::audioKnob (int control)
{
  if (control < 0 || control >= ActMode)
    return nullptr;
  return _knob[static_cast<size_t> (control)].get ();
}

juce::Component *
ActionComponent::motionControl (MotionParam param)
{
  auto const at = static_cast<size_t> (param);
  if (_motionKnob[at])
    return _motionKnob[at].get ();
  return _motionField[at].get ();
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
      auto const parts = actionFieldParts (bounds);
      auto const numberRow = parts.number;
      auto const inner = parts.name;

      g.setColour (ink);
      g.setFont (actionFieldNumberFont (numberRow));
      g.drawText ("A" + juce::String (button + 1), numberRow,
                  juce::Justification::centredLeft);

      // What an action's name in its field may cost.
      constexpr float buttonNameCap = 14.f;
      g.setFont (juce::Font (juce::FontOptions (
          fittedFontHeight (inner.getHeight () * 0.7f, buttonNameCap))));
      // Squeezed a little before it is cut: "Unwind" should read as Unwind.
      g.drawFittedText (named ? name : juce::String ("--"), inner,
                        juce::Justification::centredLeft, 1, 0.75f);

      // How this button plays, its own mode rather than the chosen one's:
      // 1 for a one-shot, H for a hold. Only on a button that plays at all.
      if (named)
        {
          auto const badge = parts.modeBadge.toFloat ();
          g.drawRoundedRectangle (badge.reduced (theme ().strokeThin * 0.5f),
                                  theme ().radiusControl, theme ().strokeThin);
          g.setFont (juce::Font (juce::FontOptions (
              badge.getHeight () * 0.75f, juce::Font::bold)));
          g.drawText (_buttonHolds[button] ? "H" : "1", parts.modeBadge,
                      juce::Justification::centred);
        }
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
ActionComponent::paintTileKeys (juce::Graphics &g)
{
  auto const ink = readableInk (_channelColour, toColour (theme ().background),
                                toColour (theme ().textPrimary));

  auto const paintKey = [&] (juce::Rectangle<int> at, char const *word,
                             bool lit) {
    if (at.isEmpty ())
      return;

    // Exactly one is lit: the tile standing in the card, in the channel's
    // colour and with the chosen field's thick outline. The other rests like
    // EDIT does.
    g.setColour (lit ? _channelColour.withAlpha (theme ().alphaFillEmphasis)
                     : toColour (theme ().textPrimary, theme ().alphaFill));
    g.fillRoundedRectangle (at.toFloat (), theme ().radiusControl);
    g.setColour (lit ? _channelColour
                     : toColour (theme ().textPrimary, theme ().alphaOutline));
    g.drawRoundedRectangle (at.toFloat (), theme ().radiusControl,
                            lit ? theme ().strokeThick : theme ().strokeThin);

    g.setColour (lit ? ink
                     : toColour (theme ().textMuted, theme ().alphaTextStrong));
    // What "AUDIO" and "MOTION" may cost.
    constexpr float tileKeyCap = 18.f;
    g.setFont (juce::Font (juce::FontOptions (
        fittedFontHeight (at.getHeight () * 0.4f, tileKeyCap))));
    g.drawFittedText (word, at.reduced (at.getHeight () / 8, 0),
                      juce::Justification::centred, 1, 0.7f);
  };

  paintKey (_layout.audioKey, "AUDIO", _tile == ActionTile::Audio);
  paintKey (_layout.motionKey, "MOTION", _tile == ActionTile::Motion);
}

void
ActionComponent::paintAfterKey (juce::Graphics &g)
{
  auto const at = _layout.afterKey;
  if (at.isEmpty ())
    return;

  // Lit like the mode beside it when something follows, resting like EDIT
  // when nothing does -- a chain is worth seeing at a glance.
  auto const set = _after.has_value ();
  g.setColour (set ? _channelColour.withAlpha (theme ().alphaFillEmphasis)
                   : toColour (theme ().textPrimary, theme ().alphaFill));
  g.fillRoundedRectangle (at.toFloat (), theme ().radiusControl);
  g.setColour (set ? _channelColour.withAlpha (theme ().alphaInactive)
                   : toColour (theme ().textPrimary, theme ().alphaOutline));
  g.drawRoundedRectangle (at.toFloat (), theme ().radiusControl,
                          theme ().strokeThin);

  g.setColour (set ? readableInk (_channelColour,
                                  toColour (theme ().background),
                                  toColour (theme ().textPrimary))
                   : toColour (theme ().textMuted, theme ().alphaTextStrong));
  // What the after key's words ("then A3") may cost.
  constexpr float afterKeyCap = 18.f;
  g.setFont (juce::Font (juce::FontOptions (
      fittedFontHeight (at.getHeight () * 0.4f, afterKeyCap))));
  g.drawFittedText ("then " + afterName (_after),
                    at.reduced (at.getHeight () / 8, 0),
                    juce::Justification::centred, 1, 0.7f);
}

void
ActionComponent::paintAudioRowNames (juce::Graphics &g)
{
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
}

void
ActionComponent::paintMotionField (juce::Graphics &g, MotionParam param)
{
  auto const at = _layout.motionControls[static_cast<size_t> (param)];
  if (at.isEmpty ())
    return;

  auto const &shown = _motionShown[static_cast<size_t> (param)];
  auto const set = shown.source != MotionSource::Clip;
  auto const own = shown.source == MotionSource::Button;

  // The knobs' three looks, said as a key: grey where the button leaves the
  // clip's value alone, the channel's colour where the script sets it, a
  // stronger wash where it was turned here.
  g.setColour (own   ? _channelColour.withAlpha (theme ().alphaInactive)
               : set ? _channelColour.withAlpha (theme ().alphaFillEmphasis)
                     : toColour (theme ().textPrimary, theme ().alphaFill));
  g.fillRoundedRectangle (at.toFloat (), theme ().radiusControl);
  g.setColour (set ? _channelColour
                   : toColour (theme ().textPrimary, theme ().alphaOutline));
  g.drawRoundedRectangle (at.toFloat (), theme ().radiusControl,
                          theme ().strokeThin);

  auto const whole = static_cast<int> (std::lround (shown.value));
  auto const word
      = param == MotionParam::Speed
            ? speedKeyName (whole, _patternLengthBeats)
        : param == MotionParam::Direction
            ? juce::String (value::directionNames[juce::jlimit (
                  0, value::numDirections - 1, whole)])
            : juce::String (value::endActionNames[juce::jlimit (
                  0, value::numEndActions - 1, whole)]);

  auto inner = at.reduced (juce::roundToInt (theme ().paddingTight));
  auto const captionRow = inner.removeFromBottom (inner.getHeight () / 3);

  g.setColour (set ? readableInk (_channelColour,
                                  toColour (theme ().background),
                                  toColour (theme ().textPrimary))
                   : toColour (theme ().textMuted, theme ().alphaMuted));
  // What a field's value ("8", "Rev", "Loop") may cost.
  constexpr float motionFieldValueCap = 16.f;
  g.setFont (juce::Font (juce::FontOptions (
      fittedFontHeight (inner.getHeight () * 0.6f, motionFieldValueCap))));
  g.drawFittedText (word, inner, juce::Justification::centred, 1, 0.75f);

  g.setColour (toColour (theme ().textMuted, theme ().alphaTextStrong));
  g.setFont (juce::Font (juce::FontOptions (_layout.metrics.captionSize)));
  g.drawFittedText (motionParamCaption (param), captionRow,
                    juce::Justification::centred, 1, 0.75f);
}

void
ActionComponent::paintMotionTile (juce::Graphics &g)
{
  if (!_motionHasAction)
    {
      // Nothing to put on a clip: said in the card, not as nineteen knobs
      // that would change nothing.
      auto area = _layout.card.withTrimmedTop (
          _layout.cardCaption.getBottom () - _layout.card.getY ());
      g.setColour (toColour (theme ().textMuted, theme ().alphaTextStrong));
      // What the "no action" note in the card may cost.
      constexpr float noActionCap = 16.f;
      g.setFont (juce::Font (juce::FontOptions (fittedFontHeight (
          _layout.cardCaption.getHeight () * 0.7f, noActionCap))));
      g.drawFittedText ("A" + juce::String (_chosenButton + 1)
                            + " has no action\npick one from the list",
                        area.reduced (area.getWidth () / 10),
                        juce::Justification::centred, 3, 0.8f);
      return;
    }

  for (auto const param : motionParamOrder)
    if (motionParamIsAField (param))
      paintMotionField (g, param);
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

  auto const rows = actionListRows (_layout);
  for (int row = 0; row < static_cast<int> (rows.size ()); ++row)
    {
      auto const index = _listTop + row;
      if (index >= _choices.size ())
        break;

      auto const at = rows[static_cast<size_t> (row)];
      auto const name = _choices[index];
      auto const chosen = name == _actionName;
      auto const highlighted = index == _listCursor && !chosen;

      if (chosen)
        {
          g.setColour (_channelColour.withAlpha (theme ().alphaDisabled));
          g.fillRect (at.reduced (juce::roundToInt (theme ().paddingTight),
                                  juce::roundToInt (theme ().paddingHair)));
        }

      // Where the encoder stands: outlined, not filled -- filled is what the
      // button holds, and the two must not be mistaken mid-set.
      if (highlighted)
        {
          g.setColour (_channelColour);
          g.drawRect (at.reduced (juce::roundToInt (theme ().paddingTight),
                                  juce::roundToInt (theme ().paddingHair)),
                      juce::roundToInt (theme ().strokeThin));
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

  switch (_tile)
    {
    case ActionTile::Audio: paintAudioRowNames (g); break;
    case ActionTile::Motion: paintMotionTile (g); break;
    }
  paintTileKeys (g);
  paintAfterKey (g);

  // The chosen button's action is running: the card says so in the white
  // its field and its pad wear, so what the page shows reads as live.
  if (_runningButton >= 0 && _runningButton == _chosenButton)
    {
      g.setColour (toColour (theme ().textPrimary));
      g.drawRoundedRectangle (_layout.card.toFloat (), theme ().radiusCard,
                              theme ().strokeThick);
    }

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


  paintEncoderMarks (g);

  // Last, so it covers what it opens over.
  paintActionList (g);
}

void
ActionComponent::paintEncoderMarks (juce::Graphics &g)
{
  // Where the panel's encoders stand on this page: a ring on the key the
  // third one would press, an outline round the row the lower four turn.
  // Outlines in the channel's colour, the list's highlight's language.
  g.setColour (_channelColour);
  auto const stroke = theme ().strokeThick;

  if (_keyRingShown)
    {
      auto const key = _keyRing == ActionKey::Edit   ? _layout.editButton
                       : _keyRing == ActionKey::Mode ? _layout.actModeField
                                                     : _layout.afterKey;
      g.drawRoundedRectangle (key.toFloat ().expanded (stroke),
                              theme ().radiusControl, stroke);
    }

  if (!_valueRowShown)
    return;

  juce::Rectangle<int> row;
  if (_tile == ActionTile::Audio)
    row = _layout.rows[static_cast<size_t> (
        juce::jlimit (0, ActionLayout::numRows - 1, _valueRow))];
  else
    for (int column = 0; column < ActionLayout::motionColumns; ++column)
      {
        auto const at = _valueRow * ActionLayout::motionColumns + column;
        if (!juce::isPositiveAndBelow (at, numMotionParams))
          break;
        auto const cell = _layout.motionControls[static_cast<size_t> (
            motionParamOrder[static_cast<size_t> (at)])];
        row = row.isEmpty () ? cell : row.getUnion (cell);
      }

  if (!row.isEmpty ())
    g.drawRoundedRectangle (row.toFloat (), theme ().radiusControl,
                            theme ().strokeThin);
}

}
