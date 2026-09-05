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

  // Every knob comes out of the rows; the mode does not stand in one.
  for (int i = 0; i < ActMode; ++i)
    _touch[static_cast<size_t> (i)]->setBounds (
        _layout.controls[static_cast<size_t> (i)]);

  _touch[ActMode]->setBounds (_layout.actModeField);
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
ActionComponent::setFreqEnvelope (int attackStep, int decayStep, float max)
{
  if (attackStep == _freqAttack && decayStep == _freqDecay && max == _freqMax)
    return;

  _freqAttack = attackStep;
  _freqDecay = decayStep;
  _freqMax = max;
  repaint ();
}

void
ActionComponent::setQEnvelope (int attackStep, int decayStep, float max)
{
  if (attackStep == _qAttack && decayStep == _qDecay && max == _qMax)
    return;

  _qAttack = attackStep;
  _qDecay = decayStep;
  _qMax = max;
  repaint ();
}

void
ActionComponent::setActionName (juce::String const &name)
{
  if (name == _actionName)
    return;

  _actionName = name;
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
ActionComponent::paintActionField (juce::Graphics &g)
{
  auto const bounds = _layout.actionField;
  if (bounds.isEmpty ())
    return;

  g.setColour (toColour (theme ().textPrimary, 0.06f));
  g.fillRoundedRectangle (bounds.toFloat (), 3.f);
  g.setColour (toColour (theme ().textPrimary, 0.15f));
  g.drawRoundedRectangle (bounds.toFloat (), 3.f, 1.f);

  auto const named = _actionName.isNotEmpty ();

  g.setColour (named ? _channelColour : toColour (theme ().textMuted, 0.5f));
  g.setFont (juce::Font (juce::FontOptions (
      juce::jmin (28.f, bounds.getHeight () / 3.f))));
  // Not a control: it says what is loaded, and the loading happens in the file
  // menu beside the clips, where everything else is chosen.
  g.drawText (named ? _actionName : juce::String ("no action"), bounds,
              juce::Justification::centred);

  g.setColour (toColour (theme ().textMuted, 0.7f));
  g.setFont (juce::Font (juce::FontOptions (
      juce::jmin (12.f, bounds.getHeight () / 6.f))));
  g.drawText ("action", bounds.withTrimmedBottom (bounds.getHeight () * 3 / 4),
              juce::Justification::centred);
}

void
ActionComponent::paint (juce::Graphics &g)
{
  // Solid, because this covers the clip bar's sections rather than sitting
  // beside them: a page that does not fill every pixel shows them through its
  // own gaps.
  g.fillAll (toColour (theme ().background));

  paintActionField (g);

  auto const &metrics = _layout.metrics;

  // Three envelopes, one row each, all the same shape: atk over atk over atk.
  // The accent is read first because it is what ACT has always done, then the
  // cutoff, then the resonance.
  int const steps[]
      = { _attack, _decay, 0, _freqAttack, _freqDecay, 0, _qAttack, _qDecay, 0 };
  float const ceilings[] = { _max, _freqMax, _qMax };

  for (int row = 0; row < ActionLayout::numRows; ++row)
    {
      auto const base = row * 3;
      paintBarKnob (g, _layout.controls[static_cast<size_t> (base)], metrics,
                    _channelColour, caption::attack,
                    envFrac (steps[base]), false, false, true);
      paintBarKnob (g, _layout.controls[static_cast<size_t> (base + 1)],
                    metrics, _channelColour, caption::decay,
                    envFrac (steps[base + 1]), false, false, true);
      paintBarKnob (g, _layout.controls[static_cast<size_t> (base + 2)],
                    metrics, _channelColour, caption::envelopeMax,
                    ceilings[row] * 2.f - 1.f, false, false, true);
    }

  // Which row is which, said once each rather than on every knob.
  char const *const rowNames[] = { "accent", caption::frequency, "q" };
  g.setColour (toColour (theme ().textMuted, 0.8f));
  g.setFont (juce::Font (juce::FontOptions (
      juce::jmin (14.f, _layout.rowLabels[0].getHeight () * 0.4f))));
  for (int row = 0; row < ActionLayout::numRows; ++row)
    g.drawText (rowNames[row], _layout.rowLabels[static_cast<size_t> (row)],
                juce::Justification::centred);

  // Not a knob: it is one of two words, and a knob that can only be at one of
  // two places is a knob that lies about what it can do. It stands beside the
  // action's name because it says what a press does to all three envelopes,
  // so it belongs to none of their rows.
  auto const modeBounds = _layout.actModeField;
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
