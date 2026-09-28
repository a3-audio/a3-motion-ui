/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include "BarKeyboardComponent.hh"

#include <a3-motion-ui/components/BarButton.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
/** Held, Backspace and the cursor start repeating after this long, then go
 *  on at the second rate -- a desktop keyboard's feel, slowed a little for a
 *  finger that is also watching the music. */
constexpr int repeatDelayMs = 450;
constexpr int repeatIntervalMs = 70;

/** The cursor keys' arrow, as a share of the key's shorter side. */
constexpr float arrowShareOfKey = 0.28f;
}

BarKeyboardComponent::BarKeyboardComponent ()
{
  // The field being typed into keeps the focus: the rename row and the
  // script editor end their edit when they lose it.
  setWantsKeyboardFocus (false);
  setMouseClickGrabsKeyboardFocus (false);
  setOpaque (true);
}

void
BarKeyboardComponent::setFields (
    std::array<juce::Rectangle<int>, 8> const &fields, ControlMetrics metrics)
{
  _fields = fields;
  _metrics = metrics;
  relayout ();
}

void
BarKeyboardComponent::applyTheme ()
{
  if (onPlacementStale)
    onPlacementStale ();
}

void
BarKeyboardComponent::relayout ()
{
  _keys = layOutBarKeyboard (_fields, _state.page);
  _pressed.clear ();
  _repeatingKey = -1;
  stopTimer ();
  repaint ();
}

int
BarKeyboardComponent::panelKeyOfBlock (int block) const
{
  auto const keys = keysOfBlock (_keys, block);
  if (keys.empty ())
    return -1;
  auto const at = juce::jlimit (0, static_cast<int> (keys.size ()) - 1,
                                _panelKey[static_cast<size_t> (block)]);
  return static_cast<int> (keys[static_cast<size_t> (at)]);
}

void
BarKeyboardComponent::turnEncoder (int column, int row, int increment)
{
  if (column < 0 || column > 3 || row < 0 || row > 1 || increment == 0)
    return;

  auto const block = static_cast<size_t> (row * 4 + column);
  auto const count = static_cast<int> (keysOfBlock (_keys, row * 4 + column).size ());

  // The first detent shows where the encoder stands rather than moving it
  // from a place nobody could see: right lands on the field's first key,
  // left on its last.
  if (!_panelShown[block])
    {
      _panelShown[block] = true;
      _panelKey[block] = increment > 0 ? 0 : count - 1;
    }
  else
    _panelKey[block] = steppedKeyInBlock (count, _panelKey[block], increment);

  repaint ();
}

void
BarKeyboardComponent::pressEncoder (int column, int row)
{
  if (column < 0 || column > 3 || row < 0 || row > 1)
    return;

  auto const block = row * 4 + column;
  if (!_panelShown[static_cast<size_t> (block)])
    return;

  auto const key = panelKeyOfBlock (block);
  if (key >= 0)
    fire (key);
}

void
BarKeyboardComponent::fire (int keyIndex)
{
  if (keyIndex < 0 || keyIndex >= static_cast<int> (_keys.size ()))
    return;

  auto const def = _keys[static_cast<size_t> (keyIndex)].def;
  auto const pageBefore = _state.page;
  auto const shiftHeld = isShiftHeld && isShiftHeld ();
  auto const outcome = pressKey (_state, def, shiftHeld);

  if (_state.page != pageBefore)
    relayout ();
  else
    repaint ();

  if (outcome.key.has_value () && onKey)
    onKey (*outcome.key);
  if (outcome.hide && onHide)
    onHide ();
}

void
BarKeyboardComponent::mouseDown (juce::MouseEvent const &e)
{
  auto const key = keyAt (_keys, e.getPosition ());
  if (key < 0)
    return;

  auto const source = e.source.getIndex ();
  _pressed[source] = key;

  // Backspace and the cursor act on touch and go on while held; every other
  // key types on release, so a finger that lands on the wrong key can slide
  // off it.
  if (keyRepeatsWhileHeld (_keys[static_cast<size_t> (key)].def.action))
    {
      _repeatingKey = key;
      _repeatingSource = source;
      fire (key);
      startTimer (repeatDelayMs);
    }

  repaint ();
}

void
BarKeyboardComponent::mouseDrag (juce::MouseEvent const &e)
{
  auto const source = e.source.getIndex ();
  auto const found = _pressed.find (source);
  if (found == _pressed.end () || found->second < 0)
    return;

  if (keyAt (_keys, e.getPosition ()) == found->second)
    return;

  // Slid off: the key is let go without typing.
  found->second = -1;
  if (source == _repeatingSource)
    {
      _repeatingKey = -1;
      stopTimer ();
    }
  repaint ();
}

void
BarKeyboardComponent::mouseUp (juce::MouseEvent const &e)
{
  auto const source = e.source.getIndex ();
  auto const found = _pressed.find (source);
  if (found == _pressed.end ())
    return;

  auto const key = found->second;
  _pressed.erase (found);

  if (source == _repeatingSource)
    {
      _repeatingKey = -1;
      _repeatingSource = -1;
      stopTimer ();
      repaint ();
      return;
    }

  if (key >= 0 && keyAt (_keys, e.getPosition ()) == key)
    fire (key);
  repaint ();
}

void
BarKeyboardComponent::timerCallback ()
{
  if (_repeatingKey < 0)
    {
      stopTimer ();
      return;
    }

  fire (_repeatingKey);
  startTimer (repeatIntervalMs);
}

void
BarKeyboardComponent::visibilityChanged ()
{
  if (isVisible ())
    return;

  // Put away, it forgets what hands were doing on it; the page stays, so a
  // keyboard brought back for a number comes back on the digits.
  _pressed.clear ();
  _repeatingKey = -1;
  _repeatingSource = -1;
  stopTimer ();
  _state.shift = ShiftState::Off;
  _panelShown.fill (false);
}

bool
BarKeyboardComponent::keyIsLit (int keyIndex) const
{
  for (auto const &[source, key] : _pressed)
    if (key == keyIndex)
      return true;

  auto const &def = _keys[static_cast<size_t> (keyIndex)].def;
  return def.action == KeyAction::Shift && _state.shift != ShiftState::Off;
}

void
BarKeyboardComponent::paintKey (juce::Graphics &g, int keyIndex) const
{
  auto const &key = _keys[static_cast<size_t> (keyIndex)];
  auto const upper = _state.shift != ShiftState::Off;
  auto label = keyLabel (key.def, _state.page, upper);
  if (key.def.action == KeyAction::Shift && _state.shift == ShiftState::Locked)
    label = "CAPS";

  auto const lit = keyIsLit (keyIndex);
  paintBarButton (g, key.bounds, _metrics, {}, label, {}, lit, false);

  if (key.def.action == KeyAction::Left || key.def.action == KeyAction::Right)
    {
      auto const centre = key.bounds.getCentre ().toFloat ();
      auto const size = static_cast<float> (juce::jmin (
                            key.bounds.getWidth (), key.bounds.getHeight ()))
                        * arrowShareOfKey;
      auto const tip = key.def.action == KeyAction::Left ? -size : size;
      juce::Path arrow;
      arrow.addTriangle (centre.x + tip, centre.y, centre.x - tip,
                         centre.y - size, centre.x - tip, centre.y + size);
      g.setColour (Colours::barText (false));
      g.fillPath (arrow);
    }
}

void
BarKeyboardComponent::paint (juce::Graphics &g)
{
  // The bar's own ground, laid the way the bar lays it.
  g.fillAll (toColour (theme ().background));
  g.fillAll (toColour (theme ().surface, theme ().panelOpacity));

  for (int i = 0; i < static_cast<int> (_keys.size ()); ++i)
    paintKey (g, i);

  // Where each turned encoder stands: a ring, not a fill, so it never reads
  // as a key held down.
  g.setColour (toColour (theme ().textPrimary, theme ().alphaSecondary));
  for (int block = 0; block < 8; ++block)
    {
      if (!_panelShown[static_cast<size_t> (block)])
        continue;
      auto const key = panelKeyOfBlock (block);
      if (key < 0)
        continue;
      g.drawRoundedRectangle (_keys[static_cast<size_t> (key)].bounds.toFloat (),
                              theme ().radiusControl, theme ().strokeThick);
    }
}

}
