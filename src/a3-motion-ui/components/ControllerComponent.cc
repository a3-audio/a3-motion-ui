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

#include <a3-motion-engine/RecMode.hh>
#include <a3-motion-ui/components/StatusBarLayout.hh>
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
/** A function key belongs to no channel: a neutral skin colour, lifted a
 *  little off the raised surface so it reads as a key and not as a gap. */
constexpr float keyLift = 0.12f;

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
    case PadFunction::Page:      return "PAGE";
    case PadFunction::Action:    return "ACT";
    }

  return "";
}

/** What a function key says on its face: its name, or for the two keys that
 *  carry a value, that value -- as the status bar and the REC page write it. */
juce::String
keyWord (FunctionKey key, FunctionKeyLook const &look)
{
  switch (key)
    {
    case FunctionKey::Tap:       return "TAP";
    case FunctionKey::ClockMode: return clockModeName (look.clockMode);
    case FunctionKey::Record:    return "REC";
    case FunctionKey::RecMode:
      return recModeName (static_cast<RecMode> (look.recMode));
    case FunctionKey::Menu:      return "MENU";
    case FunctionKey::Shift:     return "SHIFT";
    }

  return {};
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

  for (std::size_t i = 0; i < numPanelKeys; ++i)
    {
      auto touch = std::make_unique<TouchControl> ();
      touch->setIdentity (static_cast<int> (i));
      // Down on touch and up on release, never a tap: SHIFT modifies what
      // another finger presses, and an action lasts, for as long as held.
      touch->onPress = [this] (int index, int) {
        auto const k = static_cast<std::size_t> (index);
        setPressed (_keyPressed[k], true, _layout.keys[k]);
        if (onKeyPressed)
          onKeyPressed (endKeyOnPage (k));
      };
      touch->onRelease = [this] (int index, int) {
        auto const k = static_cast<std::size_t> (index);
        setPressed (_keyPressed[k], false, _layout.keys[k]);
        if (onKeyReleased)
          onKeyReleased (endKeyOnPage (k));
      };
      addAndMakeVisible (*touch);
      _keyTouch[i] = std::move (touch);
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
ControllerComponent::setPadPlaying (index_t channel, index_t pad,
                                    bool playing)
{
  if (channel >= numChannelColumns || pad >= numPadsPerChannel)
    return;
  if (_padPlaying[channel][pad] == playing)
    return;

  _padPlaying[channel][pad] = playing;
  repaint (_layout.pads[channel][pad]);
}

void
ControllerComponent::setEndKeyLook (FunctionKeyLook const &keys,
                                    RoomLook const &room)
{
  _keyLook = keys;
  _roomLook = room;
  for (auto const &key : _layout.keys)
    repaint (key);
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
  // The whole area over the sphere: PADS closes with its own key, so no
  // back and close float over it (2026-09-27).
  _layout = layOutController (getLocalBounds (),
                              theme ().fontSize (FontRole::Header),
                              fingertipSize);

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
      _padTouch[channel][pad]->setBounds (_layout.pads[channel][pad]);

  for (std::size_t i = 0; i < numPanelKeys; ++i)
    _keyTouch[i]->setBounds (_layout.keys[i]);
}

void
ControllerComponent::paint (juce::Graphics &g)
{
  // Opaque, like the browser and the big mixer beside it over the sphere. In
  // the bar it did not need this -- the bar painted the ground.
  g.fillAll (toColour (theme ().surface));

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

  for (std::size_t i = 0; i < numPanelKeys; ++i)
    paintKey (g, i);
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

  // Play|Pause wears ❚❚ while its clip runs, as the bar's key does.
  if (function == PadFunction::PlayPause && _padPlaying[channel][pad])
    {
      drawTransportGlyph (g, glyph, TransportFace::Pause);
      return;
    }

  drawTransportGlyph (g, glyph, transportKeyForPad (function));
}

void
ControllerComponent::paintKey (juce::Graphics &g, std::size_t index)
{
  auto const bounds = _layout.keys[index];
  if (bounds.isEmpty ())
    return;

  auto const key = endKeyOnPage (index);
  auto const face = endKeyFace (key, _keyLook.shiftHeld);
  auto const colour = endKeyColour (key, _keyLook, _roomLook);

  // PLAY all and the actions are pads across every channel, so they wear the
  // pad's face -- filled with the colour the panel's LED shows. A function
  // keeps the neutral face the key words have always had: its colour is in
  // the word, the ground says only whether it is doing something. A key with
  // nothing under SHIFT is the bare surface, as dark as its LED.
  auto const neutral = toColour (theme ().surfaceRaised)
                           .interpolatedWith (toColour (theme ().textPrimary),
                                              keyLift);
  auto ground = toColour (theme ().surface);
  if (face.scene)
    ground = colour;
  else if (face.function)
    {
      ground = neutral;
      if (functionKeyLit (*face.function, _keyLook) && !colour.isTransparent ())
        ground = ground.interpolatedWith (colour, theme ().alphaFillEmphasis);
    }
  if (_keyPressed[index])
    ground = ground.interpolatedWith (toColour (theme ().textPrimary),
                                      pressedLift);

  g.setColour (ground);
  g.fillRoundedRectangle (bounds.toFloat (), padCorner);
  g.setColour (toColour (theme ().textPrimary, edgeWash));
  g.drawRoundedRectangle (bounds.toFloat (), padCorner, theme ().strokeThin);

  if (face.function)
    paintKeyWord (g, bounds, *face.function);
  else if (face.scene)
    paintKeyGlyph (g, bounds, *face.scene, ground);
}

void
ControllerComponent::paintKeyWord (juce::Graphics &g,
                                   juce::Rectangle<int> bounds,
                                   FunctionKey key)
{
  auto const tint = functionKeyColour (key, _keyLook);
  auto const inner = bounds.reduced (bounds.getWidth () / 10);
  // The bar's header size, as the status bar's keys are written; a word that
  // is wider than the key is squeezed by drawFittedText, not cut.
  g.setFont (juce::Font (juce::FontOptions (
                             juce::jmin (theme ().fontSize (FontRole::Header),
                                         static_cast<float> (inner.getHeight ())))
                             .withStyle ("Bold")));
  g.setColour (tint.isTransparent () ? toColour (theme ().textPrimary)
                                     : tint);
  g.drawFittedText (keyWord (key, _keyLook), inner,
                    juce::Justification::centred, 1, 0.5f);
}

void
ControllerComponent::paintKeyGlyph (juce::Graphics &g,
                                    juce::Rectangle<int> bounds,
                                    PadFunction scene, juce::Colour ground)
{
  // The mark of the pad it fires across the channels, black or white like
  // every pad's; PLAY all wears ❚❚ while anything plays, since a press then
  // pauses.
  auto const glyph = bounds.toFloat ().withSizeKeepingCentre (
      bounds.getHeight () * 0.32f, bounds.getHeight () * 0.32f);
  g.setColour (padGlyphInk (ground));
  if (scene == PadFunction::PlayPause && _roomLook.anythingPlays)
    {
      drawTransportGlyph (g, glyph, TransportFace::Pause);
      return;
    }
  drawTransportGlyph (g, glyph, transportKeyForPad (scene));
}

}
