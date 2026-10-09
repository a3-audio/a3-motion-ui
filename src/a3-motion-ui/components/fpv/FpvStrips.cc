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

#include "FpvStrips.hh"

#include <algorithm>
#include <cmath>

#include <a3-motion-ui/components/FittedFont.hh>
#include <a3-motion-ui/components/DeskMeter.hh>
#include <a3-motion-ui/components/fpv/BodyLook.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>
#include <a3-motion-ui/theme/TransportLook.hh>

namespace a3
{

namespace
{
// Fractions of what each thing hangs on.
constexpr float slotOfThird = 0.3f;      // a bar's slot, in its third: a fader's slot, not a field
constexpr float textOfSection = 0.7f;    // a line of text in its section
constexpr float playKeyOfClip = 0.3f;    // the play key's widest share of the clip row
constexpr float labelOfInstruments = 0.25f; // the bar label's share of the width
constexpr float captionOfBody = 0.8f;    // the caption step: a fifth under body text
constexpr float modeKeyOfHeader = 0.4f;  // the mode key's share of the header width
constexpr float targetOfClip = 0.55f;    // PATROL / G3's share of the clip row
constexpr float nameOfTarget = 0.75f;    // the clip name, smaller after the target
constexpr float discOfText = 1.f;        // the heaviest escort disc, to a line of text

constexpr char const *potLabels[3] = { "3D", "FREQ", "Q" };

juce::Font
boldFont (float height)
{
  return juce::Font (juce::FontOptions (fittedFontHeight (height, theme ().fontSize (FontRole::Header)))
                         .withStyle ("Bold"));
}

/** The caption step -- 3D, FREQ, Q -- below the body size, so the bars'
 *  names stay quieter than what the bars say. */
juce::Font
captionFont (float height)
{
  return juce::Font (
      juce::FontOptions (fittedFontHeight (
                             height, theme ().fontSize (FontRole::Body) * captionOfBody))
          .withStyle ("Bold"));
}

/** The height a line of bold text in a box of `boxHeight` is drawn at. */
float
lineHeightIn (float boxHeight)
{
  return fittedFontHeight (boxHeight * textOfSection,
                           theme ().fontSize (FontRole::Header));
}

juce::Rectangle<int>
headerContent (juce::Rectangle<int> header)
{
  return header.reduced (juce::roundToInt (theme ().padding));
}

juce::Rectangle<int>
clipContent (juce::Rectangle<int> clip)
{
  return clip.reduced (juce::roundToInt (theme ().padding),
                       juce::roundToInt (theme ().paddingSmall));
}

float
srgbToLinear (float channel)
{
  return channel <= 0.04045f ? channel / 12.92f
                             : std::pow ((channel + 0.055f) / 1.055f, 2.4f);
}

float
linearToSrgb (float channel)
{
  return channel <= 0.0031308f ? channel * 12.92f
                               : 1.055f * std::pow (channel, 1.f / 2.4f) - 0.055f;
}

/** `colour` with its light scaled by `k` (0..1). Scaling linear light keeps
 *  the chromaticity, so the hue stays what identifies the channel. */
juce::Colour
scaledLight (juce::Colour colour, float k)
{
  auto const scale = [k] (juce::uint8 channel) {
    auto const linear = srgbToLinear (static_cast<float> (channel) / 255.f) * k;
    return static_cast<juce::uint8> (
        juce::roundToInt (juce::jlimit (0.f, 1.f, linearToSrgb (linear)) * 255.f));
  };
  return juce::Colour (scale (colour.getRed ()), scale (colour.getGreen ()),
                       scale (colour.getBlue ()), colour.getAlpha ());
}

/** A key as StemDeck draws one: an opaque face of the skin's raised surface
 *  and a hairline. `lifted` is the "current" face -- the face with the skin's
 *  text washed in at the emphasis rung -- for a mode that is switched on. */
void
paintKeyFace (juce::Graphics &g, juce::Rectangle<float> key, bool lifted)
{
  auto const &t = theme ();
  auto face = toColour (t.surfaceRaised);
  if (lifted)
    face = face.overlaidWith (toColour (t.textPrimary, t.alphaFillEmphasis));
  g.setColour (face);
  g.fillRoundedRectangle (key, t.radiusControl);
  g.setColour (toColour (t.textPrimary, t.alphaOutline));
  g.drawRoundedRectangle (key.reduced (t.strokeThin / 2.f), t.radiusControl,
                          t.strokeThin);
}

void
paintHeader (juce::Graphics &g, FpvStrip const &strip, int channel,
             FpvChannel const &state)
{
  auto const plate = fpvHeaderPlate (strip.header);
  auto const plateColour = fpvPlateColour (state.colour);
  g.setColour (plateColour);
  g.fillRoundedRectangle (plate, theme ().radiusControl);
  g.setFont (boldFont (plate.getHeight () * textOfSection));
  g.setColour (fpvInkOn (plateColour));
  g.drawFittedText ("CH " + juce::String (channel + 1), plate.toNearestInt (),
                    juce::Justification::centred, 1, 0.5f);

  auto const key = fpvModeKey (strip.header);
  paintKeyFace (g, key, state.orbit);
  g.setFont (boldFont (key.getHeight () * textOfSection));
  g.setColour (toColour (theme ().textPrimary));
  g.drawFittedText (state.orbit ? "ORBIT" : "CLIP", key.toNearestInt (),
                    juce::Justification::centred, 1, 0.5f);
}

/** The disc the floor draws for that group, at the size of its weight. */
void
paintEscortDisc (juce::Graphics &g, juce::Rectangle<int> box, float lineHeight,
                 float mass, FlightTuning const &tuning)
{
  auto const full = lineHeight * discOfText;
  auto const diameter
      = full
        * juce::jmin (1.f, bodyWeightScale (mass, tuning)
                               / bodyWeightScale (tuning.hotspotMass, tuning)); // a hotspot fills discOfText
  g.setColour (toColour (bodyColour (BodyRole::Attract), theme ().alphaActive));
  g.fillEllipse (box.toFloat ()
                     .withWidth (full)
                     .withSizeKeepingCentre (diameter, diameter));
}

/** In ORBIT: PATROL or the escorted group, then the clip name smaller. */
void
paintOrbitTarget (juce::Graphics &g, juce::Rectangle<int> area,
                  FpvChannel const &channel, FlightTuning const &tuning)
{
  // The line of text as drawn, not the box: the disc stands for a word.
  auto const lineHeight = lineHeightIn (static_cast<float> (area.getHeight ()));
  auto target = area.removeFromLeft (juce::roundToInt (
      static_cast<float> (area.getWidth ()) * targetOfClip));
  area.removeFromLeft (juce::roundToInt (theme ().paddingSmall));
  g.setFont (boldFont (lineHeight));
  g.setColour (toColour (theme ().textPrimary));
  if (channel.escort < 0)
    g.drawFittedText ("PATROL", target, juce::Justification::centredLeft, 1,
                      0.5f);
  else
    {
      paintEscortDisc (g, target, lineHeight, channel.escortMass, tuning);
      g.setColour (toColour (theme ().textPrimary));
      g.drawFittedText (
          "G" + juce::String (channel.escort + 1),
          target.withTrimmedLeft (juce::roundToInt (lineHeight * discOfText)
                                  + juce::roundToInt (theme ().paddingSmall)),
          juce::Justification::centredLeft, 1, 0.5f);
    }

  g.setFont (juce::Font (juce::FontOptions (fittedFontHeight (
      static_cast<float> (area.getHeight ()) * textOfSection * nameOfTarget,
      theme ().fontSize (FontRole::Body)))));
  g.setColour (toColour (theme ().textPrimary, theme ().alphaSecondary));
  g.drawFittedText (channel.clipName, area, juce::Justification::centredLeft,
                    1, 0.5f);
}

/** The clip's state as a key: play's colour with the triangle while it
 *  runs, the idle face with the two bars while it does not. */
void
paintPlayKey (juce::Graphics &g, juce::Rectangle<float> key, bool playing)
{
  if (playing)
    {
      auto const lit = transportColour (TransportKey::PlayPause);
      g.setColour (lit);
      g.fillRoundedRectangle (key, theme ().radiusControl);
      g.setColour (fpvInkOn (lit));
      drawTransportGlyph (g, transportGlyphArea (key), TransportFace::PlayPause);
      return;
    }
  paintKeyFace (g, key, false);
  g.setColour (toColour (theme ().textPrimary));
  drawTransportGlyph (g, transportGlyphArea (key), TransportFace::Pause);
}

void
paintClip (juce::Graphics &g, FpvStrip const &strip, FpvChannel const &channel,
           FlightTuning const &tuning)
{
  auto const key = fpvPlayKey (strip.clip);
  auto const area = clipContent (strip.clip).withTrimmedRight (
      juce::roundToInt (key.getWidth () + theme ().paddingSmall));
  if (channel.orbit)
    paintOrbitTarget (g, area, channel, tuning);
  else
    {
      g.setFont (boldFont (static_cast<float> (area.getHeight ()) * textOfSection));
      g.setColour (toColour (theme ().textPrimary));
      g.drawFittedText (channel.clipName.isEmpty () ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94"))
                                                : channel.clipName,
                        area, juce::Justification::centredLeft, 1, 0.5f);
    }
  paintPlayKey (g, key, channel.playing);
}

/** 3D, FREQ and Q as fader slots: a recess in the skin's ground, filled in
 *  the channel's colour, the caption muted so the colour is said once. */
void
paintInstruments (juce::Graphics &g, FpvStrip const &strip,
                  FpvChannel const &channel)
{
  auto const &t = theme ();
  auto area = strip.instruments.reduced (juce::roundToInt (t.padding),
                                         juce::roundToInt (t.paddingSmall));
  auto const third = area.getHeight () / 3;
  for (size_t i = 0; i < 3; ++i)
    {
      auto row = area.removeFromTop (third);
      auto const label = row.removeFromLeft (juce::roundToInt (
          static_cast<float> (row.getWidth ()) * labelOfInstruments));
      g.setFont (captionFont (static_cast<float> (row.getHeight ()) * textOfSection));
      g.setColour (toColour (t.textMuted));
      g.drawFittedText (potLabels[i], label, juce::Justification::centredLeft,
                        1, 0.5f);

      auto const slot = row.withSizeKeepingCentre (
          row.getWidth (),
          juce::roundToInt (static_cast<float> (row.getHeight ()) * slotOfThird));
      g.setColour (toColour (t.background));
      g.fillRoundedRectangle (slot.toFloat (), t.radiusTick);
      // A hairline, as a key has: on a skin whose ground and surface are
      // nearly one grey, an empty slot would otherwise not be there at all.
      g.setColour (toColour (t.textPrimary, t.alphaOutline));
      g.drawRoundedRectangle (slot.toFloat ().reduced (t.strokeThin / 2.f),
                              t.radiusTick, t.strokeThin);
      auto const value = juce::jlimit (0.f, 1.f, channel.pots[i]);
      g.setColour (channel.colour);
      g.fillRoundedRectangle (
          slot.withWidth (juce::roundToInt (static_cast<float> (slot.getWidth ()) * value))
              .toFloat (),
          t.radiusTick);
    }
}
}

juce::Rectangle<float>
fpvModeKey (juce::Rectangle<int> header)
{
  auto area = headerContent (header).toFloat ();
  return area.removeFromRight (area.getWidth () * modeKeyOfHeader);
}

juce::Rectangle<float>
fpvHeaderPlate (juce::Rectangle<int> header)
{
  auto const area = headerContent (header).toFloat ();
  return area.withTrimmedRight (area.getWidth () * modeKeyOfHeader
                                + theme ().paddingSmall);
}

juce::Rectangle<float>
fpvPlayKey (juce::Rectangle<int> clip)
{
  auto area = clipContent (clip).toFloat ();
  auto const side
      = std::min (area.getHeight (), area.getWidth () * playKeyOfClip);
  return area.removeFromRight (side).withSizeKeepingCentre (side, side);
}

juce::Colour
fpvPlateColour (juce::Colour channel)
{
  auto const ceiling = relativeLuminance (toColour (theme ().textMuted));
  auto const own = relativeLuminance (channel);
  if (own <= ceiling)
    return channel;
  return scaledLight (channel, ceiling / own);
}

juce::Colour
fpvInkOn (juce::Colour ground)
{
  auto const dark = toColour (theme ().textOnAccent);
  auto const light = toColour (theme ().textPrimary);
  return contrastRatio (dark, ground) >= contrastRatio (light, ground) ? dark
                                                                       : light;
}

FpvStrips::FpvStrips ()
{
  // It fills every pixel it owns, and it is redrawn at the meters' rate: a
  // see-through component would drag its parent into each of those repaints.
  setOpaque (true);
  for (auto &meter : _meters)
    addAndMakeVisible (meter);
}

bool
FpvChannel::operator== (FpvChannel const &other) const
{
  return colour == other.colour && clipName == other.clipName
         && playing == other.playing && pots == other.pots
         && orbit == other.orbit && escort == other.escort
         && escortMass == other.escortMass;
}

bool
FpvStrips::setChannels (std::array<FpvChannel, 4> const &channels)
{
  if (channels == _channels)
    return false;
  _channels = channels;
  repaint ();
  return true;
}

int
FpvStrips::refreshMeters ()
{
  int redrawn = 0;
  for (size_t ch = 0; ch < _meters.size (); ++ch)
    redrawn += _meters[ch].setReading (
                   channelLevel ? channelLevel (static_cast<int> (ch))
                                : VuReading{})
                   ? 1
                   : 0;
  return redrawn;
}

void
FpvStrips::applyTheme ()
{
  resized ();
  repaint ();
}

void
FpvStrips::resized ()
{
  _strips = fpvStripRow (getLocalBounds (),
                          juce::roundToInt (theme ().paddingSmall));
  for (size_t ch = 0; ch < _meters.size (); ++ch)
    _meters[ch].setBounds (
        _strips[ch].meter.reduced (juce::roundToInt (theme ().padding)));
}

std::array<FpvStrip, 4> const &
FpvStrips::strips () const
{
  return _strips;
}

void
FpvStrips::paint (juce::Graphics &g)
{
  // The ground between and around the strips. Left unpainted, it was JUCE's
  // stock window grey showing through, which no skin can reach.
  g.fillAll (toColour (theme ().background));

  for (size_t ch = 0; ch < _strips.size (); ++ch)
    {
      auto const &strip = _strips[ch];
      auto const &channel = _channels[ch];

      // An opaque card of the skin's surface, as StemDeck's channel strip:
      // whose strip it is, the plate says, not a wash over the whole card.
      g.setColour (toColour (theme ().surface));
      g.fillRoundedRectangle (strip.whole.toFloat (), theme ().radiusCard);

      paintHeader (g, strip, static_cast<int> (ch), channel);
      paintClip (g, strip, channel, _tuning);
      paintInstruments (g, strip, channel);
    }
}

}
