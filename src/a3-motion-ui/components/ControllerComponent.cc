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

#include "ControllerComponent.hh"

#include <a3-motion-ui/io/PadFunctions.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>
#include <a3-motion-ui/theme/TransportLook.hh>

namespace a3
{

namespace
{
constexpr float boxWash = 0.06f;
constexpr float edgeWash = 0.18f;
constexpr float padCorner = 4.f;
/** How far a pad under a finger runs towards the skin's text colour (white
 *  on a shipped device). Enough to be seen from
 *  the corner of an eye, not so much that the channel colour is lost. */
constexpr float pressedLift = 0.35f;
/** A scene pad belongs to no channel: a neutral skin colour, lifted a little
 *  off the raised surface so it reads as a pad and not as a gap. */
constexpr float sceneLift = 0.12f;

/** What a pad is called on the screen. The panel says it with a position and
 *  a colour; here there is room for a word, and a word beats a glyph nobody
 *  has been taught — this page exists for the build with no panel to learn
 *  from. */
char const *
padName (index_t pad)
{
  switch (padFunctionByPadIndex[pad])
    {
    case PadFunction::PlayPause: return "PLAY";
    case PadFunction::Stop:      return "STOP";
    case PadFunction::Action:    return "ACT";
    case PadFunction::Settings:  return "SET";
    }

  return "";
}
}

ControllerComponent::ControllerComponent ()
{
  setInterceptsMouseClicks (false, true);

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    {
      _padColours[channel].fill (juce::Colours::transparentBlack);

      for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
        {
          auto touch = std::make_unique<TouchControl> ();
          touch->setIdentity (static_cast<int> (channel),
                              static_cast<int> (pad));

          // On press, not on tap: a pad fires when it is touched, the way the
          // panel's does. Waiting for the finger to come up again would put
          // the whole gesture a reaction time late, and this is the one place
          // where late is wrong (see the beat clock).
          touch->onPress = [this] (int c, int p) {
            setPressed (_padPressed[static_cast<index_t> (c)]
                                   [static_cast<index_t> (p)],
                        true, _layout.pads[static_cast<index_t> (c)]
                                          [static_cast<index_t> (p)]);
            if (onPadPressed)
              onPadPressed (static_cast<index_t> (c),
                            static_cast<index_t> (p));
          };
          touch->onRelease = [this] (int c, int p) {
            setPressed (_padPressed[static_cast<index_t> (c)]
                                   [static_cast<index_t> (p)],
                        false, _layout.pads[static_cast<index_t> (c)]
                                           [static_cast<index_t> (p)]);
            if (onPadReleased)
              onPadReleased (static_cast<index_t> (c),
                             static_cast<index_t> (p));
          };

          addAndMakeVisible (*touch);
          _padTouch[channel][pad] = std::move (touch);
        }
    }

  for (index_t slot = 0; slot < numPadSlots; ++slot)
    for (std::size_t row = 0; row < numSceneRows; ++row)
      {
        auto touch = std::make_unique<TouchControl> ();
        touch->setIdentity (static_cast<int> (slot), static_cast<int> (row));
        // On press, like the pads: a scene is fired when it is touched.
        touch->onPress = [this] (int s, int r) {
          auto const slotIndex = static_cast<index_t> (s);
          auto const rowIndex = static_cast<std::size_t> (r);
          setPressed (_scenePressed[slotIndex][rowIndex], true,
                      _layout.scenes[slotIndex][rowIndex]);
          if (onScenePressed)
            onScenePressed (slotIndex, rowIndex);
        };
        touch->onRelease = [this] (int s, int r) {
          auto const slotIndex = static_cast<index_t> (s);
          auto const rowIndex = static_cast<std::size_t> (r);
          setPressed (_scenePressed[slotIndex][rowIndex], false,
                      _layout.scenes[slotIndex][rowIndex]);
          if (onSceneReleased)
            onSceneReleased (slotIndex, rowIndex);
        };
        addAndMakeVisible (*touch);
        _sceneTouch[slot][row] = std::move (touch);
      }

}

ControllerComponent::~ControllerComponent () = default;

void
ControllerComponent::setPadColour (index_t channel, index_t pad,
                                   juce::Colour colour)
{
  if (channel >= numChannelColumns || pad >= numPadsPerChannel)
    return;
  if (_padColours[channel][pad] == colour)
    return;

  _padColours[channel][pad] = colour;
  repaint (_layout.pads[channel][pad]);
}

void
ControllerComponent::setPressed (bool &pressed, bool down,
                                 juce::Rectangle<int> area)
{
  if (pressed == down)
    return;
  pressed = down;
  repaint (area);
}

void
ControllerComponent::applyTheme ()
{
  resized ();
  repaint ();
}

void
ControllerComponent::resized ()
{
  _layout = layOutController (getLocalBounds (),
                              theme ().fontSize (FontRole::Header),
                              fingertipSize);

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
      _padTouch[channel][pad]->setBounds (_layout.pads[channel][pad]);

  for (index_t slot = 0; slot < numPadSlots; ++slot)
    for (std::size_t row = 0; row < numSceneRows; ++row)
      _sceneTouch[slot][row]->setBounds (_layout.scenes[slot][row]);
}

void
ControllerComponent::paint (juce::Graphics &g)
{
  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t slot = 0; slot < numPadSlots; ++slot)
      {
        auto const box = _layout.clipBoxes[channel][slot];
        g.setColour (toColour (theme ().textPrimary, boxWash));
        g.fillRoundedRectangle (box.toFloat (), theme ().radiusRow);
      }

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
      paintPad (g, _layout.pads[channel][pad], channel, pad);

  for (index_t slot = 0; slot < numPadSlots; ++slot)
    for (std::size_t row = 0; row < numSceneRows; ++row)
      paintScene (g, slot, row);

}

void
ControllerComponent::paintPad (juce::Graphics &g, juce::Rectangle<int> bounds,
                               index_t channel, index_t pad)
{
  if (bounds.isEmpty ())
    return;

  // The colour is the panel's, worked out by padLEDCallback() — empty, idle,
  // armed and running look here exactly as they look on the hardware, and
  // there is one place that decides what that means.
  // Lifted while a finger is on it: a press is seen at once, whatever it
  // goes on to do to the slot.
  auto const colour
      = _padPressed[channel][pad]
            ? _padColours[channel][pad].interpolatedWith (toColour (theme ().textPrimary),
                                                          pressedLift)
            : _padColours[channel][pad];

  g.setColour (colour);
  g.fillRoundedRectangle (bounds.toFloat (), padCorner);

  g.setColour (toColour (theme ().textPrimary, edgeWash));
  g.drawRoundedRectangle (bounds.toFloat (), padCorner, theme ().strokeThin);

  // The same marks the bar's transport keys use -- a circle, a square, a
  // triangle or two bars -- rather than the words they used to be. A shape is
  // read without being read, which is the point on a page you hit while
  // looking at the room, and it survives a channel-coloured pad far better
  // than four letters do: the word needed a plate behind it to be legible at
  // all, and a plate on every pad ate into the block of channel colour you
  // find your deck by.
  auto const function = padFunctionByPadIndex[pad];

  // A third of the pad rather than nearer half: the mark is what the pad is
  // for, but the pad's colour is which channel it belongs to, and a mark that
  // fills it leaves less of that colour to find the deck by.
  auto const glyph = bounds.toFloat ().withSizeKeepingCentre (
      bounds.getHeight () * 0.32f, bounds.getHeight () * 0.32f);

  // Black or white, whichever the pad lets stand out -- see padGlyphInk().
  g.setColour (padGlyphInk (colour));

  // Settings opens a menu, and a menu's mark is three bars.
  if (!hasTransportGlyph (function))
    {
      drawMenuGlyph (g, glyph);
      return;
    }

  drawTransportGlyph (g, glyph, transportKeyForPad (function));
}

void
ControllerComponent::paintScene (juce::Graphics &g, index_t slot,
                                 std::size_t row)
{
  auto const bounds = _layout.scenes[slot][row];
  if (bounds.isEmpty ())
    return;

  auto const neutral = toColour (theme ().surfaceRaised)
                           .interpolatedWith (toColour (theme ().textPrimary),
                                              sceneLift);
  auto const ground
      = _scenePressed[slot][row]
            ? neutral.interpolatedWith (toColour (theme ().textPrimary), pressedLift)
            : neutral;

  g.setColour (ground);
  g.fillRoundedRectangle (bounds.toFloat (), padCorner);
  g.setColour (toColour (theme ().textPrimary, edgeWash));
  g.drawRoundedRectangle (bounds.toFloat (), padCorner, theme ().strokeThin);

  // The mark of the row it fires, black or white like every pad's.
  auto const function = sceneRowFunction[row];
  auto const glyph = bounds.toFloat ().withSizeKeepingCentre (
      bounds.getHeight () * 0.32f, bounds.getHeight () * 0.32f);
  g.setColour (padGlyphInk (ground));
  drawTransportGlyph (g, glyph, transportKeyForPad (function));
}


}
