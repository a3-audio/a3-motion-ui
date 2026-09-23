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

#include "SkinEditorComponent.hh"

#include <a3-motion-ui/components/ListScroll.hh>

#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
constexpr int paddingH = 24;
constexpr int paddingV = 20;
constexpr int rowGap = 4;
constexpr int maxPanelW = 560;

// Structural washes, the same grammar the settings menu uses.
constexpr float rowWash = 0.063f;
constexpr float browsedRowWash = 0.086f;
constexpr float armedRowWash = 0.133f;
}

SkinEditorComponent::SkinEditorComponent ()
{
  // Not for itself, but for its children: the dimmed area beside the panel
  // stays transparent to touch, the rows on it do not.
  setInterceptsMouseClicks (false, true);
  setWantsKeyboardFocus (true);

  createTouchControls ();
}

void
SkinEditorComponent::browseRow (int index)
{
  if (index < 0 || index >= totalRows ())
    return;

  // Step over a heading in the direction the browse was going. Always
  // stepping down meant a drag upwards that landed on one was pushed back
  // where it came from — the list could not be scrolled past the first
  // group, and the action rows above it were unreachable.
  _index = skipHeadings (index, index < _index ? -1 : 1);
  // Letting the armed row go, exactly as turning to another row does:
  // otherwise the next drag would edit a row nobody is looking at.
  _editing = false;
  // And calling off a pending delete, for the same reason turning away does:
  // it must never wait around for a press meant for something else.
  _deleteAsked = false;
  _saved = false;

  // Bring it into view if it is not, and no further — before laying out, so
  // the rows' hit areas are placed against the window that will be drawn.
  // The list does not rearrange itself around a selection any more.
  _scrollTop = scrollToShow (_scrollTop, _index, visibleRows (), totalRows ());

  resized ();
  repaint ();
}

int
SkinEditorComponent::firstVisibleRow () const
{
  // Where the window is, not where the selection is. It used to be
  // `_index - rows / 2`: the window went wherever the selection went, so a
  // row you touched slid to the middle and left your finger behind. The
  // window follows the selection only when it has to — see scrollToShow().
  return scrollBy (_scrollTop, 0, visibleRows (), totalRows ());
}

void
SkinEditorComponent::scrollList (int steps)
{
  auto const moved = scrollBy (_scrollTop, steps, visibleRows (), totalRows ());
  if (moved == _scrollTop)
    return;

  _scrollTop = moved;
  resized (); // the rows' hit areas move with what is under them
  repaint ();
}

juce::Rectangle<int>
SkinEditorComponent::listPanelBounds () const
{
  auto const rows = visibleRows ();
  auto const itemH
      = static_cast<int> (theme ().fontSize (FontRole::Body) * 1.9f);
  auto const headerH
      = static_cast<int> (theme ().fontSize (FontRole::Header) * 2.2f);

  auto const panelW = juce::jmin (maxPanelW, getWidth () - 2 * paddingH);
  auto const panelH
      = paddingV * 2 + headerH + rows * itemH + (rows - 1) * rowGap;

  return juce::Rectangle<int> ((getWidth () - panelW) / 2,
                               (getHeight () - panelH) / 2, panelW, panelH);
}

juce::Rectangle<int>
SkinEditorComponent::listContentBounds () const
{
  auto const headerH
      = static_cast<int> (theme ().fontSize (FontRole::Header) * 2.2f);

  auto content = listPanelBounds ().reduced (paddingH, paddingV);
  content.removeFromTop (headerH);
  return content;
}

juce::Rectangle<int>
SkinEditorComponent::visibleRowBounds (int slot) const
{
  auto const itemH
      = static_cast<int> (theme ().fontSize (FontRole::Body) * 1.9f);
  auto const content = listContentBounds ();

  return juce::Rectangle<int> (content.getX (),
                               content.getY () + slot * (itemH + rowGap),
                               content.getWidth (), itemH);
}

juce::Rectangle<int>
SkinEditorComponent::rowValueArea (juce::Rectangle<int> row,
                                   int absoluteIndex) const
{
  bool const isAction = absoluteIndex < _actionRows;
  auto const valueShare
      = (!isAction && rowValue (absoluteIndex).length () > 8) ? 2 : 3;

  return row.removeFromRight (row.getWidth () / valueShare)
      .reduced (juce::roundToInt (theme ().padding), 0);
}

juce::Rectangle<int>
SkinEditorComponent::rowNameArea (juce::Rectangle<int> row,
                                  int absoluteIndex) const
{
  bool const isAction = absoluteIndex < _actionRows;
  auto const valueShare
      = (!isAction && rowValue (absoluteIndex).length () > 8) ? 2 : 3;

  row.removeFromRight (row.getWidth () / valueShare);
  return row.reduced (juce::roundToInt (theme ().padding), 0);
}

void
SkinEditorComponent::createTouchControls ()
{
  // Behind everything: a drag anywhere on the list that is not on a value
  // field scrolls it. Added first so the rows sit in front of it.
  auto *const latch = &FingerLatch::forGroup (FingerLatch::menuList);

  _listScroll = std::make_unique<TouchControl> ();
  _listScroll->setFingerLatch (latch);
  _listScroll->onDragIncrement = [this] (int, int, int increment) {
    // The page under the finger, like the strips beside it. It used to call
    // navigate(), which turns the *armed row's value* — a drag on empty space
    // in a list quietly editing whatever happened to be selected.
    scrollList (increment);
  };
  addAndMakeVisible (*_listScroll);

  // As many pairs as the panel can ever draw. Which absolute row each shows
  // changes as the list scrolls, so resized() re-labels them.
  for (int slot = 0; slot < 24; ++slot)
    {
      RowTouch touch;

      // A tap selects and does nothing else, a double tap opens the row's
      // mask, a drag scrolls. The same three answers on the name and on the
      // value: "kein edit ohne eingabemaske, das kollidiert mit scroll." A
      // tap used to press the row -- fire Save, arm a number -- and a drag on
      // the value turned it, so a list that was being scrolled was also being
      // edited.
      touch.name = std::make_unique<TouchControl> ();
      touch.name->onTap = [this] (int absoluteRow, int) {
        if (!isHeadingRow (absoluteRow))
          browseRow (absoluteRow);
      };
      touch.name->onDoubleTap = [this] (int absoluteRow, int) {
        doubleTapRow (absoluteRow);
      };
      touch.name->onDragIncrement = [this] (int, int, int increment) {
        scrollList (increment);
      };

      touch.value = std::make_unique<TouchControl> ();
      touch.value->onTap = [this] (int absoluteRow, int) {
        if (!isHeadingRow (absoluteRow))
          browseRow (absoluteRow);
      };
      touch.value->onDoubleTap = [this] (int absoluteRow, int) {
        doubleTapRow (absoluteRow);
      };
      touch.value->onDragIncrement = [this] (int, int, int increment) {
        scrollList (increment);
      };
      touch.name->setFingerLatch (latch);
      touch.value->setFingerLatch (latch);

      addAndMakeVisible (*touch.name);
      addAndMakeVisible (*touch.value);
      _rowTouch.push_back (std::move (touch));
    }

  // The mask's two keys for a skin number: dialling while watching the
  // sphere, which the drag on the row used to do, kept -- inside the mask.
  for (auto const key : { maskMinusKey, maskPlusKey })
    {
      auto control = std::make_unique<TouchControl> ();
      control->setIdentity (key);
      control->onTap = [this] (int which, int) {
        stepTypedNumber (which == maskPlusKey ? 1 : -1);
      };
      addChildComponent (*control);
      (key == maskPlusKey ? _maskPlus : _maskMinus) = std::move (control);
    }
}

void
SkinEditorComponent::resized ()
{
  auto const keysShown = hasStepKeys ();
  _maskMinus->setVisible (keysShown);
  _maskPlus->setVisible (keysShown);
  if (keysShown)
    {
      _maskMinus->setBounds (maskKeyBounds (false));
      _maskPlus->setBounds (maskKeyBounds (true));
    }

  // While a name is being typed the list steps aside, so nothing on it can
  // be aimed at.
  auto const listShown = totalRows () > 0 && !_naming;

  _listScroll->setVisible (listShown);
  if (listShown)
    _listScroll->setBounds (listContentBounds ());
  // One row of list per row of finger. The skin's step (12 px) against a row
  // of 34 ran the list three times faster than the hand, a row at a time.
  _listScroll->setPixelsPerStep (rowPitch ());

  auto const rows = listShown ? visibleRows () : 0;
  auto const first = firstVisibleRow ();

  for (size_t slot = 0; slot < _rowTouch.size (); ++slot)
    {
      auto const index = first + static_cast<int> (slot);
      auto const shown = listShown && static_cast<int> (slot) < rows
                         && index < totalRows ();

      auto &touch = _rowTouch[slot];
      // Every area, shown or not: one scrolled into view mid-drag drags at
      // the same rate as the rest.
      touch.name->setPixelsPerStep (rowPitch ());
      touch.value->setPixelsPerStep (rowPitch ());
      touch.name->setVisible (shown);
      touch.value->setVisible (shown);
      if (!shown)
        continue;

      // Visible on a heading too. A drag stays with the area the finger went
      // down on only while that area is visible, and scrolling re-labels the
      // areas: one that came to stand for a heading was hidden, and the drag
      // under it stopped half way -- on the list, never beside it. A heading
      // ignores the tap instead (see the tap handlers).
      auto const row = visibleRowBounds (static_cast<int> (slot));
      touch.name->setIdentity (index);
      touch.value->setIdentity (index);
      touch.name->setBounds (rowNameArea (row, index));
      touch.value->setBounds (rowValueArea (row, index));
    }
}

void
SkinEditorComponent::visibilityChanged ()
{
  // The arrows have to land here, and a component that is not focused never
  // sees a key. Focus used to be taken only when a name was being typed,
  // which is why the arrows did nothing at all the rest of the time.
  if (isShowing ())
    grabKeyboardFocus ();
}

void
SkinEditorComponent::mouseWheelMove (juce::MouseEvent const &,
                                     juce::MouseWheelDetails const &wheel)
{
  // What a two-finger scroll arrives as. The list follows the finger, the way
  // the browser's does and the way a drag over the rows here already did --
  // pushing the list up shows what is below it.
  auto const delta = wheel.deltaY > 0.f ? -1 : (wheel.deltaY < 0.f ? 1 : 0);
  if (delta != 0)
    scrollList (delta);
}

bool
SkinEditorComponent::keyPressed (juce::KeyPress const &key)
{
  if (!_naming)
    {
      // A keyboard's arrows are navigation and nothing else. Deliberately not
      // navigate(): that carries the encoder's second level, so an arrow on an
      // armed row would change its value -- and somebody reaching for an arrow
      // is trying to get somewhere, not to edit. browseRow() also steps over a
      // heading *in the direction of travel* and brings the row into view,
      // which is both of the things arrows have to do.
      if (key == juce::KeyPress::upKey)
        {
          browseRow (_index - 1);
          return true;
        }
      if (key == juce::KeyPress::downKey)
        {
          browseRow (_index + 1);
          return true;
        }

      // A page is the window moving, not the selection: that is what a page
      // key means everywhere else, and it is the only way to cross a hundred
      // and forty rows without dragging them past one at a time.
      if (key == juce::KeyPress::pageUpKey)
        {
          scrollList (-visibleRows ());
          return true;
        }
      if (key == juce::KeyPress::pageDownKey)
        {
          scrollList (visibleRows ());
          return true;
        }

      // Enter is the keyboard's double tap: it opens the selected row.
      if (key == juce::KeyPress::returnKey)
        {
          openBrowsedRow ();
          return true;
        }

      return false;
    }

  if (key == juce::KeyPress::backspaceKey)
    {
      backspaceName ();
      return true;
    }

  if (key == juce::KeyPress::returnKey)
    {
      finishNaming ();
      return true;
    }

  if (key == juce::KeyPress::escapeKey)
    {
      cancelNaming ();
      return true;
    }

  // In a skin number's mask the arrows up and down are its minus and plus.
  if (hasStepKeys ()
      && (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey))
    {
      stepTypedNumber (key == juce::KeyPress::upKey ? 1 : -1);
      return true;
    }

  if (key == juce::KeyPress::leftKey)
    {
      _nameEntry.moveCursor (-1);
      repaint ();
      return true;
    }

  if (key == juce::KeyPress::rightKey)
    {
      _nameEntry.moveCursor (1);
      repaint ();
      return true;
    }

  auto const character = key.getTextCharacter ();
  if (character == 0)
    return false;

  // Lower case throughout: a name and a host are lower case, and the field
  // ignores what it may not hold anyway — this only saves a trip through
  // shift for the common case.
  typeIntoName (juce::CharacterFunctions::toLowerCase (character));
  return true;
}

void
SkinEditorComponent::setSkin (juce::var skin, juce::String const &name)
{
  setDocument (std::move (skin), name, true, Numbers::Turned);
}

void
SkinEditorComponent::setDocument (juce::var document, juce::String const &title,
                                  bool isSkinDocument, Numbers numbers)
{
  _actionRows = isSkinDocument ? 5 : 0;
  _numbers = numbers;
  _skin = std::move (document);
  _name = title;
  // Only an actual skin gets the theme's defaults merged in (see
  // skinParameters()) -- a config-page slice like the Network page shares
  // this component but is not one, and would otherwise show every theme
  // colour and metric ahead of its own two or three fields.
  _parameters = skinParameters (_skin, isSkinDocument);
  rebuildRows ();
  // Every page opens at its top: carrying a row number over from another
  // document lands on whatever happens to sit at that number.
  _index = 0;
  _editing = false;
  _naming = false;
  resized (); // the list steps aside while typing; its hit areas follow
  _deleteAsked = false;
  _textPath = {};
  repaint ();
}

void
SkinEditorComponent::rebuildRows ()
{
  _rows.clear ();

  for (int i = 0; i < _actionRows; ++i)
    {
      Row kind = Row::Save;
      switch (i)
        {
        case 0: kind = Row::Save; break;
        case 1: kind = Row::SaveAsNew; break;
        case 2: kind = Row::Rename; break;
        case 3: kind = Row::Delete; break;
        default: kind = Row::Reset; break;
        }
      _rows.push_back ({ kind, -1, {} });
    }

  // A heading wherever the group changes, and the group comes with the
  // parameter (SkinGroups.hh) rather than being read off its path. The path
  // was the file's own nesting, which grouped a skin the way it happens to be
  // written rather than the way it is read: eighty-five keys in alphabetical
  // order, the twenty-one that design a skin scattered among the blocks that
  // tune a shader.
  //
  // The network page is untouched: its keys match none of the skin's groups
  // and fall back to their own parent path, which is exactly what grouped
  // them before.
  juce::String group;
  for (size_t i = 0; i < _parameters.size (); ++i)
    {
      auto const here = _parameters[i].group;

      if (here != group)
        {
          group = here;
          if (here.isNotEmpty ())
            _rows.push_back ({ Row::Heading, -1, here });
        }

      _rows.push_back ({ Row::Parameter, (int)i, {} });
    }
}

int
SkinEditorComponent::totalRows () const
{
  return (int)_rows.size ();
}

int
SkinEditorComponent::skipHeadings (int index, int delta) const
{
  auto const step = delta >= 0 ? 1 : -1;

  while (index >= 0 && index < totalRows ()
         && _rows[(size_t)index].kind == Row::Heading)
    index += step;

  return juce::jlimit (0, juce::jmax (0, totalRows () - 1), index);
}

SkinParameter const *
SkinEditorComponent::browsedParameter () const
{
  if (_index < 0 || _index >= totalRows ())
    return nullptr;

  auto const &row = _rows[(size_t)_index];
  if (row.kind != Row::Parameter || row.parameter < 0
      || row.parameter >= (int)_parameters.size ())
    return nullptr;

  return &_parameters[(size_t)row.parameter];
}

double
SkinEditorComponent::parameterValue (SkinParameter const &parameter) const
{
  if (skinHasValue (_skin, parameter.path))
    return skinValue (_skin, parameter.path);

  // Absent from the file: fall back to the theme's own default rather than
  // the 0 skinValue() would otherwise answer with, so a role merged in by
  // skinParameters() reads as the value the app is actually drawing with.
  // Once an edit writes the path, skinHasValue() above starts saying true
  // and this is never consulted again.
  return parameter.hasDefault ? static_cast<double> (parameter.defaultValue)
                              : 0.0;
}

double
SkinEditorComponent::colourChannelValue (SkinParameter const &parameter,
                                         char const *channel) const
{
  auto const path = parameter.path + "." + channel;
  if (skinHasValue (_skin, path))
    return skinValue (_skin, path);

  // Same fallback as parameterValue(), but the default for a whole colour is
  // one var covering r, g and b together (see themeDefaultsVar()), so the
  // channel is pulled out of it here instead of being a value of its own.
  auto const identifier = juce::Identifier (channel);
  return parameter.hasDefault
             ? static_cast<double> (
                 parameter.defaultValue.getProperty (identifier, 0))
             : 0.0;
}

SkinEditorComponent::Row
SkinEditorComponent::browsedRow () const
{
  if (_index < 0 || _index >= totalRows ())
    return Row::Parameter;

  return _rows[(size_t)_index].kind;
}

void
SkinEditorComponent::finishNaming ()
{
  if (!_naming)
    return;

  auto const typed = _nameEntry.name ();
  auto const path = _textPath;
  _naming = false;
  _steppedInMask = false;
  resized (); // the list steps aside while typing; its hit areas follow
  _editing = false;
  _textPath = {};
  repaint ();

  if (onNamingChanged)
    onNamingChanged (false);

  if (path.isNotEmpty ())
    {
      // A parameter, not the document's own name.
      if (_typingNumber)
        {
          // The same range the encoder is held to. Typing is the other way in,
          // and a bound only one of them respects is not a bound.
          auto const *browsed = browsedParameter ();
          if (browsed == nullptr)
            return;
          auto const &parameter = *browsed;
          setSkinValue (
              _skin, path,
              clampSkinValue (_skin, path, typed.getDoubleValue ()),
              parameter.isWholeNumber);
        }
      else
        setSkinText (_skin, path, typed);

      _typingNumber = false;
      repaint ();
      if (onValueChanged)
        onValueChanged ();
      return;
    }

  if (onRename && typed.isNotEmpty () && typed != _name)
    onRename (typed);
}

void
SkinEditorComponent::navigate (int delta)
{
  if (delta == 0)
    return;

  if (_naming)
    {
      // Same two-level rhythm as everywhere else: armed changes the letter,
      // otherwise the cursor walks along the name.
      if (_editing)
        _nameEntry.changeCharacter (delta);
      else
        _nameEntry.moveCursor (delta);
      repaint ();
      return;
    }

  // Walks the rows and nothing else. A value changes in its mask; nothing in
  // the list is armed for turning any more.
  _index = skipHeadings (
      juce::jlimit (0, totalRows () - 1, _index + delta), delta);
  // Turning away is how a delete is called off — it never waits around
  // for a press that was meant for something else.
  _deleteAsked = false;
  _saved = false;
  repaint ();
}

void
SkinEditorComponent::cancelNaming ()
{
  if (!_naming)
    return;

  // Only a number can have changed before Enter: the minus and plus keys
  // write it at once. Text and a name are only written by finishNaming().
  auto const restore = _typingNumber && _steppedInMask;
  auto const path = _textPath;

  _naming = false;
  _steppedInMask = false;
  _typingNumber = false;
  _editing = false;
  _textPath = {};
  resized ();
  repaint ();

  if (onNamingChanged)
    onNamingChanged (false);

  if (restore && path.isNotEmpty ())
    {
      _skin = juce::JSON::parse (_documentBeforeMask);
      _parameters = skinParameters (_skin, _actionRows > 0);
      repaint ();
      if (onValueChanged)
        onValueChanged ();
    }
}

void
SkinEditorComponent::openBrowsedRow ()
{
  if (_naming)
    return;
  toggleEditing ();
}

bool
SkinEditorComponent::hasStepKeys () const
{
  return _naming && _typingNumber && _numbers == Numbers::Turned;
}

juce::Rectangle<int>
SkinEditorComponent::maskKeyBounds (bool plus) const
{
  // Laid out the way paint() draws the mask: header, the typed field, a gap,
  // then the keys -- a row of two, each half the panel and a fingertip tall
  // at least.
  auto const itemH
      = static_cast<int> (theme ().fontSize (FontRole::Body) * 1.9f);
  auto const headerH
      = static_cast<int> (theme ().fontSize (FontRole::Header) * 2.2f);

  auto content = listPanelBounds ().reduced (paddingH, paddingV);
  content.removeFromTop (headerH);
  content.removeFromTop (
      typingFieldHeight (theme ().fontSize (FontRole::Header), itemH));
  content.removeFromTop (rowGap);

  auto keys = content.removeFromTop (juce::jmax (2 * itemH, 48));
  // Apart by a full padding, so the two read as two keys and not one bar.
  auto const half = (keys.getWidth () - paddingH) / 2;
  return plus ? keys.removeFromRight (half) : keys.removeFromLeft (half);
}

void
SkinEditorComponent::toggleEditing ()
{
  if (_naming)
    {
      _editing = !_editing;
      repaint ();
      return;
    }

  switch (browsedRow ())
    {
    case Row::Save:
      _saved = true;
      repaint ();
      if (onSave)
        onSave ();
      return;

    case Row::SaveAsNew:
      if (onSaveAsNew)
        onSaveAsNew ();
      return;

    case Row::Rename:
      _nameEntry = TextInput{ _name };
      _naming = true;
      resized (); // the list steps aside while typing; its hit areas follow
      _editing = false;
      repaint ();
      grabKeyboardFocus (); // the keys have to land here, not in the void
      if (onNamingChanged)
        onNamingChanged (true);
      return;

    case Row::Delete:
      // Asked twice: a skin is somebody's work, and one press of the only
      // control on the panel is too easy to make by accident.
      if (!_deleteAsked)
        {
          _deleteAsked = true;
          repaint ();
          return;
        }
      _deleteAsked = false;
      if (onDelete)
        onDelete ();
      return;

    case Row::Reset:
      // Not asked twice, unlike Delete: this destroys nothing that cannot be
      // dialled back in, and it is the way out of a skin nobody can read any
      // more.
      if (onReset)
        onReset ();
      return;

    case Row::Parameter:
      {
        if (_parameters.empty ())
          return;

        auto const *browsed = browsedParameter ();
  if (browsed == nullptr)
    return;
  auto const &parameter = *browsed;
        if (parameter.isColour)
          {
            // Resolved the same way the list row is drawn, not re-read from
            // the raw document: a role the file leaves unstated is 0 in the
            // document but the theme's default here, which is what stopped
            // the picker opening on black for every colour a skin omits.
            if (onColourPicked)
              onColourPicked (
                  parameter.path,
                  juce::Colour (
                      (juce::uint8)juce::jlimit (
                          0, 255, (int)colourChannelValue (parameter, "r")),
                      (juce::uint8)juce::jlimit (
                          0, 255, (int)colourChannelValue (parameter, "g")),
                      (juce::uint8)juce::jlimit (
                          0, 255, (int)colourChannelValue (parameter, "b"))));
            return;
          }

        if (parameter.isText)
          {
            // Text is typed, not turned. The alphabet follows what the value
            // is: a host takes dots, a path takes slashes as well.
            auto const alphabet = parameter.path.containsIgnoreCase ("dir")
                                      ? TextInput::pathAlphabet
                                      : TextInput::hostAlphabet;

            _textPath = parameter.path;
            _typingNumber = false;
            _steppedInMask = false;
            _nameEntry
                = TextInput{ skinText (_skin, parameter.path), alphabet };
            _naming = true;
            resized (); // the list steps aside while typing; its hit areas follow
            _editing = false;
            repaint ();
            if (onNamingChanged)
              onNamingChanged (true);
            return;
          }

        // Every number is typed now. A skin number's mask has minus and plus
        // as well, which is where dialling went.
        beginTypingBrowsedRow ();
        return;
      }
    }
}

int
typingFieldHeight (float textFontSize, int rowHeight)
{
  // The same proportion an ordinary row gives its text, applied to the font
  // the field actually draws with — and never tighter than a normal row.
  return juce::jmax (rowHeight,
                     static_cast<int> (std::lround (textFontSize * 1.9f)));
}

float
typingCaretY (juce::Rectangle<int> field, juce::Font const &font)
{
  return static_cast<float> (field.getCentreY ()) + font.getHeight () * 0.5f;
}

juce::String
SkinEditorComponent::browsedPath () const
{
  if (browsedRow () != Row::Parameter || _parameters.empty ())
    return {};

  auto const *browsed = browsedParameter ();
  return browsed != nullptr ? browsed->path : juce::String{};
}

bool
SkinEditorComponent::canTypeBrowsedRow () const
{
  if (_naming)
    return true;

  return browsedRow () == Row::Rename
         || (browsedRow () == Row::Parameter && !_parameters.empty ());
}

void
SkinEditorComponent::doubleTapRow (int absoluteRow)
{
  // The row the second finger landed on, not the one that happened to be
  // armed: the first tap of the pair browses, but a double tap somewhere else
  // must open what it was made on.
  browseRow (absoluteRow);
  if (browsedRowIndex () != absoluteRow)
    return; // a heading, or off the end of the list

  openBrowsedRow ();
}

bool
SkinEditorComponent::beginTypingBrowsedRow ()
{
  if (_naming || !canTypeBrowsedRow ())
    return false;

  if (browsedRow () == Row::Rename)
    {
      toggleEditing (); // the rename row already knows how
      return true;
    }

  auto const *browsed = browsedParameter ();
  if (browsed == nullptr)
    return false;
  auto const &parameter = *browsed;
  if (parameter.isColour)
    return false; // a colour is picked, not typed

  // A number is typed as its digits and read back as a number on the way
  // out — one keyboard, whatever the row holds.
  _textPath = parameter.path;
  _typingNumber = !parameter.isText;
  _steppedInMask = false;
  // The whole document, not the one number: a value the file never stated
  // shows the theme's default in the row but reads as 0 from the document,
  // and putting 0 back would write a key the skin never had.
  if (_typingNumber)
    _documentBeforeMask = juce::JSON::toString (_skin, true);
  _nameEntry = TextInput{ parameter.isText
                              ? skinText (_skin, parameter.path)
                              : rowValue (_index),
                          parameter.isText ? TextInput::pathAlphabet
                                           : TextInput::numberAlphabet };
  _naming = true;
  resized (); // the list steps aside while typing; its hit areas follow
  _editing = false;
  repaint ();

  if (onNamingChanged)
    onNamingChanged (true);

  return true;
}

void
SkinEditorComponent::stepTypedNumber (int delta)
{
  if (!hasStepKeys () || delta == 0 || _textPath.isEmpty ())
    return;

  auto const *browsed = browsedParameter ();
  if (browsed == nullptr)
    return;
  auto const &parameter = *browsed;

  // From what the field says, so a number typed and then nudged is nudged
  // from where the hand put it.
  auto const from = _nameEntry.name ().trim ().isNotEmpty ()
                        ? _nameEntry.name ().getDoubleValue ()
                        : skinValue (_skin, _textPath);
  auto const stepped
      = clampSkinValue (_skin, _textPath,
                        stepSkinValue (from, delta, parameter.isWholeNumber,
                                       isColourChannelPath (_textPath)));

  setSkinValue (_skin, _textPath, stepped, parameter.isWholeNumber);
  _steppedInMask = true;
  _nameEntry = TextInput{ rowValue (_index), TextInput::numberAlphabet };
  repaint ();

  if (onValueChanged)
    onValueChanged ();
}

void
SkinEditorComponent::typeIntoName (juce::juce_wchar character)
{
  if (!_naming)
    return;

  _nameEntry.type (character);
  repaint ();
}

void
SkinEditorComponent::backspaceName ()
{
  if (!_naming)
    return;

  _nameEntry.backspace ();
  repaint ();
}

juce::String
SkinEditorComponent::rowLabel (int index) const
{
  if (index < 0 || index >= totalRows ())
    return {};

  auto const &row = _rows[(size_t)index];

  switch (row.kind)
    {
    case Row::Heading: return row.heading;
    case Row::Parameter:
      {
        // Under its heading the group is already said; the row only has to
        // add what it is called within it.
        auto const &path = _parameters[(size_t)row.parameter].path;
        auto const dot = path.lastIndexOfChar ('.');
        return dot > 0 ? path.substring (dot + 1) : path;
      }
    case Row::Save: return juce::String::fromUTF8 ("\xc2\xbb Save");
    case Row::SaveAsNew: return juce::String::fromUTF8 ("\xc2\xbb Save as new");
    case Row::Rename: return juce::String::fromUTF8 ("\xc2\xbb Rename");
    case Row::Delete: return juce::String::fromUTF8 ("\xc2\xbb Delete");
    case Row::Reset: return juce::String::fromUTF8 ("\xc2\xbb Reset");
    }

  return {};
}

juce::String
SkinEditorComponent::rowValue (int index) const
{
  if (index < 0 || index >= totalRows ()
      || _rows[(size_t)index].kind != Row::Parameter)
    {
      if (index == 3 && _deleteAsked)
        return "sure?";
      return (index == 0 && _saved) ? "saved" : "";
    }

  auto const &parameter
      = _parameters[(size_t)_rows[(size_t)index].parameter];
  if (parameter.isColour)
    return {};
  if (parameter.isText)
    return skinText (_skin, parameter.path);

  auto const value = parameterValue (parameter);

  return parameter.isWholeNumber ? juce::String ((int)std::lround (value))
                                 : juce::String (value, 3);
}

int
SkinEditorComponent::visibleRows () const
{
  auto const rowHeight
      = static_cast<int> (theme ().fontSize (FontRole::Body) * 1.9f) + rowGap;
  auto const room = getHeight () - 2 * paddingV
                    - static_cast<int> (theme ().fontSize (FontRole::Header)
                                        * 2.2f);

  return juce::jlimit (3, 24, room / juce::jmax (1, rowHeight));
}

void
SkinEditorComponent::paint (juce::Graphics &g)
{
  // Dim around the panel; the panel itself is solid, filled below. See
  // GlobalSettingsComponent::paint() for why that is two fills.
  g.fillAll (toColour (theme ().surface, theme ().overlayScrim));

  if (totalRows () == 0)
    return;

  auto const rows = visibleRows ();
  auto const itemH = static_cast<int> (theme ().fontSize (FontRole::Body) * 1.9f);
  auto const headerH = static_cast<int> (theme ().fontSize (FontRole::Header) * 2.2f);

  auto const panelBounds = listPanelBounds ();

  g.setColour (toColour (theme ().surface, theme ().overlayOpacity));
  g.fillRoundedRectangle (panelBounds.toFloat (), theme ().radiusPanel);
  g.setColour (toColour (theme ().textPrimary, rowWash));
  g.fillRoundedRectangle (panelBounds.toFloat (), theme ().radiusPanel);

  auto content = panelBounds.reduced (paddingH, paddingV);

  auto headerArea = content.removeFromTop (headerH);
  g.setFont (juce::Font (theme ().fontSize (FontRole::Header), juce::Font::bold));
  g.setColour (toColour (theme ().accent));
  // A skin says so; any other page is already named by what it holds.
  g.drawText (_actionRows > 0 ? "Skin: " + _name : _name, headerArea,
              juce::Justification::centredLeft, true);
  g.setColour (toColour (theme ().textPrimary, theme ().alphaInactive));
  g.setFont (juce::Font (theme ().fontSize (FontRole::Body), juce::Font::plain));
  g.drawText (juce::String (_index + 1) + " / "
                  + juce::String ((int)_parameters.size ()),
              headerArea, juce::Justification::centredRight, true);

  // The list is far longer than the screen, so it scrolls around the
  // selected row rather than paging — the row being edited stays put while
  // its value changes.
  // While a name is being typed the list steps aside: one thing at a time on
  // a panel with one control.
  if (_naming)
    {
      // Drawn as text with a caret under it, not a character per cell: a
      // cell grid reads as spaced-out letters, which a short name survives
      // and a path does not.
      auto const font
          = juce::Font (juce::FontOptions (theme ().fontSize (FontRole::Header)));

      auto nameRow = content.removeFromTop (typingFieldHeight (
          theme ().fontSize (FontRole::Header), itemH));
      g.setColour (toColour (theme ().textPrimary, browsedRowWash));
      g.fillRoundedRectangle (nameRow.toFloat (), theme ().radiusRow);

      g.setFont (font);

      auto const textArea
          = nameRow.reduced (juce::roundToInt (theme ().padding), 0);
      auto const typed = _nameEntry.buffer ().trimEnd ();

      g.setColour (toColour (theme ().textPrimary));
      g.drawText (typed, textArea, juce::Justification::centredLeft, false);

      // The caret sits after everything before it.
      auto const before = juce::GlyphArrangement::getStringWidth (
          font, _nameEntry.buffer ().substring (0, _nameEntry.cursor ()));
      auto const caretW = juce::jmax (
          2.f, juce::GlyphArrangement::getStringWidth (font, "n"));

      // Full opacity while editing rather than an alpha rung: "editing" has
      // always meant no dimming at all, which the alpha-less overload already
      // says. This used to be `_editing ? 1.f : theme ().alphaInactive`; 1.f
      // fits no rung, and full opacity is the absence of an emphasis decision
      // rather than one of its rungs, so it deliberately gets no role of its
      // own. See issues/a3-motion-ui-metric-role-deviations.md (Task 16).
      g.setColour (_editing ? toColour (theme ().accent)
                            : toColour (theme ().accent,
                                       theme ().alphaInactive));
      g.fillRect (static_cast<float> (textArea.getX ()) + before,
                  typingCaretY (textArea, font), caretW,
                  theme ().strokeThick);

      content.removeFromTop (rowGap);

      if (hasStepKeys ())
        {
          // The same rectangles the touch areas sit on (maskKeyBounds), so the
          // picture and the target cannot drift apart.
          for (auto const plus : { false, true })
            {
              auto const key = maskKeyBounds (plus);
              g.setColour (toColour (theme ().textPrimary, armedRowWash));
              g.fillRoundedRectangle (key.toFloat (), theme ().radiusRow);
              g.setColour (toColour (theme ().textPrimary, browsedRowWash));
              g.drawRoundedRectangle (key.toFloat (), theme ().radiusRow,
                                      theme ().strokeThick);
              // As big as the key allows: it is aimed at, not read.
              g.setColour (toColour (theme ().textPrimary));
              g.setFont (juce::Font (
                  juce::FontOptions (static_cast<float> (key.getHeight ())
                                     * 0.7f)
                      .withStyle ("Bold")));
              g.drawText (plus ? "+" : juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")),
                          key, juce::Justification::centred, false);
            }
          content.setTop (maskKeyBounds (true).getBottom () + rowGap);
        }

      g.setFont (
          juce::Font (theme ().fontSize (FontRole::Body), juce::Font::plain));
      g.setColour (toColour (theme ().textPrimary, theme ().alphaInactive));
      g.drawText (hasStepKeys () ? "enter: keep   esc / back: undo   up / down: step"
                                 : "enter: keep   esc / back: undo",
                  content.removeFromTop (itemH),
                  juce::Justification::centredLeft, true);
      return;
    }

  auto const first = firstVisibleRow ();

  for (int i = 0; i < rows; ++i)
    {
      auto const index = first + i;
      if (index >= totalRows ())
        break;

      auto const row = visibleRowBounds (i);

      auto const kind = _rows[(size_t)index].kind;
      bool const isAction
          = kind != Row::Parameter && kind != Row::Heading;
      bool const isHeading = kind == Row::Heading;
      bool const isBrowsed = index == _index;
      bool const isArmed = isBrowsed && _editing;

      if (isHeading)
        {
          // No wash and no value: a heading names what follows, it is not
          // one of the things you can land on. Drawn small and in the
          // accent so the eye finds the boundaries between groups without
          // reading them.
          g.setFont (juce::Font (theme ().fontSize (FontRole::Body) * 0.85f,
                                 juce::Font::bold));
          g.setColour (toColour (theme ().accent, theme ().alphaInactive));
          g.drawText (rowLabel (index),
                      row.reduced (juce::roundToInt (theme ().padding), 0),
                      juce::Justification::centredLeft, true);
          continue;
        }

      g.setColour (toColour (theme ().textPrimary,
                             isArmed     ? armedRowWash
                             : isBrowsed ? browsedRowWash
                                         : rowWash));
      g.fillRoundedRectangle (row.toFloat (), theme ().radiusRow);

      auto const valueArea = rowValueArea (row, index);
      auto const nameArea = rowNameArea (row, index);

      g.setFont (
          juce::Font (theme ().fontSize (FontRole::Body), juce::Font::plain));
      // Same restructuring as the caret above: full opacity for the browsed
      // row's name rather than an alpha rung. Was `isBrowsed ? 1.f : theme
      // ().alphaInactive`. See issues/a3-motion-ui-metric-role-deviations.md
      // (Task 16).
      g.setColour (isAction && isBrowsed
                       ? toColour (theme ().accent)
                       : (isBrowsed
                              ? toColour (theme ().textPrimary)
                              : toColour (theme ().textPrimary,
                                         theme ().alphaInactive)));
      g.drawText (rowLabel (index), nameArea, juce::Justification::centredLeft,
                  true);

      auto const shown = rowValue (index);

      // A colour channel shows the colour it is part of, so a number can be
      // judged without leaving the row it sits in.
      if (kind == Row::Parameter
          && _parameters[(size_t)_rows[(size_t)index].parameter].isColour)
        {
          auto const &colourParameter
              = _parameters[(size_t)_rows[(size_t)index].parameter];
          auto swatch = valueArea.reduced (
              valueArea.getWidth () / 4,
              juce::roundToInt (theme ().paddingSmall));
          g.setColour (juce::Colour (
              (juce::uint8)juce::jlimit (
                  0, 255, (int)colourChannelValue (colourParameter, "r")),
              (juce::uint8)juce::jlimit (
                  0, 255, (int)colourChannelValue (colourParameter, "g")),
              (juce::uint8)juce::jlimit (
                  0, 255, (int)colourChannelValue (colourParameter, "b"))));
          g.fillRoundedRectangle (swatch.toFloat (), theme ().radiusControl);
        }

      g.setFont (
          juce::Font (theme ().fontSize (FontRole::Body), juce::Font::bold));
      // Same restructuring again for the browsed row's value. Was
      // `isBrowsed ? 1.f : theme ().alphaInactive`. See
      // issues/a3-motion-ui-metric-role-deviations.md (Task 16).
      g.setColour (isArmed ? toColour (theme ().accent)
                          : (isBrowsed
                                 ? toColour (theme ().textPrimary)
                                 : toColour (theme ().textPrimary,
                                            theme ().alphaInactive)));
      g.drawText (shown, valueArea, juce::Justification::centredRight, true);
    }
}

bool
SkinEditorComponent::isHeadingRow (int index) const
{
  return index >= 0 && index < totalRows ()
         && _rows[(size_t)index].kind == Row::Heading;
}

int
SkinEditorComponent::rowPitch () const
{
  return static_cast<int> (theme ().fontSize (FontRole::Body) * 1.9f) + rowGap;
}

}
