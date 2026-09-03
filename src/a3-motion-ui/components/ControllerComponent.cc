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

namespace a3
{

namespace
{
constexpr float boxWash = 0.06f;
constexpr float edgeWash = 0.18f;
constexpr float padCorner = 4.f;

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
            if (onPadPressed)
              onPadPressed (static_cast<index_t> (c),
                            static_cast<index_t> (p));
          };
          touch->onRelease = [this] (int c, int p) {
            if (onPadReleased)
              onPadReleased (static_cast<index_t> (c),
                             static_cast<index_t> (p));
          };

          addAndMakeVisible (*touch);
          _padTouch[channel][pad] = std::move (touch);
        }
    }

  // Both members are reached through pointers-to-member rather than
  // references. A reference to the parameter would dangle the moment this
  // lambda returned, and the write through it would be silent: the callback
  // still fired, so the readout said the modifier was down while the flag it
  // was supposed to set had never been written.
  auto const modifier
      = [this] (std::unique_ptr<TouchControl> &into,
                bool ControllerComponent::*state,
                std::function<void (bool)> ControllerComponent::*callback) {
          auto const set = [this, state, callback] (bool held) {
            this->*state = held;
            if (this->*callback)
              (this->*callback) (held);
            repaint ();
          };

          into = std::make_unique<TouchControl> ();
          into->onPress = [set] (int, int) { set (true); };
          into->onRelease = [set] (int, int) { set (false); };
          addAndMakeVisible (*into);
        };

  modifier (_shiftTouch, &ControllerComponent::_shiftHeld,
            &ControllerComponent::onShiftHeld);
  modifier (_recordTouch, &ControllerComponent::_recordHeld,
            &ControllerComponent::onRecordHeld);
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
ControllerComponent::setShiftHeld (bool held)
{
  if (_shiftHeld == held)
    return;
  _shiftHeld = held;
  repaint (_layout.shiftButton);
}

void
ControllerComponent::setRecordHeld (bool held)
{
  if (_recordHeld == held)
    return;
  _recordHeld = held;
  repaint (_layout.recordButton);
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

  _shiftTouch->setBounds (_layout.shiftButton);
  _recordTouch->setBounds (_layout.recordButton);
}

void
ControllerComponent::paint (juce::Graphics &g)
{
  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t slot = 0; slot < numPadSlots; ++slot)
      {
        auto const box = _layout.clipBoxes[channel][slot];
        g.setColour (toColour (theme ().textPrimary, boxWash));
        g.fillRoundedRectangle (box.toFloat (), 6.f);
      }

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
      paintPad (g, _layout.pads[channel][pad], channel, pad);

  // Colour, not a shade. A held modifier changed the button by six values out
  // of 255 against the bar's near-black ground — and the thing it changes is
  // what the next pad press does, with Record over Play writing over a take.
  // That is not something to squint at.
  paintModifier (g, _layout.shiftButton, "SHIFT", _shiftHeld,
                 toColour (theme ().accent));
  paintModifier (g, _layout.recordButton, "REC", _recordHeld,
                 toColour (theme ().danger));
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
  auto const colour = _padColours[channel][pad];

  g.setColour (colour);
  g.fillRoundedRectangle (bounds.toFloat (), padCorner);

  g.setColour (toColour (theme ().textPrimary, edgeWash));
  g.drawRoundedRectangle (bounds.toFloat (), padCorner, 1.f);

  // Written on the lit pad rather than beside it, in whichever of black or
  // white the pad's own colour leaves readable — the colour is the state and
  // it changes under the word.
  g.setColour (colour.contrasting (0.7f));
  g.setFont (juce::Font (
      juce::jmin (theme ().fontSize (FontRole::Body),
                  static_cast<float> (bounds.getHeight ()) * 0.5f),
      juce::Font::plain));
  g.drawFittedText (padName (pad), bounds, juce::Justification::centred, 1);
}

void
ControllerComponent::paintModifier (juce::Graphics &g,
                                    juce::Rectangle<int> bounds,
                                    juce::String const &label, bool held,
                                    juce::Colour heldColour)
{
  if (bounds.isEmpty ())
    return;

  g.setColour (held ? heldColour
                    : toColour (theme ().textPrimary, boxWash));
  g.fillRoundedRectangle (bounds.toFloat (), padCorner);
  g.setColour (held ? heldColour.brighter (0.4f)
                    : toColour (theme ().textPrimary, edgeWash));
  g.drawRoundedRectangle (bounds.toFloat (), padCorner, 1.f);

  g.setColour (held ? heldColour.contrasting (0.8f)
                    : toColour (theme ().textPrimary));
  g.setFont (juce::Font (theme ().fontSize (FontRole::Body),
                         juce::Font::plain));
  g.drawFittedText (label, bounds, juce::Justification::centred, 1);
}

}
