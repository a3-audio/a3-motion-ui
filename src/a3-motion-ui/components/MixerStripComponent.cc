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

#include "MixerStripComponent.hh"

#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/components/MixerComponent.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

MixerStripComponent::MixerStripComponent (MixerState &state,
                                          VuLevels const &levels)
    : _state (state), _levels (levels)
{
  setInterceptsMouseClicks (false, true);

  for (int i = 0; i < numMixerControls; ++i)
    {
      auto const index = static_cast<std::size_t> (i);
      auto touch = std::make_unique<TouchControl> ();

      // Where the control sits in mixerControlOrder, and nothing else -- the
      // channel is the component's and is read at the moment of the gesture,
      // so a face tapped in the header swaps the strip without seven hit
      // areas having to be told about it.
      touch->setIdentity (i);

      // The accumulator is TouchControl's own, fed from the skin's
      // touchDragPixelsPerStep, so how far a finger travels for one step
      // stays one screw in the skin editor rather than becoming a second one
      // here.
      touch->onDragIncrement = [this] (int primary, int, int increment) {
        if (onChannelDragged)
          onChannelDragged (
              _channel, mixerControlOrder[static_cast<std::size_t> (primary)],
              increment);
      };

      // Only the two-valued controls answer a tap. A continuous value is
      // dragged and never tapped -- a tap has no direction, so there is
      // nothing for it to say about a level.
      if (mixerControlIsAToggle (mixerControlOrder[index]))
        touch->onTap = [this] (int primary, int) {
          if (onChannelTapped)
            onChannelTapped (
                _channel,
                mixerControlOrder[static_cast<std::size_t> (primary)]);
        };

      addAndMakeVisible (*touch);
      _touch[index] = std::move (touch);
    }

  applyTheme ();
}

MixerStripComponent::~MixerStripComponent () = default;

void
MixerStripComponent::visibilityChanged ()
{
  if (isVisible ())
    startTimerHz (vuMeterRefreshHz);
  else
    stopTimer ();
}

void
MixerStripComponent::timerCallback ()
{
  // The same guard the overlay's timer carries, for the same reason: a tab
  // too small to lay out draws a sentence and no meter, so there is nothing
  // here to keep up to date.
  if (!_layout.fits)
    return;

  repaint (_layout.channelMeter[0]);
}

void
MixerStripComponent::setChannel (int channel)
{
  if (channel < 0 || channel >= numChannelsInitial || channel == _channel)
    return;

  _channel = channel;
  repaint ();
}

int
MixerStripComponent::getChannel () const
{
  return _channel;
}

void
MixerStripComponent::applyTheme ()
{
  // The knob's diameter is the skin's pot size, and the layout's floor for a
  // cell is that diameter -- so a skin change moves every rectangle here, not
  // only their colours.
  _metrics = ControlMetrics{
    knobDiameterForFont (theme ().fontSize (FontRole::Body), theme ().potSize),
    // One size for both the caption and the value, as in the overlay: a cell
    // here is a column of its own with a four-character word in it, so the
    // body size the skin asks for is what it gets.
    theme ().fontSize (FontRole::Body),
    theme ().fontSize (FontRole::Body),
  };

  resized ();
  repaint ();
}

void
MixerStripComponent::resized ()
{
  _layout = layOutMixerStrip (getLocalBounds (), _metrics);

  // The same rule the overlay follows: a strip drawing "not enough room" must
  // not go on answering drags across that sentence. It matters more here
  // rather than less -- the bar stays on screen underneath the overlay, so
  // this page is reachable even while the mixer is up.
  for (std::size_t i = 0; i < static_cast<std::size_t> (numMixerControls);
       ++i)
    {
      _touch[i]->setBounds (_layout.controls[0][i]);
      _touch[i]->setVisible (_layout.fits);
    }
}

void
MixerStripComponent::paint (juce::Graphics &g)
{
  // No ground of its own. The bar has already filled this area with its own
  // surface, and a second panel over it would make the page read as an
  // overlay laid on the bar rather than as one of its views -- which is the
  // whole distinction between this and the MIX key in the status bar.

  if (!_layout.fits)
    {
      // One line of text rather than targets nobody can hit, the same answer
      // the overlay gives: a strip that cannot be operated is worse than a
      // sentence saying so, because the sentence can be acted on.
      g.setColour (toColour (theme ().textMuted));
      g.setFont (
          juce::Font (juce::FontOptions (theme ().fontSize (FontRole::Body))));
      g.drawFittedText ("Not enough room for the strip", getLocalBounds (),
                        juce::Justification::centred, 1);
      return;
    }

  auto const colour = toColour (theme ().channel[_channel]);

  for (std::size_t i = 0; i < static_cast<std::size_t> (numMixerControls);
       ++i)
    {
      auto const control = mixerControlOrder[i];
      paintMixerChannelControl (g, _layout.controls[0][i], _metrics, colour,
                                control,
                                _state.channelValue (_channel, control),
                                _state.channelToggle (_channel, control));
    }

  // The shown channel's input meter, beside its level -- the same picture the
  // overlay draws, from the same store, so the two pages cannot disagree
  // about how loud a deck is.
  paintVuMeter (g, _layout.channelMeter[0], colour,
                _levels.channel (_channel, vuNowMs ()));
}

}
