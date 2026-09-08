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

#include "BarKnob.hh"

#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

#include <cmath>

namespace a3
{

namespace
{
// The same two washes the rest of the bar uses; they came across with the
// drawing rather than being guessed at again.
constexpr float highlightWash = 0.18f;
constexpr float trackWash = 0.18f;

/** Selected, the control wears its channel's colour; otherwise the bar's own
 *  muted grey. The bar asked its member for this -- here it is the argument. */
juce::Colour
controlColour (juce::Colour channelColour, bool isSelected)
{
  return isSelected ? channelColour : toColour (theme ().textMuted);
}

juce::Colour
captionColour (bool isSelected)
{
  // Full opacity rather than an alpha rung: a selected caption has always
  // meant no dimming at all, which the alpha-less overload already says. This
  // used to be `isSelected ? 1.f : 0.55f`; 1.f fits no rung, and full opacity
  // is the absence of an emphasis decision rather than one of its rungs, so
  // it deliberately gets no role of its own. 0.55 is 0.05 from alphaInactive
  // (0.6), inside the snapping tolerance. See
  // issues/a3-motion-ui-metric-role-deviations.md (Task 16).
  return isSelected ? toColour (theme ().textMuted)
                    : toColour (theme ().textMuted, theme ().alphaInactive);
}
}

void
paintBarKnob (juce::Graphics &g, juce::Rectangle<int> bounds,
              ControlMetrics metrics, juce::Colour channelColour,
              juce::String const &label, float angleFrac, bool fillFromZero,
              bool isActive, bool isSelected, float reachFrac, bool wraps)
{
  bool const highlight = isActive && isSelected;
  if (highlight)
    {
      g.setColour (channelColour.withAlpha (highlightWash));
      g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);
    }

  auto content = bounds.reduced (juce::roundToInt (theme ().paddingTight));

  auto labelArea
      = content.removeFromBottom (textRowHeight (content, metrics.captionSize));

  auto const knobColour = controlColour (channelColour, isSelected);
  // The knob keeps its diameter; the captions get the whole cell. Confining
  // both to knobDiam is what truncated "Forward" and "end-action" to "...".
  auto const knobSize = static_cast<float> (
      juce::jmin (metrics.knobDiam, juce::jmin (content.getWidth (),
                                                content.getHeight ())));
  auto const centre = content.toFloat ().getCentre ();
  auto const r = knobSize * 0.5f * 0.82f;

  // Rotary knob, Ableton/Bitwig-style: angleFrac in [-1, 1], 0 points
  // straight up, -1/+1 sit at -135deg/+135deg. Angles here follow JUCE's
  // addCentredArc convention (0 = 12 o'clock, increasing clockwise).
  // A closed control has no ends, so its scale is the whole turn and its
  // value is taken modulo that rather than clamped: clamping is what a stop
  // does, and there is no stop here.
  auto const sweep = wraps ? juce::MathConstants<float>::pi
                           : juce::MathConstants<float>::pi * 0.75f; // 135deg
  auto const wrapped = [] (float frac) {
    frac = std::fmod (frac + 1.f, 2.f);
    return (frac < 0.f ? frac + 2.f : frac) - 1.f;
  };
  auto const angleValue
      = (wraps ? wrapped (angleFrac) : std::clamp (angleFrac, -1.0f, 1.0f))
        * sweep;

  juce::Path track;
  if (wraps)
    track.addEllipse (juce::Rectangle<float> (r * 2.f, r * 2.f)
                          .withCentre (centre));
  else
    track.addCentredArc (centre.x, centre.y, r, r, 0.f, -sweep, sweep, true);
  g.setColour (toColour (theme ().textPrimary, trackWash));
  g.strokePath (track, juce::PathStrokeType (juce::jmax (1.f, r * 0.16f)));

  // Bipolar params (e.g. wrap) fill from the centre out to the value;
  // unipolar params (e.g. clip-top/clip-bottom) fill from the sweep's
  // start, like a standard volume-style knob.
  // A ring has no start to fill from -- filling one would draw a quantity
  // where the reading is an angle. The pointer says it, and the space is left
  // for what the modulation is doing to it.
  if (!wraps)
    {
      juce::Path valueArc;
      auto const fromAngle = fillFromZero ? std::min (0.f, angleValue) : -sweep;
      auto const toAngle = fillFromZero ? std::max (0.f, angleValue) : angleValue;
      valueArc.addCentredArc (centre.x, centre.y, r, r, 0.f, fromAngle, toAngle,
                              true);
      g.setColour (knobColour);
      g.strokePath (valueArc,
                    juce::PathStrokeType (juce::jmax (1.5f, r * 0.16f)));
    }

  // Where a modulation has carried the knob past what was set. The pointer
  // stays put and the arc between the two fills, exactly as the channel grid
  // shows the accent over 3d — one idea, said the same way in both places, so
  // a blue arc always means "something is moving this".
  if (reachFrac > -2.f)
    {
      auto const thickness = juce::jmax (1.5f, r * 0.16f);
      auto const reachAngle
          = (wraps ? wrapped (reachFrac) : std::clamp (reachFrac, -1.f, 1.f))
            * sweep;

      auto const arc = [&] (float from, float to) {
        if (to <= from)
          return;
        juce::Path piece;
        piece.addCentredArc (centre.x, centre.y, r, r, 0.f, from, to, true);
        g.setColour (toColour (theme ().notice));
        g.strokePath (piece, juce::PathStrokeType (thickness));
      };

      if (reachAngle >= angleValue)
        arc (angleValue, reachAngle);
      else
        {
          // Gone round. Drawn as the two pieces it is rather than as nothing:
          // a rotation that passes the end of the scale has not stopped, and
          // an arc that vanished at the top would say it had. On a ring the
          // two pieces meet, so what you see is one arc crossing the top --
          // which is what actually happened.
          arc (angleValue, sweep);
          arc (-sweep, reachAngle);
        }
    }

  // Said outright rather than inherited: the pointer used to be drawn in
  // whatever colour the value arc had left set, so a ring -- which has no
  // value arc -- drew its pointer in the modulation's blue.
  g.setColour (knobColour);
  auto const tip = centre.getPointOnCircumference (r, angleValue);
  g.drawLine (centre.x, centre.y, tip.x, tip.y, juce::jmax (1.5f, r * 0.12f));

  auto const dotR = r * 0.22f;
  g.fillEllipse (juce::Rectangle<float> (dotR, dotR).withCentre (centre));

  // The shared size, not this caption's own fit. Its box is only consulted as
  // a floor: a control box too short for the shared size would otherwise have
  // drawFittedText spill the caption over the row beneath it.
  g.setFont (juce::Font (juce::jmin (metrics.captionSize,
                                     static_cast<float> (labelArea.getHeight ())
                                         * 0.85f),
                         juce::Font::plain));
  g.setColour (captionColour (isSelected));
  g.drawFittedText (label, labelArea,
                    juce::Justification::centred, 1);
}

}
