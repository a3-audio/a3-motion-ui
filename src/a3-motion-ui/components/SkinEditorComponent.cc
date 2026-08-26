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
  _skin = std::move (skin);
  _name = name;
  _parameters = skinParameters (_skin);
  _index = juce::jlimit (0, juce::jmax (0, (int)_parameters.size () - 1), _index);
  _editing = false;
  repaint ();
}

void
SkinEditorComponent::navigate (int delta)
{
  if (_parameters.empty () || delta == 0)
    return;

  if (!_editing)
    {
      _index = juce::jlimit (0, (int)_parameters.size () - 1, _index + delta);
      repaint ();
      return;
    }

  auto const &parameter = _parameters[(size_t)_index];
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
  if (_parameters.empty ())
    return;

  _editing = !_editing;
  repaint ();
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

  if (_parameters.empty ())
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
  g.drawText ("Skin: " + _name, headerArea, juce::Justification::centredLeft,
              true);
  g.setColour (toColour (theme ().textPrimary, theme ().alphaInactive));
  g.setFont (juce::Font (theme ().fontSize (FontRole::Body), juce::Font::plain));
  g.drawText (juce::String (_index + 1) + " / "
                  + juce::String ((int)_parameters.size ()),
              headerArea, juce::Justification::centredRight, true);

  // The list is far longer than the screen, so it scrolls around the
  // selected row rather than paging — the row being edited stays put while
  // its value changes.
  auto const first = juce::jlimit (
      0, juce::jmax (0, (int)_parameters.size () - rows), _index - rows / 2);

  for (int i = 0; i < rows; ++i)
    {
      auto const index = first + i;
      if (index >= (int)_parameters.size ())
        break;

      auto row = content.removeFromTop (itemH);
      if (i < rows - 1)
        content.removeFromTop (rowGap);

      auto const &parameter = _parameters[(size_t)index];
      bool const isBrowsed = index == _index;
      bool const isArmed = isBrowsed && _editing;

      g.setColour (toColour (theme ().textPrimary,
                             isArmed     ? armedRowWash
                             : isBrowsed ? browsedRowWash
                                         : rowWash));
      g.fillRoundedRectangle (row.toFloat (), 6.f);

      auto valueArea = row.removeFromRight (row.getWidth () / 3).reduced (8, 0);
      auto nameArea = row.reduced (8, 0);

      g.setFont (
          juce::Font (theme ().fontSize (FontRole::Body), juce::Font::plain));
      g.setColour (toColour (theme ().textPrimary,
                             isBrowsed ? 1.f : theme ().alphaInactive));
      g.drawText (parameter.path, nameArea, juce::Justification::centredLeft,
                  true);

      auto const value = skinValue (_skin, parameter.path);
      auto const shown = parameter.isWholeNumber
                             ? juce::String ((int)std::lround (value))
                             : juce::String (value, 3);

      // A colour channel shows the colour it is part of, so a number can be
      // judged without leaving the row it sits in.
      if (isColourChannelPath (parameter.path))
        {
          auto const group = parameter.path.upToLastOccurrenceOf (".", false,
                                                                  false);
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
