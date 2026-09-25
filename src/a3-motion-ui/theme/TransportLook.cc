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

#include "TransportLook.hh"

#include <a3-motion-ui/theme/ThemeColours.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

juce::Colour
transportColour (TransportKey key)
{
  switch (key)
    {
    case TransportKey::Record:
      return toColour (theme ().danger);

    case TransportKey::Stop:
      return toColour (theme ().danger);

    case TransportKey::PlayPause:
      // Green, running or not. The colour says which key this is, and a key
      // that changes colour with its state is one you have to look at twice --
      // once to find it and once to read it. Whether the clip is running is
      // said by the ground, not by the shape -- the shape is a triangle
      // always, because three states will not fit into two of them.
      return toColour (theme ().accent);

    case TransportKey::Action:
      return toColour (theme ().highlight);
    }

  return {};
}

TransportKey
transportKeyForPad (PadFunction function)
{
  switch (function)
    {
    case PadFunction::Stop: return TransportKey::Stop;
    case PadFunction::Action: return TransportKey::Action;
    case PadFunction::PlayPause:
    case PadFunction::Settings: break;
    }

  return TransportKey::PlayPause;
}

bool
hasTransportGlyph (PadFunction function)
{
  return function != PadFunction::Settings;
}

void
drawMenuGlyph (juce::Graphics &g, juce::Rectangle<float> area)
{
  // Three bars and two gaps of the same height, so the mark reads as evenly
  // striped at any size rather than as three bars that happen to be near each
  // other.
  auto const unit = area.getHeight () / 5.f;
  auto const thickness = juce::jmax (1.f, unit);

  for (int i = 0; i < 3; ++i)
    g.fillRoundedRectangle (
        area.withHeight (thickness).withY (area.getY () + unit * 2.f * i),
        thickness * 0.35f);
}

void
drawTransportGlyph (juce::Graphics &g, juce::Rectangle<float> area,
                    TransportKey key)
{
  switch (key)
    {
    case TransportKey::Record:
      g.fillEllipse (area);
      return;

    case TransportKey::Stop:
      g.fillRect (area);
      return;

    case TransportKey::PlayPause:
      {
        // Always the triangle. The shape is which key this is; whether it is
        // running is the ground's to say -- see the header, and
        // transportKeyGround().
        juce::Path play;
        play.addTriangle (area.getX (), area.getY (), area.getX (),
                          area.getBottom (), area.getRight (),
                          area.getCentreY ());
        g.fillPath (play);
        return;
      }

    case TransportKey::Action:
      {
        // Sized to the box it is given rather than to the bar's body size:
        // this is a mark like the other three, not a caption.
        g.setFont (juce::Font (area.getHeight (), juce::Font::bold));
        g.drawFittedText ("A", area.toNearestInt (),
                          juce::Justification::centred, 1);
        return;
      }
    }
}

namespace
{
bool
takeIsWaiting (TransportState const &state)
{
  return state.unsaved && !state.recording;
}

TransportKey
keyWearing (TransportFace face)
{
  switch (face)
    {
    case TransportFace::Record: return TransportKey::Record;
    case TransportFace::Stop: return TransportKey::Stop;
    case TransportFace::PlayPause: return TransportKey::PlayPause;
    case TransportFace::Action: return TransportKey::Action;
    case TransportFace::Save: return TransportKey::PlayPause;
    case TransportFace::Discard: return TransportKey::Stop;
    }
  return TransportKey::PlayPause;
}
}

TransportFace
transportFace (TransportKey key, TransportState const &state)
{
  switch (key)
    {
    case TransportKey::Record:
      return takeIsWaiting (state) ? TransportFace::Save
                                   : TransportFace::Record;
    case TransportKey::Action:
      return takeIsWaiting (state) ? TransportFace::Discard
                                   : TransportFace::Action;
    case TransportKey::Stop: return TransportFace::Stop;
    case TransportKey::PlayPause: return TransportFace::PlayPause;
    }
  return TransportFace::PlayPause;
}

juce::Colour
transportColour (TransportFace face)
{
  return transportColour (keyWearing (face));
}

TransportGround
transportKeyGround (TransportKey key, TransportState const &state)
{
  switch (key)
    {
    case TransportKey::Record:
      // SAVE is lit while there is something to save.
      if (takeIsWaiting (state))
        return TransportGround::Lit;
      return state.recording ? TransportGround::Lit : TransportGround::Dark;

    case TransportKey::PlayPause:
      // Waiting first, and that order is the point: a scheduled *stop* leaves
      // the clip running until the beat lands, so both are true at once. What
      // the key has to say then is "your press was taken", not "still
      // playing" -- the blink is the newer fact.
      //
      // Both ends of the transport wait for the next beat, and up to half a
      // second of nothing after a press reads as a key that did not work.
      if (state.scheduled)
        return TransportGround::Waiting;

      return state.playing ? TransportGround::Lit : TransportGround::Dark;

    case TransportKey::Action:
      // DISCARD is dark until it has been pressed once: a lit key is one the
      // eye goes to, and this one should not be.
      if (takeIsWaiting (state))
        return state.discardArmed ? TransportGround::Lit
                                  : TransportGround::Dark;
      return state.actionActive ? TransportGround::Lit : TransportGround::Dark;

    case TransportKey::Stop:
      // The press alone. Stop has no state to be in, and that is exactly why
      // it needed this: every other key in the row answers a press by
      // changing what it shows, and this one had nothing to change.
      return state.stopPressed ? TransportGround::Lit : TransportGround::Dark;
    }

  return TransportGround::Dark;
}

float
relativeLuminance (juce::Colour colour)
{
  auto const channel = [] (float v) {
    return v <= 0.03928f ? v / 12.92f
                         : std::pow ((v + 0.055f) / 1.055f, 2.4f);
  };

  return 0.2126f * channel (colour.getFloatRed ())
         + 0.7152f * channel (colour.getFloatGreen ())
         + 0.0722f * channel (colour.getFloatBlue ());
}

float
contrastRatio (juce::Colour a, juce::Colour b)
{
  auto const la = relativeLuminance (a);
  auto const lb = relativeLuminance (b);
  auto const hi = std::max (la, lb);
  auto const lo = std::min (la, lb);

  return (hi + 0.05f) / (lo + 0.05f);
}

juce::Colour
readableInk (juce::Colour ink, juce::Colour ground, juce::Colour fallback)
{
  return contrastRatio (ink, ground) >= minimumInkContrast ? ink : fallback;
}

juce::Colour
padGlyphInk (juce::Colour ground)
{
  return contrastRatio (juce::Colours::black, ground)
                 >= contrastRatio (juce::Colours::white, ground)
             ? juce::Colours::black
             : juce::Colours::white;
}

juce::Colour
padFunctionColour (PadFunction function)
{
  if (!hasTransportGlyph (function))
    return {};

  return transportColour (transportKeyForPad (function));
}

void
drawTransportGlyph (juce::Graphics &g, juce::Rectangle<float> area,
                    TransportFace face)
{
  // A stroke relative to the mark, like everything else here -- the same
  // area is drawn at pad size and at bar size.
  auto const stroke
      = juce::PathStrokeType (area.getHeight () * 0.18f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded);

  switch (face)
    {
    case TransportFace::Save:
      {
        juce::Path tick;
        tick.startNewSubPath (area.getX (), area.getCentreY ());
        tick.lineTo (area.getX () + area.getWidth () * 0.38f,
                     area.getBottom ());
        tick.lineTo (area.getRight (), area.getY ());
        g.strokePath (tick, stroke);
        return;
      }
    case TransportFace::Discard:
      {
        juce::Path cross;
        cross.startNewSubPath (area.getTopLeft ());
        cross.lineTo (area.getBottomRight ());
        cross.startNewSubPath (area.getTopRight ());
        cross.lineTo (area.getBottomLeft ());
        g.strokePath (cross, stroke);
        return;
      }
    case TransportFace::Record:
      drawTransportGlyph (g, area, TransportKey::Record);
      return;
    case TransportFace::Stop:
      drawTransportGlyph (g, area, TransportKey::Stop);
      return;
    case TransportFace::PlayPause:
      drawTransportGlyph (g, area, TransportKey::PlayPause);
      return;
    case TransportFace::Action:
      drawTransportGlyph (g, area, TransportKey::Action);
      return;
    }
}

}
