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

#include "ColourPickerComponent.hh"

#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
constexpr int padding = 20;
constexpr int hueBarThickness = 30;
constexpr float overlayOpacity = 0.55f;
constexpr float rowWash = 0.063f;
constexpr float browsedRowWash = 0.086f;
constexpr float armedRowWash = 0.133f;

// One detent moves a hundredth of each axis: fine enough to tune, coarse
// enough to cross the range in a few turns.
constexpr float step = 0.01f;
}

ColourPickerComponent::ColourPickerComponent ()
{
  setInterceptsMouseClicks (true, false);
}

void
ColourPickerComponent::setColour (juce::Colour colour,
                                  juce::String const &title)
{
  _colour = colour;
  _title = title;
  _index = 0;
  _editing = false;
  repaint ();
}

void
ColourPickerComponent::setFromHSL (float hue, float saturation,
                                   float lightness)
{
  _colour = juce::Colour::fromHSL (juce::jlimit (0.f, 1.f, hue),
                                   juce::jlimit (0.f, 1.f, saturation),
                                   juce::jlimit (0.f, 1.f, lightness), 1.f);
  repaint ();

  if (onColourChanged)
    onColourChanged ();
}

void
ColourPickerComponent::navigate (int delta)
{
  if (delta == 0)
    return;

  if (!_editing)
    {
      _index = juce::jlimit (0, 2, _index + delta);
      repaint ();
      return;
    }

  auto const move = delta * step;
  auto const hue = _colour.getHue ();
  auto const saturation = _colour.getSaturationHSL ();
  auto const lightness = _colour.getLightness ();

  switch (_index)
    {
    case 0: setFromHSL (hue + move, saturation, lightness); break;
    case 1: setFromHSL (hue, saturation + move, lightness); break;
    default: setFromHSL (hue, saturation, lightness + move); break;
    }
}

void
ColourPickerComponent::toggleEditing ()
{
  _editing = !_editing;
  repaint ();
}

void
ColourPickerComponent::resized ()
{
  // A wide, short card: the field beside its hue bar beside the numbers,
  // rather than stacked. It only covers the bottom of the screen, so the
  // sphere it is changing stays in view — seeing the change is the point.
  auto area = getLocalBounds ().reduced (padding);

  _header = area.removeFromTop (
      static_cast<int> (theme ().fontSize (FontRole::Header) * 1.6f));
  area.removeFromTop (padding / 2);

  _field = area.removeFromLeft (juce::jmin (area.getHeight (),
                                            area.getWidth () / 2));
  area.removeFromLeft (padding / 2);
  _hueBar = area.removeFromLeft (hueBarThickness);
  area.removeFromLeft (padding);
  _rows = area;
}

void
ColourPickerComponent::pickFrom (juce::Point<int> position)
{
  if (_hueBar.contains (position))
    {
      auto const hue
          = (position.getY () - _hueBar.getY ())
            / static_cast<float> (juce::jmax (1, _hueBar.getHeight ()));
      setFromHSL (hue, _colour.getSaturationHSL (), _colour.getLightness ());
      return;
    }

  if (!_field.contains (position))
    return;

  auto const saturation
      = (position.getX () - _field.getX ())
        / static_cast<float> (juce::jmax (1, _field.getWidth ()));
  auto const lightness
      = 1.f
        - (position.getY () - _field.getY ())
              / static_cast<float> (juce::jmax (1, _field.getHeight ()));

  setFromHSL (_colour.getHue (), saturation, lightness);
}

void
ColourPickerComponent::mouseDown (juce::MouseEvent const &event)
{
  pickFrom (event.getPosition ());
}

void
ColourPickerComponent::mouseDrag (juce::MouseEvent const &event)
{
  pickFrom (event.getPosition ());
}

void
ColourPickerComponent::paint (juce::Graphics &g)
{
  g.setColour (toColour (theme ().surface, overlayOpacity));
  g.fillRoundedRectangle (getLocalBounds ().toFloat (), 10.f);

  auto const hue = _colour.getHue ();
  auto const saturation = _colour.getSaturationHSL ();
  auto const lightness = _colour.getLightness ();

  // Title, with the colour itself beside it — the swatch is the answer to
  // "what am I looking at".
  auto header = _header;
  auto swatch = header.removeFromRight (header.getHeight () * 2);
  g.setFont (juce::Font (theme ().fontSize (FontRole::Header), juce::Font::bold));
  g.setColour (toColour (theme ().accent));
  g.drawText (_title, header, juce::Justification::centredLeft, true);
  g.setColour (_colour);
  g.fillRoundedRectangle (swatch.reduced (2).toFloat (), 4.f);

  // The field: saturation across, lightness down, at the chosen hue. Drawn
  // in strips rather than as a gradient, because a gradient can only run one
  // way and this runs two.
  for (int x = 0; x < _field.getWidth (); x += 3)
    {
      auto const s = x / static_cast<float> (juce::jmax (1, _field.getWidth ()));

      // Three stops, not two: lightness 1 is white and 0 is black whatever
      // the saturation, so a two-stop gradient between them runs through
      // grey and the hue never appears. The colour itself lives at the
      // half-way mark.
      auto gradient = juce::ColourGradient (
          juce::Colour::fromHSL (hue, s, 1.f, 1.f),
          static_cast<float> (_field.getX () + x),
          static_cast<float> (_field.getY ()),
          juce::Colour::fromHSL (hue, s, 0.f, 1.f),
          static_cast<float> (_field.getX () + x),
          static_cast<float> (_field.getBottom ()), false);
      gradient.addColour (0.5, juce::Colour::fromHSL (hue, s, 0.5f, 1.f));
      g.setGradientFill (gradient);
      g.fillRect (_field.getX () + x, _field.getY (), 3, _field.getHeight ());
    }

  // Where the current colour sits in it.
  auto const marker = juce::Point<int> (
      _field.getX () + static_cast<int> (saturation * _field.getWidth ()),
      _field.getY () + static_cast<int> ((1.f - lightness) * _field.getHeight ()));
  g.setColour (toColour (theme ().textPrimary));
  g.drawEllipse (marker.getX () - 7.f, marker.getY () - 7.f, 14.f, 14.f, 2.f);

  for (int y = 0; y < _hueBar.getHeight (); y += 2)
    {
      g.setColour (juce::Colour::fromHSL (
          y / static_cast<float> (juce::jmax (1, _hueBar.getHeight ())), 1.f,
          0.5f, 1.f));
      g.fillRect (_hueBar.getX (), _hueBar.getY () + y, _hueBar.getWidth (), 2);
    }

  g.setColour (toColour (theme ().textPrimary));
  g.drawRect (juce::Rectangle<int> (_hueBar.getWidth (), 6)
                  .withCentre ({ _hueBar.getCentreX (),
                                 _hueBar.getY ()
                                     + static_cast<int> (
                                         hue * _hueBar.getHeight ()) }),
              2);

  // H, S and L as numbers, so the encoder has something to aim at and a
  // value can be read off and written down.
  char const *const names[] = { "hue", "saturation", "lightness" };
  float const values[] = { hue, saturation, lightness };
  auto rows = _rows;
  auto const rowH = rows.getHeight () / 3;

  for (int i = 0; i < 3; ++i)
    {
      auto row = rows.removeFromTop (rowH).reduced (0, 2);
      bool const isBrowsed = i == _index;
      bool const isArmed = isBrowsed && _editing;

      g.setColour (toColour (theme ().textPrimary,
                             isArmed     ? armedRowWash
                             : isBrowsed ? browsedRowWash
                                         : rowWash));
      g.fillRoundedRectangle (row.toFloat (), 5.f);

      g.setFont (
          juce::Font (theme ().fontSize (FontRole::Body), juce::Font::plain));
      g.setColour (toColour (theme ().textPrimary,
                             isBrowsed ? 1.f : theme ().alphaInactive));
      g.drawText (names[i], row.reduced (10, 0),
                  juce::Justification::centredLeft, true);

      g.setFont (
          juce::Font (theme ().fontSize (FontRole::Body), juce::Font::bold));
      g.setColour (isArmed ? toColour (theme ().accent)
                           : toColour (theme ().textPrimary,
                                       isBrowsed ? 1.f
                                                 : theme ().alphaInactive));
      g.drawText (juce::String (juce::roundToInt (values[i] * 100)) + "%",
                  row.reduced (10, 0), juce::Justification::centredRight, true);
    }
}

}
