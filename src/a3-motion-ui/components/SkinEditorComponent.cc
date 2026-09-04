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
constexpr float overlayOpacity = 0.55f;
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
  resized ();
  repaint ();
}

int
SkinEditorComponent::firstVisibleRow () const
{
  auto const rows = visibleRows ();

  // The list scrolls around the browsed row rather than paging, so the row
  // being edited stays put while its value changes.
  return juce::jlimit (0, juce::jmax (0, totalRows () - rows),
                       _index - rows / 2);
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

  return row.removeFromRight (row.getWidth () / valueShare).reduced (8, 0);
}

juce::Rectangle<int>
SkinEditorComponent::rowNameArea (juce::Rectangle<int> row,
                                  int absoluteIndex) const
{
  bool const isAction = absoluteIndex < _actionRows;
  auto const valueShare
      = (!isAction && rowValue (absoluteIndex).length () > 8) ? 2 : 3;

  row.removeFromRight (row.getWidth () / valueShare);
  return row.reduced (8, 0);
}

void
SkinEditorComponent::createTouchControls ()
{
  // Behind everything: a drag anywhere on the list that is not on a value
  // field moves the selection. Added first so the rows sit in front of it.
  _listScroll = std::make_unique<TouchControl> ();
  _listScroll->onDragIncrement = [this] (int, int, int increment) {
    navigate (increment);
  };
  addAndMakeVisible (*_listScroll);

  // As many pairs as the panel can ever draw. Which absolute row each shows
  // changes as the list scrolls, so resized() re-labels them.
  for (int slot = 0; slot < 24; ++slot)
    {
      RowTouch touch;

      touch.name = std::make_unique<TouchControl> ();
      touch.name->onTap = [this] (int absoluteRow, int) {
        browseRow (absoluteRow);

        // What the encoder press on this row would do — an action row acts,
        // a colour row opens the picker, a typed number calls the keyboard.
        // toggleEditing() already knows all of those cases.
        toggleEditing ();
      };
      // The name column is where the list is rolled. Leaving that to a
      // strip behind the rows meant it could only be grabbed in the gaps
      // between them, which is to say hardly at all. Left half rolls, right
      // half changes the value — the two halves of a row, two jobs.
      touch.name->onDragIncrement = [this] (int, int, int increment) {
        // Rolls, never edits. navigate() would have changed the armed row's
        // value instead, because that is its second level — but this column
        // is the list, not a value.
        browseRow (browsedRowIndex () + increment);
      };

      touch.value = std::make_unique<TouchControl> ();

      // The row is latched when the finger lands and held for the whole
      // drag — see _dragRow.
      touch.value->onPress = [this] (int absoluteRow, int) {
        _dragRow = absoluteRow;
        if (browsedRowIndex () != absoluteRow)
          browseRow (absoluteRow);
      };
      touch.value->onTap = [this] (int, int) {
        _dragRow = -1;
        toggleEditing ();
      };
      touch.value->onDragEnd = [this] (int, int) { _dragRow = -1; };
      touch.value->onDragIncrement = [this] (int, int, int increment) {
        if (_dragRow < 0 || _dragRow != browsedRowIndex ())
          return;

        // Only a number a drag can turn. Arming an action, a colour or a
        // text row here would fire it — which is exactly what dragging past
        // a colour used to do.
        if (!isEditing ())
          {
            if (!canTurnBrowsedRow ())
              return;
            toggleEditing ();
          }

        navigate (increment);
      };

      addAndMakeVisible (*touch.name);
      addAndMakeVisible (*touch.value);
      _rowTouch.push_back (std::move (touch));
    }
}

void
SkinEditorComponent::resized ()
{
  // While a name is being typed the list steps aside, so nothing on it can
  // be aimed at.
  auto const listShown = totalRows () > 0 && !_naming;

  _listScroll->setVisible (listShown);
  if (listShown)
    _listScroll->setBounds (listContentBounds ());

  auto const rows = listShown ? visibleRows () : 0;
  auto const first = firstVisibleRow ();

  for (size_t slot = 0; slot < _rowTouch.size (); ++slot)
    {
      auto const index = first + static_cast<int> (slot);
      auto const shown = listShown && static_cast<int> (slot) < rows
                         && index < totalRows ();

      auto &touch = _rowTouch[slot];
      touch.name->setVisible (shown);
      touch.value->setVisible (shown);
      if (!shown)
        continue;

      // A heading holds nothing, so nothing on it is worth touching. The
      // list is still rolled by dragging over it — that is the scroll strip
      // behind the rows, not these.
      auto const isHeading = _rows[(size_t)index].kind == Row::Heading;
      touch.name->setVisible (!isHeading);
      touch.value->setVisible (!isHeading);
      if (isHeading)
        continue;

      auto const row = visibleRowBounds (static_cast<int> (slot));
      touch.name->setIdentity (index);
      touch.value->setIdentity (index);
      touch.name->setBounds (rowNameArea (row, index));
      touch.value->setBounds (rowValueArea (row, index));
    }
}

bool
SkinEditorComponent::keyPressed (juce::KeyPress const &key)
{
  if (!_naming)
    return false;

  if (key == juce::KeyPress::backspaceKey)
    {
      backspaceName ();
      return true;
    }

  if (key == juce::KeyPress::returnKey || key == juce::KeyPress::escapeKey)
    {
      finishNaming ();
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
                                  bool withSkinActions, Numbers numbers)
{
  _actionRows = withSkinActions ? 5 : 0;
  _numbers = numbers;
  _skin = std::move (document);
  _name = title;
  _parameters = skinParameters (_skin);
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

  if (!_editing)
    {
      _index = skipHeadings (
          juce::jlimit (0, totalRows () - 1, _index + delta), delta);
      // Turning away is how a delete is called off — it never waits around
      // for a press that was meant for something else.
      _deleteAsked = false;
      _saved = false;
      repaint ();
      return;
    }

  if (browsedRow () != Row::Parameter)
    return;

  auto const *browsed = browsedParameter ();
  if (browsed == nullptr)
    return;
  auto const &parameter = *browsed;
  if (parameter.isText || parameter.isColour)
    return; // typed or picked, not turned

  auto const stepped
      = stepSkinValue (skinValue (_skin, parameter.path), delta,
                       parameter.isWholeNumber,
                       isColourChannelPath (parameter.path));

  // Held inside whatever range this value has one. In the menu these were
  // named steps and could not fall out of range; as free numbers in the
  // editor they lost that, which is how fontBody became turnable down to 0.01.
  setSkinValue (_skin, parameter.path,
                clampSkinValue (_skin, parameter.path, stepped),
                parameter.isWholeNumber);
  repaint ();

  if (onValueChanged)
    onValueChanged ();
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
            if (onColourPicked)
              onColourPicked (parameter.path);
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

        // A config number is typed, a skin number is dialled — see Numbers.
        if (_numbers == Numbers::Typed)
          {
            beginTypingBrowsedRow ();
            return;
          }

        _editing = !_editing;
        repaint ();
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

bool
SkinEditorComponent::canTurnBrowsedRow () const
{
  if (_naming || browsedRow () != Row::Parameter || _parameters.empty ())
    return false;

  auto const *browsed = browsedParameter ();
  if (browsed == nullptr)
    return false;

  auto const &parameter = *browsed;

  // A config number is typed, not turned — see Numbers.
  return !parameter.isColour && !parameter.isText
         && _numbers == Numbers::Turned;
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

  auto const value = skinValue (_skin, parameter.path);

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
  g.fillAll (toColour (theme ().surface, overlayOpacity));

  if (totalRows () == 0)
    return;

  auto const rows = visibleRows ();
  auto const itemH = static_cast<int> (theme ().fontSize (FontRole::Body) * 1.9f);
  auto const headerH = static_cast<int> (theme ().fontSize (FontRole::Header) * 2.2f);

  auto const panelBounds = listPanelBounds ();

  g.setColour (toColour (theme ().textPrimary, rowWash));
  g.fillRoundedRectangle (panelBounds.toFloat (), 10.f);

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
      g.fillRoundedRectangle (nameRow.toFloat (), 6.f);

      g.setFont (font);

      auto const textArea = nameRow.reduced (10, 0);
      auto const typed = _nameEntry.buffer ().trimEnd ();

      g.setColour (toColour (theme ().textPrimary));
      g.drawText (typed, textArea, juce::Justification::centredLeft, false);

      // The caret sits after everything before it.
      auto const before = juce::GlyphArrangement::getStringWidth (
          font, _nameEntry.buffer ().substring (0, _nameEntry.cursor ()));
      auto const caretW = juce::jmax (
          2.f, juce::GlyphArrangement::getStringWidth (font, "n"));

      g.setColour (toColour (theme ().accent,
                             _editing ? 1.f : theme ().alphaInactive));
      g.fillRect (static_cast<float> (textArea.getX ()) + before,
                  typingCaretY (textArea, font), caretW, 2.f);

      content.removeFromTop (rowGap);
      g.setFont (
          juce::Font (theme ().fontSize (FontRole::Body), juce::Font::plain));
      g.setColour (toColour (theme ().textPrimary, theme ().alphaInactive));
      g.drawText (_editing ? "type, or turn: letter   press: let go   menu: done"
                           : "type, or turn: position   press: change   menu: done",
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
          g.drawText (rowLabel (index), row.reduced (8, 0),
                      juce::Justification::centredLeft, true);
          continue;
        }

      g.setColour (toColour (theme ().textPrimary,
                             isArmed     ? armedRowWash
                             : isBrowsed ? browsedRowWash
                                         : rowWash));
      g.fillRoundedRectangle (row.toFloat (), 6.f);

      auto const valueArea = rowValueArea (row, index);
      auto const nameArea = rowNameArea (row, index);

      g.setFont (
          juce::Font (theme ().fontSize (FontRole::Body), juce::Font::plain));
      g.setColour (isAction && isBrowsed
                       ? toColour (theme ().accent)
                       : toColour (theme ().textPrimary,
                                   isBrowsed ? 1.f
                                             : theme ().alphaInactive));
      g.drawText (rowLabel (index), nameArea, juce::Justification::centredLeft,
                  true);

      auto const shown = rowValue (index);

      // A colour channel shows the colour it is part of, so a number can be
      // judged without leaving the row it sits in.
      if (kind == Row::Parameter
          && _parameters[(size_t)_rows[(size_t)index].parameter].isColour)
        {
          auto const group
              = _parameters[(size_t)_rows[(size_t)index].parameter].path;
          auto swatch = valueArea.reduced (valueArea.getWidth () / 4, 5);
          g.setColour (juce::Colour (
              (juce::uint8)juce::jlimit (0, 255,
                                         (int)skinValue (_skin, group + ".r")),
              (juce::uint8)juce::jlimit (0, 255,
                                         (int)skinValue (_skin, group + ".g")),
              (juce::uint8)juce::jlimit (0, 255,
                                         (int)skinValue (_skin, group + ".b"))));
          g.fillRoundedRectangle (swatch.toFloat (), 3.f);
        }

      g.setFont (
          juce::Font (theme ().fontSize (FontRole::Body), juce::Font::bold));
      g.setColour (isArmed ? toColour (theme ().accent)
                           : toColour (theme ().textPrimary,
                                       isBrowsed ? 1.f
                                                 : theme ().alphaInactive));
      g.drawText (shown, valueArea, juce::Justification::centredRight, true);
    }
}

}
