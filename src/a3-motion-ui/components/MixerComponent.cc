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

#include <a3-motion-ui/components/BarFader.hh>
#include <a3-motion-ui/components/BarKnob.hh>
#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/components/OverlayButtons.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
// The dim the other overlays lay over the sphere. Taken from
// GlobalSettingsComponent rather than guessed at again: an overlay that dimmed
// the room by a different amount would read as a different kind of overlay.
constexpr float overlayOpacity = 0.55f;

// The bar's button face, brought across so a key here reads as the same kind
// of thing as a key down there -- a wash and a thin edge, never a filled slab.
constexpr float keyFaceWash = 0.08f;
constexpr float keyActiveWash = 0.36f;
constexpr float keyEdgeWash = 0.18f;

// A strip's own ground, faint enough that the four blocks read as four decks
// without any of them becoming a panel in its own right.
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

/** A key rather than a pot: the same face the bar's buttons wear, so a
 *  two-valued control reads as the same kind of thing wherever it is. */
void
paintKeyFace (juce::Graphics &g, juce::Rectangle<int> bounds,
              ControlMetrics metrics, juce::Colour tint,
              juce::String const &caption, juce::String const &value,
              bool isOn)
{
  if (bounds.isEmpty ())
    return;

  // The colour says which key this is and the ground says what it is doing --
  // two questions in two places, rather than one colour asked to answer both.
  g.setColour (isOn ? tint.withAlpha (keyActiveWash)
                    : toColour (theme ().textPrimary, keyFaceWash));
  g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);

  g.setColour (toColour (theme ().textPrimary, keyEdgeWash));
  g.drawRoundedRectangle (bounds.toFloat (), theme ().radiusControl,
                          theme ().strokeThin);

  auto box = bounds.reduced (juce::roundToInt (theme ().paddingTight));
  g.setFont (juce::Font (juce::FontOptions (metrics.captionSize)));

  if (value.isEmpty ())
    {
      g.setColour (isOn ? tint : toColour (theme ().textMuted));
      g.drawFittedText (caption, box, juce::Justification::centred, 1);
      return;
    }

  // Two lines, both inside the box: the name on top and the value under it,
  // the way every button in the bar that stands for something is drawn.
  auto const captionArea = box.removeFromTop (box.getHeight () / 2);
  g.setColour (toColour (theme ().textMuted));
  g.drawFittedText (caption, captionArea, juce::Justification::centred, 1);

  g.setColour (isOn ? tint : toColour (theme ().textMuted));
  g.drawFittedText (value, box, juce::Justification::centred, 1);
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
      paintKeyFace (g, bounds, metrics, colour, label, {}, isOn);
      return;
    }

  // A throw, because it is the channel's level: mixerControlIsAFader() says
  // which control that is and nothing here measures the cell to find out. The
  // layout has already made room for the throw, and said `fits = false` if it
  // could not.
  if (mixerControlIsAFader (control))
    {
      paintBarFader (g, bounds, metrics, colour, label, value, true, true);
      return;
    }

  paintBarKnob (g, bounds, metrics, colour, label, angleFor (value),
                fillsFromTheMiddle (control), false, true);
}

MixerComponent::MixerComponent (MixerState &state) : _state (state)
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

        // The accumulator is TouchControl's own, fed from the skin's
        // touchDragPixelsPerStep, so how far a finger travels for one step
        // stays one screw in the skin editor rather than becoming a second
        // one here.
        touch->onDragIncrement
            = [this] (int primary, int secondary, int increment) {
                if (onChannelDragged)
                  onChannelDragged (
                      primary,
                      mixerControlOrder[static_cast<std::size_t> (secondary)],
                      increment);
              };

        // Only the two-valued controls answer a tap. A continuous value is
        // dragged and never tapped -- a tap has no direction, so there is
        // nothing for it to say about a level.
        if (mixerControlIsAToggle (
                mixerControlOrder[static_cast<std::size_t> (i)]))
          touch->onTap = [this] (int primary, int secondary) {
            if (onChannelTapped)
              onChannelTapped (
                  primary,
                  mixerControlOrder[static_cast<std::size_t> (secondary)]);
          };

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

juce::Rectangle<int>
MixerComponent::panelBounds () const
{
  return getLocalBounds ();
}

void
MixerComponent::applyTheme ()
{
  // The knob's diameter is the skin's pot size, and the layout's floor for a
  // row is that diameter -- so a skin change moves every rectangle here, not
  // only their colours.
  _metrics = ControlMetrics{
    knobDiameterForFont (theme ().fontSize (FontRole::Body),
                         theme ().potSize),
    // One size for both the caption and the value. The bar fits these to the
    // width of a section it shares with two others; a strip here is a column
    // of its own with a four-character word in it, so the body size the skin
    // asks for is what it gets.
    theme ().fontSize (FontRole::Body),
    theme ().fontSize (FontRole::Body),
  };

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

  for (int channel = 0; channel < numChannelsInitial; ++channel)
    for (int i = 0; i < numMixerControls; ++i)
      _channelTouch[static_cast<std::size_t> (channel)]
                   [static_cast<std::size_t> (i)]
                       ->setBounds (
                           _layout.controls[static_cast<std::size_t> (channel)]
                                           [static_cast<std::size_t> (i)]);

  for (int i = 0; i < numMasterControls; ++i)
    _masterTouch[static_cast<std::size_t> (i)]->setBounds (
        _layout.master[static_cast<std::size_t> (i)]);

  for (int i = 0; i < numFilterControls; ++i)
    _filterTouch[static_cast<std::size_t> (i)]->setBounds (
        _layout.filter[static_cast<std::size_t> (i)]);
}

void
MixerComponent::paint (juce::Graphics &g)
{
  g.fillAll (toColour (theme ().surface, overlayOpacity));

  if (!_layout.fits)
    {
      // One line of text rather than targets nobody can hit. A mixer that
      // cannot be operated is worse than a sentence saying the window is too
      // small, because the sentence can be acted on.
      g.setColour (toColour (theme ().textMuted));
      g.setFont (juce::Font (
          juce::FontOptions (theme ().fontSize (FontRole::Body))));
      g.drawFittedText ("Not enough room for the mixer", getLocalBounds (),
                        juce::Justification::centred, 1);
      return;
    }

  for (int channel = 0; channel < numChannelsInitial; ++channel)
    paintStrip (g, channel);

  paintSumming (g);
}

void
MixerComponent::paintStrip (juce::Graphics &g, int channel)
{
  auto const &cells = _layout.controls[static_cast<std::size_t> (channel)];
  auto const colour = toColour (theme ().channel[channel]);

  // The strip's ground is the union of its rows, so a deck reads as one block
  // however the columns broke. In the channel's own colour, because finding
  // your deck by colour is the one thing this screen has to answer without
  // being read.
  auto ground = cells.front ();
  for (auto const &cell : cells)
    ground = ground.getUnion (cell);

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
MixerComponent::paintSumming (juce::Graphics &g)
{
  // No channel's colour, because none of this belongs to a channel -- the
  // same reason the bar's global strip paints its four keys grey. White
  // rather than the muted grey a deselected control wears, because the master
  // volume is not a secondary control; it is simply nobody's in particular.
  auto const colour = toColour (theme ().textPrimary);

  for (std::size_t i = 0; i < static_cast<std::size_t> (numMasterControls);
       ++i)
    {
      auto const control = masterControlOrder[i];
      auto const value = _state.masterValue (control);
      auto const bounds = _layout.master[i];
      auto const label = juce::String (masterControlLabel (control));

      // Nothing here is a throw, the master volume least of all. It is the
      // summing level -- set at the start of the night and left -- and a page
      // whose most fader-looking control was the master would send a hand
      // reaching for it to bring a channel in.
      paintBarKnob (g, bounds, _metrics, colour, label, angleFor (value),
                    fillsFromTheMiddle (control), false, true);
    }

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
          auto const highPass = _state.filterIsHighPass ();
          paintKeyFace (g, bounds, _metrics, colour, label,
                        highPass ? "HPF" : "LPF", highPass);
          continue;
        }

      paintBarKnob (g, bounds, _metrics, colour, label,
                    angleFor (_state.filterValue (control)), false, false,
                    true);
    }
}

}
