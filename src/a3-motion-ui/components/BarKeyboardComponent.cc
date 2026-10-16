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
#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/FittedFont.hh>
#include <a3-motion-ui/components/SourceKeys.hh>
#include <a3-motion-ui/components/KeyboardEncoders.hh>
#include <a3-motion-ui/components/KnobGlyph.hh>
#include <a3-motion-ui/components/PanelKeyboard.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

#include <algorithm>
#include <utility>

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

/** The air round a knob's sign, as a share of its cell's width. */
constexpr float glyphInsetShare = 0.22f;

/** How much of a file key's width its word may take. */
constexpr float fileKeyTextShare = 0.85f;

constexpr int numEncoderColumns = 4;
constexpr int numEncoderRows = 2;

/** What encoder `index` (column-major) does while the keyboard is up and
 *  SHIFT is not held -- the knobs draw that. */
KeyboardEncoderJob
jobOfKnob (size_t index)
{
  auto const i = static_cast<int> (index);
  return keyboardEncoderJob (true, i / numEncoderRows, i % numEncoderRows,
                             false);
}

size_t
knobIndex (int column, int row)
{
  return static_cast<size_t> (column * numEncoderRows + row);
}
}

BarKeyboardComponent::BarKeyboardComponent ()
{
  // The field being typed into keeps the focus: the rename row and the
  // script editor end their edit when they lose it.
  setWantsKeyboardFocus (false);
  setMouseClickGrabsKeyboardFocus (false);
  setOpaque (true);

  // The encoders' knobs only show: at 5 mm they are below a fingertip, and
  // a touch meant for a key must never turn one. The script is scrolled by
  // dragging it.
  for (size_t i = 0; i < _encoderKnobs.size (); ++i)
    {
      auto &knob = _encoderKnobs[i];
      knob.setInterceptsMouseClicks (false, false);
      // No caption, a cell of its own: as large as the cell, so the knob
      // grows and shrinks with the keys around it.
      knob.setFillsItsCell (true);
      knob.setWantsKeyboardFocus (false);
      knob.setMouseClickGrabsKeyboardFocus (false);
      switch (jobOfKnob (i))
        {
        case KeyboardEncoderJob::CursorVertical:
        case KeyboardEncoderJob::CursorHorizontal:
          // A cursor has no position on a scale: a ring with its tick up.
          knob.setWraps (true);
          break;
        case KeyboardEncoderJob::Folder:
        case KeyboardEncoderJob::File:
          knob.setEnabled (false);
          break;
        case KeyboardEncoderJob::PageControl:
        case KeyboardEncoderJob::ChannelPot:
        case KeyboardEncoderJob::Nothing:
          // Nothing to show: the tick up, no arc, dimmed for good.
          knob.setFillsFromTheMiddle (true);
          knob.setValue (0.5, juce::dontSendNotification);
          knob.setEnabled (false);
          break;
        }
      addAndMakeVisible (knob);
    }
  dressEncoderKnobs ();
}

void
BarKeyboardComponent::dressEncoderKnobs ()
{
  // The keyboard belongs to no channel: neutral arcs, the light tick.
  for (auto &knob : _encoderKnobs)
    knob.setKnobColour (toColour (theme ().textMuted));
}

void
BarKeyboardComponent::showFiles (KeyboardFilesView const &view)
{
  for (size_t i = 0; i < _encoderKnobs.size (); ++i)
    {
      auto &knob = _encoderKnobs[i];
      auto const job = jobOfKnob (i);
      if (job != KeyboardEncoderJob::Folder && job != KeyboardEncoderJob::File)
        continue;

      knob.setValue (job == KeyboardEncoderJob::Folder ? view.folderFraction
                                                       : view.fileFraction,
                     juce::dontSendNotification);
      knob.setEnabled (view.filesOnScreen);
    }

  auto const changed
      = view.filesOnScreen != _files.filesOnScreen
        || !std::equal (view.keys.begin (), view.keys.end (),
                        _files.keys.begin (),
                        [] (FileKeyLook const &a, FileKeyLook const &b) {
                          return a.label == b.label && a.enabled == b.enabled
                                 && a.lit == b.lit;
                        });
  _files = view;
  // Nothing moved, nothing repainted: this is asked on every UI tick.
  if (changed)
    repaint ();
}

bool
BarKeyboardComponent::fileKeyActs (int row) const
{
  return _files.filesOnScreen
         && _files.keys[static_cast<size_t> (fileKeyOfRow (row))].enabled;
}

int
BarKeyboardComponent::fileKeyRowAt (juce::Point<int> at) const
{
  for (int row = 0; row < numFileKeys; ++row)
    if (keyboardFileKeySlot (getLocalBounds (), row).contains (at))
      return row;
  return -1;
}

PotKnob const &
BarKeyboardComponent::encoderKnob (int column, int row) const
{
  return _encoderKnobs[knobIndex (column, row)];
}

void
BarKeyboardComponent::setMetrics (ControlMetrics metrics)
{
  _metrics = metrics;
  relayout ();
}

void
BarKeyboardComponent::resized ()
{
  relayout ();
}

void
BarKeyboardComponent::applyTheme ()
{
  dressEncoderKnobs ();
  if (onPlacementStale)
    onPlacementStale ();
}

void
BarKeyboardComponent::relayout ()
{
  _keys = layOutPanelKeyboard (getLocalBounds (), _state.page);
  for (int column = 0; column < numEncoderColumns; ++column)
    for (int row = 0; row < numEncoderRows; ++row)
      _encoderKnobs[knobIndex (column, row)].setBounds (
          keyboardEncoderSlot (getLocalBounds (), column, row).knob);
  _pressed.clear ();
  stopRepeating ();
  repaint ();
}

void
BarKeyboardComponent::fire (KeyDef const &def)
{
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
BarKeyboardComponent::startRepeating (KeyDef const &def)
{
  _repeating = def;
  startTimer (repeatDelayMs);
}

void
BarKeyboardComponent::stopRepeating ()
{
  _repeating.reset ();
  _repeatingSource = -1;
  _repeatingCell.reset ();
  stopTimer ();
}

void
BarKeyboardComponent::pressPanelCell (PanelCell cell, bool down)
{
  auto const key = std::make_pair (cell.row, cell.col);
  auto const def = panelKeyAt (_state.page, cell);

  if (!down)
    {
      _panelHeld.erase (key);
      if (_repeatingCell && *_repeatingCell == cell)
        stopRepeating ();
      repaint ();
      return;
    }

  if (def.action == KeyAction::None)
    return;

  _panelHeld.insert (key);
  fire (def);
  if (keyRepeatsWhileHeld (def.action))
    {
      startRepeating (def);
      _repeatingCell = cell;
    }
}

void
BarKeyboardComponent::mouseDown (juce::MouseEvent const &e)
{
  auto const key = keyAt (_keys, e.getPosition ());
  if (key < 0)
    {
      pressFileKey (e);
      return;
    }

  auto const source = e.source.getIndex ();
  _pressed[source] = key;

  // Backspace and the cursor act on touch and go on while held; every other
  // key types on release, so a finger that lands on the wrong key can slide
  // off it.
  auto const def = _keys[static_cast<size_t> (key)].def;
  if (keyRepeatsWhileHeld (def.action))
    {
      fire (def);
      startRepeating (def);
      _repeatingSource = source;
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
    stopRepeating ();
  repaint ();
}

void
BarKeyboardComponent::pressFileKey (juce::MouseEvent const &e)
{
  auto const row = fileKeyRowAt (e.getPosition ());
  if (row < 0)
    return;

  // One finger arrives twice on the device, as a touch and as X's mouse
  // (#64): the first leads, so DELETE is not pressed twice and confirmed.
  _fileGesture.forgetIfNotDown (isSourceDown);
  if (!_fileGesture.press (sourceKeyOf (e.source)))
    return;
  _fileKeyDown = row;
  repaint (keyboardFileKeySlot (getLocalBounds (), row));
}

bool
BarKeyboardComponent::releaseFileKey (juce::MouseEvent const &e)
{
  if (_fileKeyDown < 0)
    return false;
  if (!_fileGesture.release (sourceKeyOf (e.source)))
    return true;

  auto const row = std::exchange (_fileKeyDown, -1);
  repaint (keyboardFileKeySlot (getLocalBounds (), row));
  // On release, like the keys: a finger that landed on the wrong one can
  // slide off it.
  if (fileKeyRowAt (e.getPosition ()) == row && fileKeyActs (row)
      && onFileKey)
    onFileKey (fileKeyOfRow (row));
  return true;
}

void
BarKeyboardComponent::mouseUp (juce::MouseEvent const &e)
{
  if (releaseFileKey (e))
    return;

  auto const source = e.source.getIndex ();
  auto const found = _pressed.find (source);
  if (found == _pressed.end ())
    return;

  auto const key = found->second;
  _pressed.erase (found);

  if (source == _repeatingSource)
    {
      stopRepeating ();
      repaint ();
      return;
    }

  if (key >= 0 && keyAt (_keys, e.getPosition ()) == key)
    fire (_keys[static_cast<size_t> (key)].def);
  repaint ();
}

void
BarKeyboardComponent::timerCallback ()
{
  if (!_repeating)
    {
      stopTimer ();
      return;
    }

  fire (*_repeating);
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
  _panelHeld.clear ();
  _fileKeyDown = -1;
  stopRepeating ();
  _state.shift = ShiftState::Off;
}

bool
BarKeyboardComponent::keyIsLit (int keyIndex) const
{
  for (auto const &[source, key] : _pressed)
    if (key == keyIndex)
      return true;

  auto const &cap = _keys[static_cast<size_t> (keyIndex)];
  for (auto const &[row, col] : _panelHeld)
    if (cap.bounds.contains (panelCellBounds (getLocalBounds (), { row, col })
                                 .getCentre ()))
      return true;

  return cap.def.action == KeyAction::Shift && _state.shift != ShiftState::Off;
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

  for (int column = 0; column < numEncoderColumns; ++column)
    for (int row = 0; row < numEncoderRows; ++row)
      paintEncoderGlyph (g, column, row);

  paintFileKeys (g);
}

void
BarKeyboardComponent::paintFileKeys (juce::Graphics &g) const
{
  // One size for all six, the one the longest word fits at -- a column of
  // keys in six sizes reads as six kinds of key.
  auto metrics = _metrics;
  auto const width = static_cast<float> (
      keyboardFileKeySlot (getLocalBounds (), 0).getWidth ());
  juce::Font const font{ juce::FontOptions (metrics.valueSize) };
  for (auto const &key : _files.keys)
    if (key.label.isNotEmpty ())
      metrics.valueSize = juce::jmin (
          metrics.valueSize,
          heightToFitWidth (font, key.label, width * fileKeyTextShare));

  for (int row = 0; row < numFileKeys; ++row)
    {
      auto const &key = _files.keys[static_cast<size_t> (fileKeyOfRow (row))];
      if (key.label.isEmpty ())
        continue;

      auto const bounds = keyboardFileKeySlot (getLocalBounds (), row);
      // Dimmed the way every control that cannot act is.
      auto const acts = fileKeyActs (row);
      if (!acts)
        g.beginTransparencyLayer (theme ().alphaDisabled);
      paintBarButton (g, bounds, metrics, {}, key.label, {},
                      _fileKeyDown == row || (acts && key.lit), false);
      if (!acts)
        g.endTransparencyLayer ();
    }
}

void
BarKeyboardComponent::paintEncoderGlyph (juce::Graphics &g, int column,
                                         int row) const
{
  auto const index = knobIndex (column, row);
  auto const glyph = keyboardEncoderGlyph (jobOfKnob (index));
  if (glyph == KnobGlyph::None)
    return;

  auto const cell
      = keyboardEncoderSlot (getLocalBounds (), column, row).glyph.toFloat ();
  // Dimmed with its knob, the way every control that cannot act is.
  auto const alpha = _encoderKnobs[index].isEnabled ()
                         ? 1.f
                         : theme ().alphaDisabled;
  g.setColour (toColour (theme ().textPrimary, alpha));
  g.fillPath (knobGlyphPath (
      glyph, cell.reduced (cell.getWidth () * glyphInsetShare)));
}

}
