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
#include <a3-motion-ui/components/ControllerLayout.hh>

namespace a3
{

namespace
{
constexpr int padding = 20;
constexpr float rowWash = 0.063f;
constexpr float browsedRowWash = 0.086f;
constexpr float armedRowWash = 0.133f;

// One detent moves a hundredth of each axis: fine enough to tune, coarse
// enough to cross the range in a few turns.
constexpr float step = 0.01f;
}

ColourPickerComponent::ColourPickerComponent ()
{
  setInterceptsMouseClicks (true, true);

  // Only the colourspace: no sliders, no swatches, no alpha, no colour at the
  // top -- we draw the swatch beside the title ourselves. See the header for
  // why the rest of ColourSelector stays off.
  _selector = std::make_unique<juce::ColourSelector> (
      juce::ColourSelector::showColourspace, 0, 0);
  _selector->setColour (juce::ColourSelector::backgroundColourId,
                        juce::Colours::transparentBlack);
  _selector->addChangeListener (this);

  // ... and its mouse events, because the change message is a tick late and
  // coalesces. true: the surface's own children are what a finger actually
  // lands on.
  _selector->addMouseListener (this, true);

  addAndMakeVisible (*_selector);
}

void
ColourPickerComponent::setColour (juce::Colour colour,
                                  juce::String const &title)
{
  _colour = colour;
  _title = title;
  _index = 0;
  _editing = false;

  if (_selector)
    _selector->setCurrentColour (colour, juce::dontSendNotification);

  repaint ();
}

void
ColourPickerComponent::setFromHSL (float hue, float saturation,
                                   float lightness)
{
  _colour = juce::Colour::fromHSL (juce::jlimit (0.f, 1.f, hue),
                                   juce::jlimit (0.f, 1.f, saturation),
                                   juce::jlimit (0.f, 1.f, lightness),
                                   _colour.getFloatAlpha ());

  // The rows and the surface are two views of one colour, so a row moving
  // has to move the surface's marker with it. Without a notification, or the
  // selector would tell us what we just told it and we would go round.
  if (_selector)
    _selector->setCurrentColour (_colour, juce::dontSendNotification);

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
  _doneButton = _header.removeFromRight (_header.getHeight () * 3);
  _header.removeFromRight (padding / 2);
  area.removeFromTop (padding / 2);

  // The selector takes the field's old room and the strip's together: it
  // lays its own two out inside, hue bar included, and gives the strip
  // min(50, 15% of its width) -- above a fingertip at every size this card
  // is given.
  auto const surface
      = area.removeFromLeft (juce::jmin (area.getHeight () * 3 / 2,
                                         area.getWidth () * 2 / 3));
  if (_selector)
    _selector->setBounds (surface);

  area.removeFromLeft (padding);
  _rows = area;
}


void
ColourPickerComponent::mouseDown (juce::MouseEvent const &event)
{
  // Forwarded from the picking surface: read the colour now.
  if (event.eventComponent != this)
    takeColourFromSurface ();

  // Our own: the done key is decided on release, so a slip off it is a change
  // of mind.
}

void
ColourPickerComponent::mouseUp (juce::MouseEvent const &event)
{
  // A forwarded event's position is in *its* component's coordinates, so it
  // must never be tested against our rectangles -- the done key sits where
  // the surface's bottom left is.
  if (event.eventComponent != this)
    {
      takeColourFromSurface ();
      return;
    }

  if (_doneButton.contains (event.getPosition ()) && onDone)
    onDone ();
}

void
ColourPickerComponent::mouseDrag (juce::MouseEvent const &event)
{
  if (event.eventComponent != this)
    takeColourFromSurface ();
}

void
ColourPickerComponent::changeListenerCallback (juce::ChangeBroadcaster *)
{
  takeColourFromSurface ();
}

void
ColourPickerComponent::takeColourFromSurface ()
{
  if (!_selector)
    return;

  // Keep the alpha we were handed: the selector was built without
  // showAlphaChannel, so it knows nothing about ours and would hand back an
  // opaque colour for one that was written translucent.
  auto const picked = _selector->getCurrentColour ();
  auto const next = picked.withAlpha (_colour.getFloatAlpha ());
  if (next == _colour)
    return;

  _colour = next;
  repaint ();

  if (onColourChanged)
    onColourChanged ();
}

void
ColourPickerComponent::paint (juce::Graphics &g)
{
  g.setColour (toColour (theme ().surface, theme ().overlayOpacity));
  g.fillRoundedRectangle (getLocalBounds ().toFloat (), theme ().radiusPanel);

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
  g.fillRoundedRectangle (
      swatch.reduced (juce::roundToInt (theme ().paddingTight)).toFloat (),
      theme ().radiusControl);

  // Done, as something to touch. The change is already in force — this is
  // the way back to the list, not a commit.
  g.setColour (toColour (theme ().textPrimary, armedRowWash));
  g.fillRoundedRectangle (_doneButton.toFloat (), theme ().radiusRow);
  g.setColour (toColour (theme ().accent));
  g.setFont (juce::Font (theme ().fontSize (FontRole::Body), juce::Font::bold));
  g.drawText ("done", _doneButton, juce::Justification::centred, false);

  // The field and the hue strip are juce::ColourSelector's, drawn by it as a
  // child of this one. What is left here is the card around them: the title,
  // the swatch, the done key and the three numbers.

  // H, S and L as numbers, so the encoder has something to aim at and a
  // value can be read off and written down.
  char const *const names[] = { "hue", "saturation", "lightness" };
  float const values[] = { hue, saturation, lightness };
  auto rows = _rows;
  auto const rowH = rows.getHeight () / 3;

  for (int i = 0; i < 3; ++i)
    {
      auto row = rows.removeFromTop (rowH).reduced (
          0, juce::roundToInt (theme ().paddingTight));
      bool const isBrowsed = i == _index;
      bool const isArmed = isBrowsed && _editing;

      g.setColour (toColour (theme ().textPrimary,
                             isArmed     ? armedRowWash
                             : isBrowsed ? browsedRowWash
                                         : rowWash));
      g.fillRoundedRectangle (row.toFloat (), theme ().radiusRow);

      g.setFont (
          juce::Font (theme ().fontSize (FontRole::Body), juce::Font::plain));
      // Full opacity for the browsed row's name rather than an alpha rung:
      // "browsed" has always meant no dimming at all, which the alpha-less
      // overload already says. This used to be `isBrowsed ? 1.f : theme
      // ().alphaInactive`; 1.f fits no rung, and full opacity is the absence
      // of an emphasis decision rather than one of its rungs, so it
      // deliberately gets no role of its own. See
      // issues/a3-motion-ui-metric-role-deviations.md (Task 16).
      g.setColour (isBrowsed ? toColour (theme ().textPrimary)
                            : toColour (theme ().textPrimary,
                                       theme ().alphaInactive));
      g.drawText (names[i], row.reduced (juce::roundToInt (theme ().padding), 0),
                  juce::Justification::centredLeft, true);

      g.setFont (
          juce::Font (theme ().fontSize (FontRole::Body), juce::Font::bold));
      // Same restructuring as above for the browsed row's value.
      g.setColour (isArmed ? toColour (theme ().accent)
                          : (isBrowsed
                                 ? toColour (theme ().textPrimary)
                                 : toColour (theme ().textPrimary,
                                            theme ().alphaInactive)));
      g.drawText (juce::String (juce::roundToInt (values[i] * 100)) + "%",
                  row.reduced (juce::roundToInt (theme ().padding), 0),
                  juce::Justification::centredRight, true);
    }
}

}
