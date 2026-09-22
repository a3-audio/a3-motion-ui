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

  auto const dragged = [this] (MixerControl control, int increment) {
    if (onChannelDragged)
      onChannelDragged (_channel, control, increment);
  };

  for (int i = 0; i < numMixerFaceControls; ++i)
    {
      auto const index = static_cast<std::size_t> (i);
      auto touch = std::make_unique<TouchControl> ();

      // Where the control sits in mixerFaceOrder, and nothing else -- the
      // channel is the component's and is read at the moment of the gesture,
      // so a face tapped in the header swaps the strip without seven hit
      // areas having to be told about it. Which is why _channel is read
      // inside these two rather than closed over: the overlay's strips each
      // stand for one channel for good, and this one does not.
      touch->setIdentity (i);

      wireMixerChannelTouch (
          *touch, mixerFaceOrder[index], dragged,
          [this] (MixerControl control) {
            if (onChannelTapped)
              onChannelTapped (_channel, control);
          },
          [this] (MixerControl control) {
            if (onChannelDoubleTapped)
              onChannelDoubleTapped (_channel, control);
          });

      addAndMakeVisible (*touch);
      _touch[index] = std::move (touch);
    }

  // The meter first, the fader after it: the handle is the layer above, and
  // the meter paints itself, so its twenty five refreshes a second never
  // reach this page.
  _meterView = std::make_unique<VuMeterView> ();
  _meterView->level = [this] { return _levels.channel (_channel, vuNowMs ()); };
  addAndMakeVisible (*_meterView);

  // The meter is VOL: a fader over it, JUCE's slider doing the drag, and two
  // taps ask for full volume -- see VuFader.
  _fader = std::make_unique<VuFader> ();
  _fader->onValueChange = [this] {
    if (onMeterDraggedTo)
      onMeterDraggedTo (_channel, static_cast<float> (_fader->getValue ()));
  };
  _fader->onDoubleTapped = [this] {
    if (onMeterDoubleTapped)
      onMeterDoubleTapped (_channel);
  };
  addAndMakeVisible (*_fader);

  applyTheme ();
}

MixerStripComponent::~MixerStripComponent () = default;

void
MixerStripComponent::visibilityChanged ()
{
  runMeterTimerWhileVisible (isVisible (), *this);
}

void
MixerStripComponent::timerCallback ()
{
  // The overlay's, and the strip leaves the rectangles it has no meter for
  // empty -- so this asks for one bar and the overlay for nine, out of one
  // loop.
  _meterView->repaint ();
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
  _metrics = mixerControlMetrics ();

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
  for (std::size_t i = 0; i < static_cast<std::size_t> (numMixerFaceControls);
       ++i)
    {
      _touch[i]->setBounds (_layout.controls[0][i]);
      _touch[i]->setVisible (_layout.fits);
    }

  _meterView->setBounds (_layout.channelMeter[0]);
  _meterView->setVisible (_layout.fits);
  _fader->setBounds (_layout.channelMeter[0]);
  _fader->setVisible (_layout.fits);
}

void
MixerStripComponent::syncFader ()
{
  _fader->setValue (_state.channelValue (_channel, MixerControl::Volume),
                    juce::dontSendNotification);
  _fader->setHandleColour (toColour (theme ().channel[_channel]));
}

void
MixerStripComponent::paint (juce::Graphics &g)
{
  syncFader ();

  // No ground of its own. The bar has already filled this area with its own
  // surface, and a second panel over it would make the page read as an
  // overlay laid on the bar rather than as one of its views -- which is the
  // whole distinction between this and the MIX key in the status bar.

  if (!_layout.fits)
    {
      paintMixerHasNoRoom (g, getLocalBounds (), "the strip");
      return;
    }

  auto const colour = toColour (theme ().channel[_channel]);

  for (std::size_t i = 0; i < static_cast<std::size_t> (numMixerFaceControls);
       ++i)
    {
      auto const control = mixerFaceOrder[i];
      paintMixerChannelControl (g, _layout.controls[0][i], _metrics, colour,
                                control,
                                _state.channelValue (_channel, control),
                                _state.channelToggle (_channel, control));
    }

  // The meter is a component of its own (VuMeterView), standing under the
  // fader at the far right of the band, and it paints itself.
}

}
