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

#include "ActionComponent.hh"

#include <a3-motion-engine/Envelope.hh>

#include <a3-motion-ui/components/BarKnob.hh>
#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
/** Where a step sits on its scale, as the knob speaks it. */
float
envFrac (int step)
{
  return static_cast<float> (step) / static_cast<float> (envelopeMaxStep) * 2.f
         - 1.f;
}
}

ActionComponent::ActionComponent ()
{
  for (int i = 0; i < numControls; ++i)
    {
      auto touch = std::make_unique<TouchControl> ();
      touch->setIdentity (i);

      touch->onDragIncrement = [this] (int control, int, int increment) {
        if (onControlDragged)
          onControlDragged (control, increment);
      };
      touch->onDoubleTap = [this] (int control, int) {
        if (onControlDoubleTapped)
          onControlDoubleTapped (control);
      };
      touch->onTap = [this] (int control, int) {
        if (onControlTapped)
          onControlTapped (control);
      };

      addAndMakeVisible (*touch);
      _touch[static_cast<size_t> (i)] = std::move (touch);
    }
}

ActionComponent::~ActionComponent () = default;

void
ActionComponent::applyTheme ()
{
  resized ();
  repaint ();
}

void
ActionComponent::resized ()
{
  _layout = layOutActionPage (getLocalBounds (),
                              theme ().fontSize (FontRole::Header),
                              theme ().fontSize (FontRole::Body),
                              theme ().potSize);

  for (int i = 0; i < numControls; ++i)
    _touch[static_cast<size_t> (i)]->setBounds (
        _layout.controls[static_cast<size_t> (i)]);
}

void
ActionComponent::setTarget (int channel, int slot, juce::Colour channelColour)
{
  _channel = channel;
  _slot = slot;
  _channelColour = channelColour;
  repaint ();
}

void
ActionComponent::setEnvelope (int attackStep, int decayStep, float max)
{
  if (attackStep == _attack && decayStep == _decay && max == _max)
    return;

  _attack = attackStep;
  _decay = decayStep;
  _max = max;
  repaint ();
}

void
ActionComponent::setActMode (int mode)
{
  if (mode == _actMode)
    return;

  _actMode = mode;
  repaint ();
}

void
ActionComponent::paintEnvelope (juce::Graphics &g)
{
  auto const area = _layout.envelopeGraphic.toFloat ();
  if (area.isEmpty ())
    return;

  // The two times share the drawing in the proportion they will actually run
  // in, so a long attack against a short decay looks like one. A third of the
  // width is left for the hold between them -- the accent does not fall the
  // instant it has risen, and a curve that showed it doing so would be a lie
  // about a one-shot.
  auto const attackBars = envelopeBarsForStep (_attack);
  auto const decayBars = envelopeBarsForStep (_decay);
  auto const total = attackBars + decayBars;
  auto const holdShare = 0.2f;
  auto const rampShare = 1.f - holdShare;

  auto const attackShare
      = total > 0.f ? rampShare * attackBars / total : rampShare * 0.5f;
  auto const decayShare = rampShare - attackShare;

  auto const floorY = area.getBottom ();
  auto const peakY = area.getBottom () - area.getHeight () * _max;

  auto const startX = area.getX ();
  auto const peakX = startX + area.getWidth () * attackShare;
  auto const holdX = peakX + area.getWidth () * holdShare;
  auto const endX = holdX + area.getWidth () * decayShare;

  // The ground the curve stands on, so an envelope at zero still reads as an
  // envelope rather than as an empty panel.
  g.setColour (toColour (theme ().textMuted, 0.2f));
  g.drawLine (startX, floorY, area.getRight (), floorY, 1.f);

  juce::Path curve;
  curve.startNewSubPath (startX, floorY);
  curve.lineTo (peakX, peakY);
  curve.lineTo (holdX, peakY);
  curve.lineTo (endX, floorY);

  // Filled under the line as well as drawn: what the accent does is an amount
  // over time, and an area says amount where a line says only shape.
  auto filled = curve;
  filled.lineTo (startX, floorY);
  filled.closeSubPath ();
  g.setColour (_channelColour.withAlpha (0.18f));
  g.fillPath (filled);

  g.setColour (_channelColour);
  g.strokePath (curve, juce::PathStrokeType (2.f));

  // Where the peak stands, written where the peak is: the max knob says the
  // same number, and the two have to agree at a glance.
  g.setColour (toColour (theme ().textMuted, 0.7f));
  g.setFont (juce::Font (juce::FontOptions (
      juce::jmin (14.f, area.getHeight () / 6.f))));
  g.drawText (juce::String (juce::roundToInt (_max * 100.f)) + "%",
              juce::Rectangle<float> (peakX, peakY - area.getHeight () / 6.f,
                                      area.getWidth () * holdShare,
                                      area.getHeight () / 6.f),
              juce::Justification::centred);
}

void
ActionComponent::paint (juce::Graphics &g)
{
  paintEnvelope (g);

  auto const &metrics = _layout.metrics;

  paintBarKnob (g, _layout.controls[Attack], metrics, _channelColour,
                caption::attack, envFrac (_attack), false, false, true);
  paintBarKnob (g, _layout.controls[Decay], metrics, _channelColour,
                caption::decay, envFrac (_decay), false, false, true);
  paintBarKnob (g, _layout.controls[EnvelopeMax], metrics, _channelColour,
                caption::envelopeMax, _max * 2.f - 1.f, false, false, true);

  // Not a knob: it is one of two words, and a knob that can only be at one of
  // two places is a knob that lies about what it can do.
  auto const modeBounds = _layout.controls[ActMode];
  g.setColour (toColour (theme ().textPrimary, 0.06f));
  g.fillRoundedRectangle (modeBounds.toFloat (), 3.f);
  g.setColour (toColour (theme ().textPrimary, 0.15f));
  g.drawRoundedRectangle (modeBounds.toFloat (), 3.f, 1.f);

  g.setColour (_channelColour);
  g.setFont (juce::Font (juce::FontOptions (
      juce::jmin (20.f, modeBounds.getHeight () / 3.f))));
  g.drawText (value::actModeNames[juce::jlimit (0, value::numActModes - 1,
                                                _actMode)],
              modeBounds, juce::Justification::centred);

  g.setColour (toColour (theme ().textMuted, 0.7f));
  g.setFont (juce::Font (juce::FontOptions (
      juce::jmin (12.f, modeBounds.getHeight () / 5.f))));
  g.drawText (caption::actMode,
              modeBounds.withTrimmedTop (modeBounds.getHeight () * 2 / 3),
              juce::Justification::centred);
}

}
