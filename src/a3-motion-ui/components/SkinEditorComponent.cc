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
  setInterceptsMouseClicks (false, false);
}

void
SkinEditorComponent::setSkin (juce::var skin, juce::String const &name)
{
  setDocument (std::move (skin), name, true);
}

void
SkinEditorComponent::setDocument (juce::var document, juce::String const &title,
                                  bool withSkinActions)
{
  _actionRows = withSkinActions ? 3 : 0;
  _skin = std::move (document);
  _name = title;
  _parameters = skinParameters (_skin);
  // Every page opens at its top: carrying a row number over from another
  // document lands on whatever happens to sit at that number.
  _index = 0;
  _editing = false;
  _naming = false;
  _deleteAsked = false;
  _textPath = {};
  repaint ();
}

int
SkinEditorComponent::totalRows () const
{
  return _actionRows + (int)_parameters.size ();
}

SkinEditorComponent::Row
SkinEditorComponent::browsedRow () const
{
  // Keyed on how many action rows this document has, not on the index alone:
  // a page without them starts at its first parameter.
  if (_index >= _actionRows)
    return Row::Parameter;

  switch (_index)
    {
    case 0: return Row::SaveAsNew;
    case 1: return Row::Rename;
    default: return Row::Delete;
    }
}

void
SkinEditorComponent::finishNaming ()
{
  if (!_naming)
    return;

  auto const typed = _nameEntry.name ();
  auto const path = _textPath;
  _naming = false;
  _editing = false;
  _textPath = {};
  repaint ();

  if (onNamingChanged)
    onNamingChanged (false);

  if (path.isNotEmpty ())
    {
      // A text parameter, not the document's own name.
      setSkinText (_skin, path, typed);
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
      _index = juce::jlimit (0, totalRows () - 1, _index + delta);
      // Turning away is how a delete is called off — it never waits around
      // for a press that was meant for something else.
      _deleteAsked = false;
      repaint ();
      return;
    }

  if (browsedRow () != Row::Parameter)
    return;

  auto const &parameter = _parameters[(size_t)(_index - _actionRows)];
  if (parameter.isText)
    return; // typed, not turned

  auto const stepped
      = stepSkinValue (skinValue (_skin, parameter.path), delta,
                       parameter.isWholeNumber,
                       isColourChannelPath (parameter.path));

  setSkinValue (_skin, parameter.path, stepped, parameter.isWholeNumber);
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
    case Row::SaveAsNew:
      if (onSaveAsNew)
        onSaveAsNew ();
      return;

    case Row::Rename:
      _nameEntry = TextInput{ _name };
      _naming = true;
      _editing = false;
      repaint ();
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

    case Row::Parameter:
      {
        if (_parameters.empty ())
          return;

        auto const &parameter = _parameters[(size_t)(_index - _actionRows)];
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
            _editing = false;
            repaint ();
            if (onNamingChanged)
              onNamingChanged (true);
            return;
          }

        _editing = !_editing;
        repaint ();
        return;
      }
    }
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
  if (index >= _actionRows)
    return _parameters[(size_t)(index - _actionRows)].path;

  switch (index)
    {
    case 0: return juce::String::fromUTF8 ("\xc2\xbb Save as new");
    case 1: return juce::String::fromUTF8 ("\xc2\xbb Rename");
    default: return juce::String::fromUTF8 ("\xc2\xbb Delete");
    }
}

juce::String
SkinEditorComponent::rowValue (int index) const
{
  if (index < _actionRows)
    return (index == 2 && _deleteAsked) ? "sure?" : "";

  auto const &parameter = _parameters[(size_t)(index - _actionRows)];
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

  auto const panelW = juce::jmin (maxPanelW, getWidth () - 2 * paddingH);
  auto const panelH
      = paddingV * 2 + headerH + rows * itemH + (rows - 1) * rowGap;
  auto panelBounds
      = juce::Rectangle<int> ((getWidth () - panelW) / 2,
                              (getHeight () - panelH) / 2, panelW, panelH);

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
      auto nameRow = content.removeFromTop (itemH * 2);
      g.setColour (toColour (theme ().textPrimary, browsedRowWash));
      g.fillRoundedRectangle (nameRow.toFloat (), 6.f);

      auto const cellW = nameRow.getWidth () / TextInput::maxLength;
      auto const buffer = _nameEntry.buffer ();

      for (int i = 0; i < TextInput::maxLength; ++i)
        {
          auto cell = nameRow.removeFromLeft (cellW);
          bool const atCursor = i == _nameEntry.cursor ();

          if (atCursor)
            {
              g.setColour (toColour (theme ().textPrimary,
                                     _editing ? armedRowWash : rowWash));
              g.fillRoundedRectangle (cell.reduced (2).toFloat (), 3.f);
            }

          g.setFont (juce::Font (theme ().fontSize (FontRole::Header),
                                 juce::Font::bold));
          g.setColour (atCursor && _editing
                           ? toColour (theme ().accent)
                           : toColour (theme ().textPrimary,
                                       atCursor ? 1.f
                                                : theme ().alphaInactive));
          g.drawText (juce::String::charToString (buffer[i]), cell,
                      juce::Justification::centred, false);
        }

      content.removeFromTop (rowGap);
      g.setFont (
          juce::Font (theme ().fontSize (FontRole::Body), juce::Font::plain));
      g.setColour (toColour (theme ().textPrimary, theme ().alphaInactive));
      g.drawText (_editing ? "turn: letter   press: let go   menu: done"
                           : "turn: position   press: change   menu: done",
                  content.removeFromTop (itemH),
                  juce::Justification::centredLeft, true);
      return;
    }

  auto const first = juce::jlimit (
      0, juce::jmax (0, totalRows () - rows), _index - rows / 2);

  for (int i = 0; i < rows; ++i)
    {
      auto const index = first + i;
      if (index >= totalRows ())
        break;

      auto row = content.removeFromTop (itemH);
      if (i < rows - 1)
        content.removeFromTop (rowGap);

      bool const isAction = index < _actionRows;
      bool const isBrowsed = index == _index;
      bool const isArmed = isBrowsed && _editing;

      g.setColour (toColour (theme ().textPrimary,
                             isArmed     ? armedRowWash
                             : isBrowsed ? browsedRowWash
                                         : rowWash));
      g.fillRoundedRectangle (row.toFloat (), 6.f);

      auto const valueShare
          = (!isAction && rowValue (index).length () > 8) ? 2 : 3;
      auto valueArea
          = row.removeFromRight (row.getWidth () / valueShare).reduced (8, 0);
      auto nameArea = row.reduced (8, 0);

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
      if (!isAction
          && isColourChannelPath (_parameters[(size_t)(index - _actionRows)]
                                      .path))
        {
          auto const group
              = _parameters[(size_t)(index - _actionRows)]
                    .path.upToLastOccurrenceOf (".", false, false);
          auto swatch = valueArea.removeFromLeft (itemH / 2).reduced (0, 6);
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
