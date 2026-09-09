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

#include "MixerComponent.hh"

#include <a3-motion-ui/components/BarButton.hh>
#include <a3-motion-ui/components/BarKnob.hh>
#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/components/OverlayButtons.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
// A strip's own ground, faint enough that the five columns read as five
// blocks without any of them becoming a panel in its own right. The master
// wears it too: it is the fifth strip, not a section standing beside four.
constexpr float stripWash = 0.07f;

/** Whether the control's middle means neutral.
 *
 *  An EQ band is cut or boost either side of flat, so its arc grows out of
 *  the middle and which side of flat you are on reads at a glance. Gain and
 *  the volume run from silence upwards and fill from their start, the way a
 *  volume knob anywhere does. */
bool
fillsFromTheMiddle (MixerControl control)
{
  return control == MixerControl::EqHigh || control == MixerControl::EqMid
         || control == MixerControl::EqLow;
}

/** The same, for the summing section: the phones' blend sits between two ends
 *  and reads as a distance from the middle; everything else is a level. */
bool
fillsFromTheMiddle (MasterControl control)
{
  return control == MasterControl::PhonesMix;
}

/** A 0..1 value on paintBarKnob's -1..1 scale. */
float
angleFor (float value)
{
  return value * 2.f - 1.f;
}
}

void
paintMixerChannelControl (juce::Graphics &g, juce::Rectangle<int> bounds,
                          ControlMetrics metrics, juce::Colour colour,
                          MixerControl control, float value, bool isOn)
{
  auto const label = juce::String (mixerControlLabel (control));

  if (mixerControlIsAToggle (control))
    {
      // A key, drawn as the bar draws its keys. `isSelected` is what asks for
      // the channel's colour rather than the grey a button belonging to no
      // channel gets, and every key here belongs to one -- so an on key is
      // active and selected at once.
      paintBarButton (g, bounds, metrics, colour, label, {}, isOn, isOn);
      return;
    }

  paintBarKnob (g, bounds, metrics, colour, label, angleFor (value),
                fillsFromTheMiddle (control), false, true);
}

ControlMetrics
mixerControlMetrics ()
{
  return ControlMetrics{
    knobDiameterForFont (theme ().fontSize (FontRole::Body),
                         theme ().potSize),
    theme ().fontSize (FontRole::Body),
    theme ().fontSize (FontRole::Body),
  };
}

void
wireMixerChannelTouch (TouchControl &touch, MixerControl control,
                       std::function<void (MixerControl, int)> dragged,
                       std::function<void (MixerControl)> tapped)
{
  touch.onDragIncrement = [control, dragged] (int, int, int increment) {
    if (dragged)
      dragged (control, increment);
  };

  if (!mixerControlIsAToggle (control))
    return;

  touch.onTap = [control, tapped] (int, int) {
    if (tapped)
      tapped (control);
  };
}

void
runMeterTimerWhileVisible (bool isVisible, juce::Timer &timer)
{
  if (isVisible)
    timer.startTimerHz (vuMeterRefreshHz);
  else
    timer.stopTimer ();
}

void
repaintMixerMeters (juce::Component &page, MixerLayout const &layout)
{
  if (!layout.fits)
    return;

  auto const redraw = [&page] (juce::Rectangle<int> const &meter) {
    if (!meter.isEmpty ())
      page.repaint (meter);
  };

  for (auto const &meter : layout.channelMeter)
    redraw (meter);
  for (auto const &bar : layout.outputMeters)
    redraw (bar);
}

void
paintMixerHasNoRoom (juce::Graphics &g, juce::Rectangle<int> bounds,
                     juce::String const &what)
{
  g.setColour (toColour (theme ().textMuted));
  g.setFont (
      juce::Font (juce::FontOptions (theme ().fontSize (FontRole::Body))));
  g.drawFittedText ("Not enough room for " + what, bounds,
                    juce::Justification::centred, 1);
}

MixerComponent::MixerComponent (MixerState &state, VuLevels const &levels)
    : _state (state), _levels (levels)
{
  setInterceptsMouseClicks (false, true);

  auto const hookUp = [this] (TouchControl &touch, int primary,
                              int secondary) {
    touch.setIdentity (primary, secondary);
    addAndMakeVisible (touch);
  };

  for (int channel = 0; channel < numChannelsInitial; ++channel)
    for (int i = 0; i < numMixerControls; ++i)
      {
        auto touch = std::make_unique<TouchControl> ();

        // The channel is closed over rather than read off the identity: a
        // strip here is one channel's for the life of the component, where the
        // bar's tab swaps whose strip it shows under a finger. Both pages hand
        // wireMixerChannelTouch the same two questions and differ only in how
        // they answer "whose".
        wireMixerChannelTouch (
            *touch, mixerControlOrder[static_cast<std::size_t> (i)],
            [this, channel] (MixerControl control, int increment) {
              if (onChannelDragged)
                onChannelDragged (channel, control, increment);
            },
            [this, channel] (MixerControl control) {
              if (onChannelTapped)
                onChannelTapped (channel, control);
            });

        hookUp (*touch, channel, i);
        _channelTouch[static_cast<std::size_t> (channel)]
                     [static_cast<std::size_t> (i)]
            = std::move (touch);
      }

  for (int i = 0; i < numMasterControls; ++i)
    {
      auto touch = std::make_unique<TouchControl> ();
      touch->onDragIncrement = [this] (int, int secondary, int increment) {
        if (onMasterDragged)
          onMasterDragged (
              masterControlOrder[static_cast<std::size_t> (secondary)],
              increment);
      };

      hookUp (*touch, masterGroup, i);
      _masterTouch[static_cast<std::size_t> (i)] = std::move (touch);
    }

  for (int i = 0; i < numFilterControls; ++i)
    {
      auto touch = std::make_unique<TouchControl> ();
      touch->onDragIncrement = [this] (int, int secondary, int increment) {
        if (onFilterDragged)
          onFilterDragged (
              filterControlOrder[static_cast<std::size_t> (secondary)],
              increment);
      };

      if (filterControlOrder[static_cast<std::size_t> (i)]
          == FilterControl::Mode)
        touch->onTap = [this] (int, int secondary) {
          if (onFilterTapped)
            onFilterTapped (
                filterControlOrder[static_cast<std::size_t> (secondary)]);
        };

      hookUp (*touch, filterGroup, i);
      _filterTouch[static_cast<std::size_t> (i)] = std::move (touch);
    }

  applyTheme ();
}

MixerComponent::~MixerComponent () = default;

void
MixerComponent::visibilityChanged ()
{
  runMeterTimerWhileVisible (isVisible (), *this);
}

void
MixerComponent::timerCallback ()
{
  repaintMixerMeters (*this, _layout);
}

juce::Rectangle<int>
MixerComponent::panelBounds () const
{
  return getLocalBounds ();
}

void
MixerComponent::applyTheme ()
{
  _metrics = mixerControlMetrics ();

  resized ();
  repaint ();
}

void
MixerComponent::resized ()
{
  // Back and close float in the top right of whatever overlay is open, and
  // they are drawn after it. The band they stand in is kept clear rather than
  // laid out under them: the menu's panel is narrow enough to miss them, and
  // a mixer that filled the width put its fourth strip's top control under a
  // key that answers for something else.
  auto area = getLocalBounds ();
  area.removeFromTop (OverlayButtons::preferredHeight ()
                      + 2 * OverlayButtons::preferredMargin ());

  _layout = layOutMixerOverlay (area, _metrics);

  // A page that does not fit draws one sentence and no controls, so nothing
  // here may still take a touch: a hit area with nothing under it is how a
  // finger changes a value it cannot see -- the rule
  // ClipSettingsComponent::setPage stands on. The rectangles are deliberately
  // sane rather than empty when `fits` is false (MixerLayout.cc), which is
  // what left fifteen full-size targets over the sentence, and the row floor
  // is made of Pot Size -- a skin value the performer dials on the device --
  // so this is reachable without resizing anything.
  for (int channel = 0; channel < numChannelsInitial; ++channel)
    for (int i = 0; i < numMixerControls; ++i)
      {
        auto &touch = _channelTouch[static_cast<std::size_t> (channel)]
                                   [static_cast<std::size_t> (i)];
        touch->setBounds (_layout.controls[static_cast<std::size_t> (channel)]
                                          [static_cast<std::size_t> (i)]);
        touch->setVisible (_layout.fits);
      }

  for (int i = 0; i < numMasterControls; ++i)
    {
      auto &touch = _masterTouch[static_cast<std::size_t> (i)];
      touch->setBounds (_layout.master[static_cast<std::size_t> (i)]);
      touch->setVisible (_layout.fits);
    }

  for (int i = 0; i < numFilterControls; ++i)
    {
      auto &touch = _filterTouch[static_cast<std::size_t> (i)];
      touch->setBounds (_layout.filter[static_cast<std::size_t> (i)]);
      touch->setVisible (_layout.fits);
    }
}

void
MixerComponent::paint (juce::Graphics &g)
{
  // Opaque, unlike the menu and the skin editor: those are settings pages you
  // glance at, and seeing the room through them says the set is still running
  // behind. This one *is* the set -- nine meters and twenty-three controls
  // read at a glance in a dark booth -- and a sphere showing through a level
  // meter is a moving picture behind the one thing on this device that has to
  // be read exactly.
  g.fillAll (toColour (theme ().surface));

  if (!_layout.fits)
    {
      paintMixerHasNoRoom (g, getLocalBounds (), "the mixer");
      return;
    }

  for (int channel = 0; channel < numChannelsInitial; ++channel)
    paintStrip (g, channel);

  paintMasterColumn (g);
  paintFilterRow (g);
  paintMeters (g);
}

void
MixerComponent::paintMeters (juce::Graphics &g)
{
  // One reading of the clock for the whole page. Nine meters each asking the
  // time would draw nine slightly different moments, and a peak mark that
  // expired between two columns of the same picture is a picture that
  // contradicts itself.
  auto const now = vuNowMs ();

  // Every meter on the page is drawn the same, the channels' and the outputs'
  // alike: green, yellow and red down a bar is a scale, and a scale that meant
  // something different on the fifth column from the four beside it would be
  // read wrong exactly once, at the moment it mattered.
  for (int channel = 0; channel < numChannelsInitial; ++channel)
    paintVuMeter (g, _layout.channelMeter[static_cast<std::size_t> (channel)],
                  _levels.channel (channel, now));

  for (int meter = 0; meter < numOutputMeters; ++meter)
    paintVuMeter (g, _layout.outputMeters[static_cast<std::size_t> (meter)],
                  _levels.output (meter, now));

  if (!_layout.outputMeterCaption.isEmpty ())
    {
      g.setColour (toColour (theme ().textMuted));
      g.setFont (juce::Font (juce::FontOptions (_metrics.captionSize)));
      g.drawFittedText ("OUT", _layout.outputMeterCaption,
                        juce::Justification::centred, 1);
    }
}

void
MixerComponent::paintStrip (juce::Graphics &g, int channel)
{
  auto const &cells = _layout.controls[static_cast<std::size_t> (channel)];
  auto const colour = toColour (theme ().channel[channel]);

  // The strip's ground is the union of its rows and the meter column beside
  // them, so a deck reads as one block however the columns broke. In the
  // channel's own colour, because finding your deck by colour is the one thing
  // this screen has to answer without being read -- and since the meter itself
  // stopped wearing that colour when it took up the green/yellow/red banding,
  // this wash is now what says whose meter it is.
  auto ground = cells.front ();
  for (auto const &cell : cells)
    ground = ground.getUnion (cell);
  ground = ground.getUnion (
      _layout.channelMeter[static_cast<std::size_t> (channel)]);

  g.setColour (colour.withAlpha (stripWash));
  g.fillRoundedRectangle (ground.toFloat (), theme ().radiusCard);

  for (std::size_t i = 0; i < static_cast<std::size_t> (numMixerControls);
       ++i)
    {
      auto const control = mixerControlOrder[i];
      paintMixerChannelControl (g, cells[i], _metrics, colour, control,
                                _state.channelValue (channel, control),
                                _state.channelToggle (channel, control));
    }
}

void
MixerComponent::paintMasterColumn (juce::Graphics &g)
{
  // No channel's colour, because none of this belongs to a channel -- the
  // same reason the bar's global strip paints its four keys grey. White
  // rather than the muted grey a deselected control wears, because the master
  // volume is not a secondary control; it is simply nobody's in particular.
  auto const colour = toColour (theme ().textPrimary);

  // The same ground the four strips wear, so the master reads as the fifth of
  // five rather than as a panel that happens to stand beside them. Down to
  // the foot of the output meters, which stand in the two rows under its own
  // five: the column is one block, and a wash stopping short of its last two
  // rows would read as the meters having been pasted on underneath it.
  auto ground = _layout.master.front ();
  for (auto const &cell : _layout.master)
    ground = ground.getUnion (cell);
  for (auto const &bar : _layout.outputMeters)
    ground = ground.getUnion (bar);
  ground = ground.getUnion (_layout.outputMeterCaption);

  g.setColour (colour.withAlpha (stripWash));
  g.fillRoundedRectangle (ground.toFloat (), theme ().radiusCard);

  for (std::size_t i = 0; i < static_cast<std::size_t> (numMasterControls);
       ++i)
    {
      auto const control = masterControlOrder[i];
      auto const value = _state.masterValue (control);
      auto const bounds = _layout.master[i];
      auto const label = juce::String (masterControlLabel (control));

      paintBarKnob (g, bounds, _metrics, colour, label, angleFor (value),
                    fillsFromTheMiddle (control), false, true);
    }
}

void
MixerComponent::paintFilterRow (juce::Graphics &g)
{
  auto const colour = toColour (theme ().textPrimary);

  for (std::size_t i = 0; i < static_cast<std::size_t> (numFilterControls);
       ++i)
    {
      auto const control = filterControlOrder[i];
      auto const bounds = _layout.filter[i];
      auto const label = juce::String (filterControlLabel (control));

      if (control == FilterControl::Mode)
        {
          // The key says which mode it is *in*, not which one the next press
          // would bring -- the same rule the library's filter key follows. A
          // key naming what you would get is one you press to find out where
          // you are.
          //
          // Lit whichever mode that is. The filter has no off state, so a key
          // that dimmed in one of its two positions would say the filter was
          // off in the more common one -- and the grab that follows, meaning
          // "switch it on", is LPF to HPF across all four channels. The word
          // is what says the direction; the brightness has nothing left to
          // say and so says nothing. Selected, like the master column beside
          // it, for the same reason: `colour` is textPrimary rather than a
          // channel's, because none of this belongs to a channel.
          paintBarButton (g, bounds, _metrics, colour,
                          _state.filterIsHighPass () ? "HPF" : "LPF", label,
                          true, true);
          continue;
        }

      paintBarKnob (g, bounds, _metrics, colour, label,
                    angleFor (_state.filterValue (control)), false, false,
                    true);
    }
}

}
