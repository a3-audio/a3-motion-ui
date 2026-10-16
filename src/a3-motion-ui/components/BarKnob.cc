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

#include <a3-motion-ui/components/ClipKnobs.hh>
#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>
#include <a3-motion-ui/theme/TransportLook.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
// The same two washes the rest of the bar uses; they came across with the
// drawing rather than being guessed at again.
constexpr float highlightWash = 0.18f;

/** Selected, the control wears its channel's colour; otherwise the bar's own
 *  muted grey. The bar asked its member for this -- here it is the argument.
 *
 *  The one of these that is genuinely its own rule: a knob's arc and pointer
 *  do take the channel's colour when its section is picked. Everything that
 *  is *read* rather than drawn -- this knob's caption included -- is grey
 *  either way and goes through Colours::barText. */
juce::Colour
controlColour (juce::Colour channelColour, bool isSelected)
{
  return isSelected ? channelColour : toColour (theme ().textMuted);
}

}

namespace
{
// StemDeck's knob (stemdeck/Source/Theme.cpp, drawRotarySlider), in shares.
constexpr float ringShareOfHalfSize = 0.92f;
constexpr float strokeShareOfRadius = 0.18f;
constexpr float bodyGapInStrokes = 2.2f;
constexpr float pointerFromShare = 0.4f;
constexpr float pointerToShare = 0.9f;
constexpr float pointerWidthShare = 0.2f;
}

KnobGeometry
knobGeometry (float knobSize)
{
  KnobGeometry k;
  k.radius = knobSize * 0.5f * ringShareOfHalfSize;
  k.stroke = k.radius * strokeShareOfRadius;
  k.ringRadius = k.radius - 0.5f * k.stroke;
  k.bodyRadius = k.radius - bodyGapInStrokes * k.stroke;
  return k;
}

std::vector<std::pair<float, float> >
modulationArcs (float valueAngle, float reachAngle, float sweep, bool wraps)
{
  if (juce::approximatelyEqual (valueAngle, reachAngle))
    return {};

  if (reachAngle > valueAngle)
    return { { valueAngle, reachAngle } };

  if (!wraps)
    return { { reachAngle, valueAngle } };

  // Gone round. Drawn as the two pieces it is rather than as nothing:
  // a rotation that passes the end of the scale has not stopped, and
  // an arc that vanished at the top would say it had. On a ring the
  // two pieces meet, so what you see is one arc crossing the top --
  // which is what actually happened.
  return { { valueAngle, sweep }, { -sweep, reachAngle } };
}

namespace
{
/** The widest caption any of the bar's knobs carries, at `size`. */
float
widestKnobCaption (float size)
{
  juce::Font const font{ juce::FontOptions (size) };
  auto widest = 0.f;
  for (int sub = 0; sub < numMotionKnobs; ++sub)
    widest = std::max (widest, juce::GlyphArrangement::getStringWidth (
                                   font, motionKnobSpec (sub).label));
  for (int sub = 0; sub < 4; ++sub)
    widest = std::max (widest, juce::GlyphArrangement::getStringWidth (
                                   font, elevationKnobSpec (sub).label));
  return widest;
}
}

/** How tall a knob and its caption stand together: the knob, the caption's
 *  row under it, and the cell's own padding. A cell taller than this keeps
 *  them together in its middle rather than pulling them apart. */
float
knobBlockHeight (ControlMetrics metrics)
{
  return static_cast<float> (metrics.knobDiam)
         + std::ceil (metrics.captionSize * rowHeightFactor)
         + 2.f * theme ().paddingTight;
}

juce::Rectangle<float>
encoderMarkBounds (juce::Rectangle<int> bounds, ControlMetrics metrics)
{
  auto const cell = bounds.toFloat ();
  auto const inner = std::max (static_cast<float> (metrics.knobDiam),
                               widestKnobCaption (metrics.captionSize));
  // Half a caption's height of air each side, so the widest word does not
  // touch the frame.
  auto const air = std::max (theme ().paddingTight, metrics.captionSize * 0.5f);
  auto const width = std::min (cell.getWidth (), inner + 2.f * air);
  // And as tall as the knob with its caption, with the same air above and
  // below.
  auto const height
      = std::min (cell.getHeight (), knobBlockHeight (metrics) + 2.f * air);
  return cell.withSizeKeepingCentre (width, height);
}

void
paintBarKnob (juce::Graphics &g, juce::Rectangle<int> bounds,
              ControlMetrics metrics, juce::Colour channelColour,
              juce::String const &label, float angleFrac, bool fillFromZero,
              bool isActive, bool isSelected, float reachFrac, bool wraps,
              bool writing, bool laneDriven, bool encoderMarked)
{
  bool const highlight = isActive && isSelected;
  if (highlight)
    {
      g.setColour (channelColour.withAlpha (highlightWash));
      g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);
    }

  // The knob an encoder is on, where a press switches between two: a frame
  // in the skin's accent, one size on every knob, so a press is seen to move
  // it.
  if (encoderMarked)
    {
      g.setColour (toColour (theme ().accent));
      // On whole pixels, the line on their centres: a thin line between two
      // pixels is drawn as a blur across both.
      g.drawRoundedRectangle (encoderMarkBounds (bounds, metrics)
                                  .toNearestInt ()
                                  .toFloat ()
                                  .reduced (theme ().strokeThin * 0.5f),
                              theme ().radiusControl,
                              theme ().strokeThin);
    }

  // Knob and caption together in the middle of a tall cell, not the knob up
  // top and the caption at the foot (2026-09-27).
  auto content = bounds.reduced (juce::roundToInt (theme ().paddingTight));
  content = content.withSizeKeepingCentre (
      content.getWidth (),
      std::min (content.getHeight (),
                juce::roundToInt (knobBlockHeight (metrics)
                                  - 2.f * theme ().paddingTight)));

  // No caption, no caption row: the knob takes the cell (the KEYS knobs).
  auto labelArea
      = label.isEmpty ()
            ? juce::Rectangle<int> ()
            : content.removeFromBottom (
                  textRowHeight (content, metrics.captionSize));

  auto const knobColour = controlColour (channelColour, isSelected);
  // A take writing this knob: the REC key's red on what the hand sets, the
  // value arc and the pointer. Blue stays what a lane playing it looks like.
  auto const writingColour = transportColour (TransportKey::Record);
  auto const valueColour = writing ? writingColour : knobColour;
  // The knob keeps its diameter; the captions get the whole cell. Confining
  // both to knobDiam is what truncated "Forward" and "end-action" to "...".
  auto const knobSize = static_cast<float> (
      juce::jmin (metrics.knobDiam, juce::jmin (content.getWidth (),
                                                content.getHeight ())));
  auto const centre = content.toFloat ().getCentre ();
  auto const k = knobGeometry (knobSize);
  auto const r = k.ringRadius;

  // StemDeck's knob (2026-10-16): a ring with rounded ends around a raised
  // cap, the channel's colour in the arc, a neutral tick on the cap. angleFrac
  // in [-1, 1], 0 points straight up, -1/+1 sit at -144deg/+144deg. Angles
  // follow JUCE's addCentredArc convention (0 = 12 o'clock, clockwise).
  // A closed control has no ends, so its scale is the whole turn and its
  // value is taken modulo that rather than clamped: clamping is what a stop
  // does, and there is no stop here.
  auto const sweep = wraps ? juce::MathConstants<float>::pi : knobSweep;
  auto const wrapped = [] (float frac) {
    frac = std::fmod (frac + 1.f, 2.f);
    return (frac < 0.f ? frac + 2.f : frac) - 1.f;
  };
  auto const angleValue
      = (wraps ? wrapped (angleFrac) : std::clamp (angleFrac, -1.0f, 1.0f))
        * sweep;

  juce::PathStrokeType const ringStroke (k.stroke,
                                         juce::PathStrokeType::curved,
                                         juce::PathStrokeType::rounded);
  auto const strokeArc = [&] (float from, float to) {
    // A rounded end on nothing would still draw a dot.
    if (juce::approximatelyEqual (from, to))
      return;
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, r, r, 0.f, from, to, true);
    g.strokePath (arc, ringStroke);
  };

  g.setColour (toColour (theme ().textPrimary, theme ().alphaGuide));
  if (wraps)
    {
      juce::Path track;
      track.addEllipse (juce::Rectangle<float> (r * 2.f, r * 2.f)
                            .withCentre (centre));
      g.strokePath (track, ringStroke);
    }
  else
    strokeArc (-sweep, sweep);

  // Bipolar params (e.g. wrap) fill from the centre out to the value;
  // unipolar params (e.g. clip-top/clip-bottom) fill from the sweep's
  // start, like a standard volume-style knob.
  // A ring has no start to fill from -- filling one would draw a quantity
  // where the reading is an angle. The pointer says it, and the space is left
  // for what the modulation is doing to it.
  if (!wraps)
    {
      g.setColour (valueColour);
      strokeArc (fillFromZero ? std::min (0.f, angleValue) : -sweep,
                 fillFromZero ? std::max (0.f, angleValue) : angleValue);
    }

  // Where a modulation has carried the knob past what was set. The pointer
  // stays put and the arc between the two fills, exactly as the channel grid
  // shows the accent over 3d — one idea, said the same way in both places, so
  // a blue arc always means "something is moving this".
  if (reachFrac > -2.f)
    {
      auto const reachAngle
          = (wraps ? wrapped (reachFrac) : std::clamp (reachFrac, -1.f, 1.f))
            * sweep;

      g.setColour (toColour (theme ().notice));
      for (auto const &[from, to] :
           modulationArcs (angleValue, reachAngle, sweep, wraps))
        strokeArc (from, to);
    }

  // The cap, lit from above -- lightness is elevation. Both ends are the
  // bar's ground lifted by the skin's own washes, so a skin moves it with
  // everything else.
  auto const ground = toColour (theme ().surface);
  auto const bodyR = k.bodyRadius;
  g.setGradientFill (juce::ColourGradient (
      ground.overlaidWith (toColour (theme ().textPrimary, theme ().alphaGuide)),
      centre.x, centre.y - bodyR,
      ground.overlaidWith (toColour (theme ().textPrimary, theme ().alphaFill)),
      centre.x, centre.y + bodyR, false));
  g.fillEllipse (juce::Rectangle<float> (bodyR * 2.f, bodyR * 2.f)
                     .withCentre (centre));

  // The pointer: a light tick on the cap, not the channel's colour -- the
  // arc carries whose knob it is, the tick where it stands. Said outright
  // rather than inherited from the last colour set.
  auto const pointerColour
      = writing ? writingColour
                : (isSelected ? toColour (theme ().textPrimary)
                              : toColour (theme ().textMuted));
  auto const tickWidth = bodyR * pointerWidthShare;
  juce::Path tick;
  tick.addRoundedRectangle (-0.5f * tickWidth, -bodyR * pointerToShare,
                            tickWidth,
                            bodyR * (pointerToShare - pointerFromShare),
                            0.5f * tickWidth);
  g.setColour (pointerColour);
  g.fillPath (tick, juce::AffineTransform::rotation (angleValue)
                        .translated (centre));

  // A take's lane is turning it: a small dot in the REC key's red, up and to
  // the right of the ring, where it names the knob without covering its arc.
  if (laneDriven)
    {
      auto const markR = juce::jmax (2.f, r * 0.18f);
      auto const at = centre.getPointOnCircumference (
          r + markR * 1.6f, juce::MathConstants<float>::pi * 0.25f);
      g.setColour (transportColour (TransportKey::Record));
      g.fillEllipse (
          juce::Rectangle<float> (markR * 2.f, markR * 2.f).withCentre (at));
    }

  if (label.isEmpty ())
    return;

  // The shared size, not this caption's own fit. Its box is only consulted as
  // a floor: a control box too short for the shared size would otherwise have
  // drawFittedText spill the caption over the row beneath it.
  g.setFont (juce::Font (juce::jmin (metrics.captionSize,
                                     static_cast<float> (labelArea.getHeight ())
                                         * 0.85f),
                         juce::Font::plain));
  g.setColour (Colours::barText (isSelected));
  g.drawFittedText (label, labelArea,
                    juce::Justification::centred, 1);
}

}
