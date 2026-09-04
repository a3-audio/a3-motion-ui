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
#include <a3-motion-ui/theme/TransportColours.hh>

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

}

ControllerComponent::~ControllerComponent () = default;

void
ControllerComponent::setPadColour (index_t channel, index_t pad,
                                   juce::Colour colour, bool playing)
{
  if (channel >= numChannelColumns || pad >= numPadsPerChannel)
    return;
  if (_padColours[channel][pad] == colour
      && _padPlaying[channel][pad] == playing)
    return;

  _padColours[channel][pad] = colour;
  _padPlaying[channel][pad] = playing;
  repaint (_layout.pads[channel][pad]);
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

  // Written on the lit pad rather than beside it, and in its function's own
  // colour rather than in whichever of black or white the pad leaves readable
  // -- the same red, green and yellow the bar's transport keys use, so the
  // four things you can do to a clip look the same wherever you do them.
  //
  // The pad's colour is the channel's and changes under the word, which a red
  // word on a red pad would not survive, so the word is laid over a dark plate
  // of itself first. Same trick the pictograms use on a lit pad, and for the
  // same reason.
  auto const function = padFunctionByPadIndex[pad];
  auto const playing = _padPlaying[channel][pad];
  auto const ink = padFunctionColour (function, playing);

  g.setFont (juce::Font (
      juce::jmin (theme ().fontSize (FontRole::Body),
                  static_cast<float> (bounds.getHeight ()) * 0.5f),
      juce::Font::plain));

  if (ink.isTransparent ())
    {
      g.setColour (colour.contrasting (0.7f));
      g.drawFittedText (padName (pad), bounds, juce::Justification::centred, 1);
      return;
    }

  // The word on its own dark ground. The pad under it is the channel's colour
  // and the ink is the function's, and those two were chosen for different
  // reasons -- red STOP landed on a red pad and yellow ACT on a yellow one.
  // Moving the ink until it read was the first attempt and it cannot be done:
  // pure red on channel one's pink tops out around 2:1 whatever is done to its
  // brightness, and a red pushed further than that has stopped being red.
  //
  // A plate only as big as the word, so the pad around it is still the block
  // of channel colour you find your deck by.
  auto const plate
      = juce::Rectangle<int> (
            juce::jmin (bounds.getWidth () - 4,
                        static_cast<int> (juce::GlyphArrangement::getStringWidth (
                                              g.getCurrentFont (),
                                              padName (pad)))
                            + bounds.getWidth () / 6),
            juce::jmin (bounds.getHeight () - 4,
                        static_cast<int> (g.getCurrentFont ().getHeight ())
                            + bounds.getHeight () / 10))
            .withCentre (bounds.getCentre ());

  g.setColour (toColour (theme ().surface, 0.88f));
  g.fillRoundedRectangle (plate.toFloat (), 3.f);

  g.setColour (ink);
  g.drawFittedText (padName (pad), plate, juce::Justification::centred, 1);
}


}
