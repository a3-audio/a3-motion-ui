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

#include <a3-motion-engine/flight/FlightTuning.hh>
#include <a3-motion-ui/components/FittedFont.hh>
#include <a3-motion-ui/components/fpv/BodyLook.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

namespace
{
// Fractions of what each thing hangs on.
constexpr float barOfThird = 0.5f;       // a bar's height in its third
constexpr float textOfSection = 0.7f;    // a line of text in its section
constexpr float glyphOfClip = 0.2f;      // the play state's share of the width
constexpr float labelOfInstruments = 0.2f; // the bar label's share of the width
constexpr float pillOfHeader = 0.4f;     // the mode pill's share of the header width
constexpr float pillOfSection = 0.8f;    // the pill's height in the header
constexpr float targetOfClip = 0.45f;    // PATROL / G3's share of the clip row
constexpr float nameOfTarget = 0.75f;    // the clip name, smaller after the target
constexpr float discOfText = 1.f;        // the heaviest escort disc, to a line of text
float const heaviestMass = FlightTuning{}.hotspotMass; // a hotspot fills discOfText

constexpr char const *potLabels[3] = { "3D", "FREQ", "Q" };

juce::Font
boldFont (float height)
{
  return juce::Font (juce::FontOptions (fittedFontHeight (height, theme ().fontSize (FontRole::Header)))
                         .withStyle ("Bold"));
}

juce::Rectangle<int>
headerContent (juce::Rectangle<int> header)
{
  return header.reduced (juce::roundToInt (theme ().padding));
}

void
paintModePill (juce::Graphics &g, juce::Rectangle<int> header, bool orbit)
{
  auto const pill = fpvModePill (header);
  auto const corner = pill.getHeight () / 2.f;
  auto const text = toColour (theme ().textPrimary);
  if (orbit)
    {
      g.setColour (text);
      g.fillRoundedRectangle (pill, corner);
      g.setColour (toColour (theme ().background));
    }
  else
    {
      g.setColour (text);
      g.drawRoundedRectangle (pill.reduced (theme ().strokeMedium / 2.f),
                              corner, theme ().strokeMedium);
    }
  g.setFont (boldFont (pill.getHeight () * textOfSection));
  g.drawFittedText (orbit ? "ORBIT" : "CLIP", pill.toNearestInt (),
                    juce::Justification::centred, 1, 0.5f);
}

void
paintHeader (juce::Graphics &g, FpvStrip const &strip, int channel,
             bool orbit)
{
  auto const area = headerContent (strip.header);
  g.setFont (boldFont (static_cast<float> (area.getHeight ()) * textOfSection));
  g.setColour (toColour (theme ().textPrimary));
  g.drawFittedText ("CH " + juce::String (channel + 1), area,
                    juce::Justification::centredLeft, 1, 0.5f);
  paintModePill (g, strip.header, orbit);
}

/** The disc the floor draws for that group, at the size of its weight. */
void
paintEscortDisc (juce::Graphics &g, juce::Rectangle<int> box, float mass)
{
  auto const full = static_cast<float> (box.getHeight ()) * textOfSection
                    * discOfText;
  auto const diameter
      = full
        * juce::jmin (1.f, bodyWeightScale (mass) / bodyWeightScale (heaviestMass));
  g.setColour (toColour (bodyColour (BodyRole::Attract), theme ().alphaActive));
  g.fillEllipse (box.toFloat ()
                     .withWidth (full)
                     .withSizeKeepingCentre (diameter, diameter));
}

/** In ORBIT: PATROL or the escorted group, then the clip name smaller. */
void
paintOrbitTarget (juce::Graphics &g, juce::Rectangle<int> area,
                  FpvChannel const &channel)
{
  auto const lineHeight = static_cast<float> (area.getHeight ()) * textOfSection;
  auto target = area.removeFromLeft (juce::roundToInt (
      static_cast<float> (area.getWidth ()) * targetOfClip));
  g.setFont (boldFont (lineHeight));
  g.setColour (toColour (theme ().textPrimary));
  if (channel.escort < 0)
    g.drawFittedText ("PATROL", target, juce::Justification::centredLeft, 1,
                      0.5f);
  else
    {
      paintEscortDisc (g, target, channel.escortMass);
      g.setColour (toColour (theme ().textPrimary));
      g.drawFittedText (
          "G" + juce::String (channel.escort + 1),
          target.withTrimmedLeft (juce::roundToInt (lineHeight * discOfText)
                                  + juce::roundToInt (theme ().paddingSmall)),
          juce::Justification::centredLeft, 1, 0.5f);
    }

  g.setFont (juce::Font (juce::FontOptions (fittedFontHeight (
      lineHeight * nameOfTarget, theme ().fontSize (FontRole::Body)))));
  g.setColour (toColour (theme ().textPrimary, theme ().alphaSecondary));
  g.drawFittedText (channel.clipName, area, juce::Justification::centredLeft,
                    1, 0.5f);
}

void
paintClip (juce::Graphics &g, FpvStrip const &strip, FpvChannel const &channel)
{
  auto area = strip.clip.reduced (juce::roundToInt (theme ().padding),
                                  juce::roundToInt (theme ().paddingSmall));
  auto const glyph = area.removeFromRight (juce::roundToInt (
      static_cast<float> (area.getWidth ()) * glyphOfClip));
  if (channel.orbit)
    paintOrbitTarget (g, area, channel);
  else
    {
      g.setFont (boldFont (static_cast<float> (area.getHeight ()) * textOfSection));
      g.setColour (toColour (theme ().textPrimary));
      g.drawFittedText (channel.clipName.isEmpty () ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94"))
                                                : channel.clipName,
                        area, juce::Justification::centredLeft, 1, 0.5f);
    }
  g.setFont (boldFont (static_cast<float> (glyph.getHeight ()) * textOfSection));
  g.setColour (toColour (theme ().textPrimary));
  g.drawFittedText (channel.playing ? juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6"))
                                    : juce::String (juce::CharPointer_UTF8 ("\xe2\x9d\x9a\xe2\x9d\x9a")),
                    glyph, juce::Justification::centredRight, 1, 0.5f);
}

void
paintInstruments (juce::Graphics &g, FpvStrip const &strip,
                  FpvChannel const &channel)
{
  auto area = strip.instruments.reduced (juce::roundToInt (theme ().padding),
                                         juce::roundToInt (theme ().paddingSmall));
  auto const third = area.getHeight () / 3;
  for (size_t i = 0; i < 3; ++i)
    {
      auto row = area.removeFromTop (third);
      auto const label = row.removeFromLeft (juce::roundToInt (
          static_cast<float> (row.getWidth ()) * labelOfInstruments));
      g.setFont (boldFont (static_cast<float> (row.getHeight ()) * textOfSection));
      g.setColour (toColour (theme ().textPrimary));
      g.drawFittedText (potLabels[i], label, juce::Justification::centredLeft,
                        1, 0.5f);

      auto const bar = row.withSizeKeepingCentre (
          row.getWidth (),
          juce::roundToInt (static_cast<float> (row.getHeight ()) * barOfThird));
      g.setColour (toColour (theme ().textPrimary, theme ().alphaGuide));
      g.fillRoundedRectangle (bar.toFloat (), theme ().radiusCard);
      auto const value = juce::jlimit (0.f, 1.f, channel.pots[i]);
      g.setColour (channel.colour);
      g.fillRoundedRectangle (
          bar.withWidth (juce::roundToInt (static_cast<float> (bar.getWidth ()) * value))
              .toFloat (),
          theme ().radiusCard);
    }
}
}

juce::Rectangle<float>
fpvModePill (juce::Rectangle<int> header)
{
  auto const area = headerContent (header).toFloat ();
  return area.withTrimmedLeft (area.getWidth () * (1.f - pillOfHeader))
      .withSizeKeepingCentre (area.getWidth () * pillOfHeader,
                              area.getHeight () * pillOfSection);
}

void
FpvStrips::setChannels (std::array<FpvChannel, 4> const &channels)
{
  _channels = channels;
  repaint ();
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
}

std::array<FpvStrip, 4> const &
FpvStrips::strips () const
{
  return _strips;
}

void
FpvStrips::paint (juce::Graphics &g)
{
  for (size_t ch = 0; ch < _strips.size (); ++ch)
    {
      auto const &strip = _strips[ch];
      auto const &channel = _channels[ch];

      g.setColour (channel.colour.withAlpha (theme ().alphaFill));
      g.fillRoundedRectangle (strip.whole.toFloat (), theme ().radiusCard);

      paintHeader (g, strip, static_cast<int> (ch), channel.orbit);
      paintClip (g, strip, channel);
      paintInstruments (g, strip, channel);
      paintVuMeter (g, strip.meter.reduced (juce::roundToInt (theme ().padding)),
                    channelLevel ? channelLevel (static_cast<int> (ch))
                                 : VuReading{},
                    VuDirection::Right);
    }
}

}
