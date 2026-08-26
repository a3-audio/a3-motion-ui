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

#include "ClipSettingsComponent.hh"

#include <a3-motion-ui/theme/Theme.hh>

#include <cmath>

namespace a3
{

ClipSettingsComponent::ClipSettingsComponent ()
{
  setInterceptsMouseClicks (false, false);
}

void
ClipSettingsComponent::setTarget (int channel, int slot,
                                  juce::Colour channelColour)
{
  _channel = channel;
  _slot = slot;
  _channelColour = channelColour;
  repaint ();
}

void
ClipSettingsComponent::setTrajectoryIcon (TrajectoryIconData const &icon)
{
  _trajectoryIcon = icon;
  repaint ();
}

void
ClipSettingsComponent::setTrajectoryName (juce::String const &name)
{
  _trajectoryName = name;
  repaint ();
}

void
ClipSettingsComponent::setElevationReach (float reach)
{
  _elevationReach = std::clamp (reach, 0.05f, 1.0f);
  repaint ();
}

void
ClipSettingsComponent::setElevationMirrorSouth (bool mirrorSouth)
{
  _elevationMirrorSouth = mirrorSouth;
  repaint ();
}

void
ClipSettingsComponent::setElevationClipTop (float clipTop)
{
  _elevationClipTop = std::clamp (clipTop, 0.0f, 1.0f);
  repaint ();
}

void
ClipSettingsComponent::setElevationClipBottom (float clipBottom)
{
  _elevationClipBottom = std::clamp (clipBottom, 0.0f, 1.0f);
  repaint ();
}

void
ClipSettingsComponent::setElevationFlat (bool flat)
{
  _elevationFlat = flat;
  repaint ();
}

void
ClipSettingsComponent::setElevationFlatElevation (float flatElevation)
{
  _elevationFlatElevation = std::clamp (flatElevation, 0.0f, 1.0f);
  repaint ();
}

void
ClipSettingsComponent::setElevationSubIndex (int subIndex)
{
  _elevationSubIndex = subIndex;
  repaint ();
}

void
ClipSettingsComponent::setMotionSpeed (float normalizedFrac,
                                       juce::String const &label)
{
  _motionSpeedFrac = std::clamp (normalizedFrac, 0.0f, 1.0f);
  _motionSpeedLabel = label;
  repaint ();
}

void
ClipSettingsComponent::setMotionDirection (int direction)
{
  _motionDirection = direction;
  repaint ();
}

void
ClipSettingsComponent::setMotionEndAction (int endAction)
{
  _motionEndAction = endAction;
  repaint ();
}

void
ClipSettingsComponent::setMotionSubIndex (int subIndex)
{
  _motionSubIndex = subIndex;
  repaint ();
}

void
ClipSettingsComponent::setFilterSweep (float sweep)
{
  _filterSweep = std::clamp (sweep, 0.0f, 1.0f);
  repaint ();
}

void
ClipSettingsComponent::setFilterQ (float q)
{
  _filterQ = std::clamp (q, 0.0f, 1.0f);
  repaint ();
}

void
ClipSettingsComponent::setFilterSubIndex (int subIndex)
{
  _filterSubIndex = subIndex;
  repaint ();
}

void
ClipSettingsComponent::setSelectedParameterIndex (int index)
{
  jassert (index >= 0 && index < numParameters);
  _selectedIndex = index;
  repaint ();
}

void
ClipSettingsComponent::setPotSizeScale (float scale)
{
  _potSizeScale = std::clamp (scale, 0.25f, 4.0f);
  repaint ();
}

void
ClipSettingsComponent::setLastControlReadout (juce::String const &text)
{
  _lastControlText = text;
  repaint ();
}

void
ClipSettingsComponent::paint (juce::Graphics &g)
{
  g.fillAll (juce::Colour (0, 0, 0).withAlpha (0.85f));

  // Frame the whole panel in the selected clip's channel colour, so it's
  // obvious at a glance which channel is currently shown.
  auto const frameThickness = juce::jmax (2, getHeight () / 60);
  g.setColour (_channelColour);
  g.drawRect (getLocalBounds (), frameThickness);

  // All sizing is derived from the actual height we're given (a fixed
  // fraction of the screen, see A3MotionUIComponent::resized()) rather than
  // fixed pixel constants, so the panel always fills it exactly.
  auto const paddingV = juce::jmax (4, getHeight () / 40);
  auto const headerH = juce::jmax (18, getHeight () / 12);

  auto area = getLocalBounds ().reduced (paddingH, paddingV);

  auto headerArea = area.removeFromTop (headerH);

  // Terminal-style readout of the last-operated control, top-right.
  auto readoutArea = headerArea.removeFromRight (headerArea.getWidth () / 2);
  g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName (),
                         fontFor (FontRole::Status, readoutArea, _lastControlText),
                         juce::Font::plain));
  g.setColour (juce::Colours::limegreen.withAlpha (0.85f));
  g.drawText (_lastControlText, readoutArea, juce::Justification::centredRight,
             true);

  auto const slotName = "Slot " + juce::String (_slot + 1);
  g.setFont (juce::Font (fontFor (FontRole::Status, headerArea, slotName),
                         juce::Font::bold));
  g.setColour (_channelColour);
  g.drawText (slotName, headerArea,
             juce::Justification::centredLeft, true);

  area.removeFromTop (juce::jmax (4, getHeight () / 50));

  // 4 vertical sections side by side, with a small gap between them.
  auto const gap = juce::jmax (3, getWidth () / 200);
  auto const sectionW = area.getWidth () / numParameters;

  // Shared knob/toggle diameter for every section (see class doc /
  // controlBounds()) — sized to comfortably fit Elevation's tightest
  // layout (its own small label row, then a graphic, then a 2x3 grid of
  // controls), then reused as-is by Motion/Filter's roomier single-row
  // layouts. Scales with _potSizeScale (Global Settings "Pot Size").
  auto const elevationLabelH = juce::jmax (9, area.getHeight () / 10);
  auto const elevationGraphicH = static_cast<int> (
      static_cast<float> (area.getHeight () - elevationLabelH) * 0.34f);
  auto const elevationGridH
      = area.getHeight () - elevationLabelH - elevationGraphicH;
  auto const elevationRowH = elevationGridH / 3;
  auto const elevationColW = sectionW / 2;
  auto const controlBoxBase = juce::jmin (elevationRowH, elevationColW);
  auto const knobDiam = juce::jmax (
      10, static_cast<int> (static_cast<float> (controlBoxBase) / 2.2f
                            * _potSizeScale));

  // One size for every caption in the bar, decided here rather than inside
  // each control. Fitting a caption to its own box drew "Q" three times the
  // height of "end-action" standing next to it — a size difference that reads
  // as a hierarchy which is not there. The sections all carve their columns
  // out of an equally wide card, so their geometry is known already at this
  // point.
  auto const cardW = sectionW - 2 * (gap / 2);
  auto const sectionContentW = cardW - 2 * juce::jmax (2, cardW / 12);
  auto const columnGap = juce::jmax (2, sectionContentW / 20);
  auto const controlBoxH
      = juce::jmin (elevationRowH,
                    static_cast<int> (static_cast<float> (knobDiam) * 2.2f))
        - 4;

  ControlMetrics const metrics{
    knobDiam,
    sharedCaptionSize (theme ().fontSize (FontRole::Label), sectionContentW,
                       columnGap, controlBoxH)
  };

  for (int i = 0; i < numParameters; ++i)
    {
      auto sectionBounds = (i < numParameters - 1)
                               ? area.removeFromLeft (sectionW)
                               : area;
      auto cardBounds = sectionBounds.reduced (gap / 2, 0);

      bool const isSelected = (i == _selectedIndex);
      if (i == trajectoryIndex)
        paintTrajectorySection (g, cardBounds, isSelected);
      else if (i == elevationIndex)
        paintElevationSection (g, cardBounds, isSelected, metrics);
      else if (i == motionIndex)
        paintMotionSection (g, cardBounds, isSelected, metrics);
      else
        paintFilterSection (g, cardBounds, isSelected, metrics);
    }
}

void
ClipSettingsComponent::paintSectionLabel (juce::Graphics &g,
                                          juce::Rectangle<int> labelArea,
                                          juce::String const &text,
                                          bool isSelected)
{
  g.setFont (juce::Font (fontFor (FontRole::Heading, labelArea, text),
                         juce::Font::plain));
  g.setColour (juce::Colours::white.withAlpha (isSelected ? 0.7f : 0.45f));
  g.drawFittedText (text, labelArea, juce::Justification::centredTop, 1);
}

juce::Rectangle<int>
ClipSettingsComponent::textCell (juce::Rectangle<int> cell, int knobDiam) const
{
  // As tall as the knob box, but the cell's full width: the knob is drawn at
  // its own diameter inside this, while the caption and value get the room the
  // grid actually gives them. Never taller than the cell, or a row's captions
  // are drawn over the row beneath it.
  auto const boxH = juce::jmin (
      cell.getHeight (), static_cast<int> (static_cast<float> (knobDiam) * 2.2f));

  return juce::Rectangle<int> (cell.getWidth (), boxH)
      .withCentre (cell.getCentre ());
}

juce::Rectangle<int>
ClipSettingsComponent::controlBounds (juce::Rectangle<int> cell,
                                      int knobDiam) const
{
  // Fixed size, tied only to knobDiam/Pot Size — never grows past its grid
  // cell, so a control's position/footprint stays put when Font Size
  // changes (a growing box here previously made neighbouring controls
  // visibly jump/overlap). Font Size instead widens only the *text-fit*
  // target inside paintMiniKnob/paintMiniToggle, not this box.
  auto const boxH = static_cast<int> (static_cast<float> (knobDiam) * 2.2f);
  return juce::Rectangle<int> (knobDiam, boxH).withCentre (cell.getCentre ());
}

float
ClipSettingsComponent::fontFor (FontRole role, juce::Rectangle<int> area,
                                juce::String const &text) const
{
  // drawFittedText shrinks a string to fit the width it is given, but never to
  // fit the height — a short string in a short box renders at full size and
  // spills over whatever sits below it. That is where the huge "1" came from,
  // while "direction" in the same row shrank away to nothing.
  auto size = juce::jmin (theme ().fontSize (role),
                          static_cast<float> (area.getHeight ()) * 0.85f);

  if (text.isEmpty ())
    return size;

  // And below a certain point drawFittedText gives up shrinking and cuts the
  // string instead — "Forward" became "F...". A caption drawn small is still
  // a caption; one that is cut is not, so width is clamped here as well.
  auto const width = juce::GlyphArrangement::getStringWidth (
      juce::Font (juce::FontOptions (size)), text);
  auto const room = static_cast<float> (area.getWidth ());

  if (width > room && width > 0.f)
    size *= room / width;

  return juce::jmax (7.f, size);
}

int
ClipSettingsComponent::labelRowHeight (juce::Rectangle<int> content,
                                       float captionSize) const
{
  // Tall enough for the caption rather than a fixed fraction of the box. A
  // quarter of the box was a sliver at any size, which is what forced
  // drawFittedText to shrink the caption until it was unreadable.
  auto const needed = static_cast<int> (captionSize * 1.25f);

  return juce::jlimit (10, juce::jmax (10, content.getHeight () / 2), needed);
}

void
ClipSettingsComponent::paintTrajectorySection (juce::Graphics &g,
                                               juce::Rectangle<int> bounds,
                                               bool isSelected)
{
  g.setColour (isSelected ? _channelColour.withAlpha (0.35f)
                          : juce::Colour (0x14ffffff));
  g.fillRoundedRectangle (bounds.toFloat (), 8.f);
  if (isSelected)
    g.drawRoundedRectangle (bounds.toFloat (), 8.f, 2.f);

  auto content = bounds.reduced (juce::jmax (2, bounds.getWidth () / 12), 4);
  auto labelArea = content.removeFromTop (
      juce::jmax (9, content.getHeight () / 10));
  paintSectionLabel (g, labelArea, parameterNames[trajectoryIndex], isSelected);

  auto nameArea = content.removeFromBottom (
      juce::jmax (14, static_cast<int> (theme ().fontSize (FontRole::Value)
                                        * 1.35f)));

  // Pictogram, centred in whatever square area is left above the name.
  auto const iconSize = static_cast<float> (
      juce::jmin (content.getWidth (), content.getHeight ()));
  auto iconArea = juce::Rectangle<float> (iconSize, iconSize)
                      .withCentre (content.toFloat ().getCentre ());
  drawTrajectoryIcon (g, iconArea, _trajectoryIcon,
                      isSelected ? _channelColour
                                : juce::Colours::white.withAlpha (0.75f));

  g.setFont (juce::Font (fontFor (FontRole::Value, nameArea, _trajectoryName),
                         juce::Font::bold));
  g.setColour (isSelected ? _channelColour
                          : juce::Colours::white.withAlpha (0.7f));
  g.drawFittedText (_trajectoryName, nameArea, juce::Justification::centred,
                    1);
}

void
ClipSettingsComponent::paintElevationSection (juce::Graphics &g,
                                              juce::Rectangle<int> bounds,
                                              bool isSelected,
                                              ControlMetrics metrics)
{
  g.setColour (isSelected ? _channelColour.withAlpha (0.35f)
                          : juce::Colour (0x14ffffff));
  g.fillRoundedRectangle (bounds.toFloat (), 8.f);
  if (isSelected)
    g.drawRoundedRectangle (bounds.toFloat (), 8.f, 2.f);

  auto content = bounds.reduced (juce::jmax (2, bounds.getWidth () / 12), 4);
  auto labelArea = content.removeFromTop (
      juce::jmax (9, content.getHeight () / 10));
  paintSectionLabel (g, labelArea, "Elevation", isSelected);

  // Graphic on top (passive visualization, highlighted whenever the active
  // sub-element affects it — reach/mirror-south/flat/flat-elevation) plus
  // all six controls below it in a 2x3 grid: reach/mirror-south on the
  // first row, clip-top/clip-bottom on the second, flat/flat-elevation on
  // the third.
  auto const graphicActive = _elevationSubIndex == 0 || _elevationSubIndex == 3
                             || _elevationSubIndex == 4
                             || _elevationSubIndex == 5;
  auto const gapV0 = juce::jmax (2, content.getHeight () / 20);
  auto graphicArea = content.removeFromTop (
      static_cast<int> (content.getHeight () * 0.34f));
  content.removeFromTop (gapV0);
  paintElevationGraphic (g, graphicArea, graphicActive, isSelected);

  auto const gapV = juce::jmax (2, content.getHeight () / 30);
  auto const rowH = (content.getHeight () - 2 * gapV) / 3;
  auto row1 = content.removeFromTop (rowH);
  content.removeFromTop (gapV);
  auto row2 = content.removeFromTop (rowH);
  content.removeFromTop (gapV);
  auto &row3 = content;

  auto const gapH = juce::jmax (2, content.getWidth () / 20);
  auto reachArea = row1.removeFromLeft (row1.getWidth () / 2 - gapH / 2);
  row1.removeFromLeft (gapH);
  auto const &mirrorArea = row1;

  auto clipTopArea = row2.removeFromLeft (row2.getWidth () / 2 - gapH / 2);
  row2.removeFromLeft (gapH);
  auto const &clipBottomArea = row2;

  auto flatArea = row3.removeFromLeft (row3.getWidth () / 2 - gapH / 2);
  row3.removeFromLeft (gapH);
  auto const &flatElevationArea = row3;

  paintMiniKnob (g, textCell (reachArea, metrics.knobDiam), metrics,
                 caption::reach, _elevationReach * 2.f - 1.f, false,
                 _elevationSubIndex == 0, isSelected);
  paintMiniToggle (g, textCell (mirrorArea, metrics.knobDiam), metrics,
                   caption::pole,
                   _elevationMirrorSouth ? "South" : "North",
                   _elevationSubIndex == 3, isSelected);
  paintMiniKnob (g, textCell (clipTopArea, metrics.knobDiam), metrics,
                 caption::clipTop, _elevationClipTop * 2.f - 1.f, false,
                 _elevationSubIndex == 1, isSelected);
  paintMiniKnob (g, textCell (clipBottomArea, metrics.knobDiam), metrics,
                 caption::clipBottom, _elevationClipBottom * 2.f - 1.f, false,
                 _elevationSubIndex == 2, isSelected);
  paintMiniToggle (g, textCell (flatArea, metrics.knobDiam), metrics,
                   caption::flat, _elevationFlat ? "On" : "Off",
                   _elevationSubIndex == 4, isSelected);
  paintMiniKnob (g, textCell (flatElevationArea, metrics.knobDiam), metrics,
                 caption::flatElevation, _elevationFlatElevation * 2.f - 1.f,
                 false, _elevationSubIndex == 5, isSelected);
}

void
ClipSettingsComponent::paintElevationGraphic (juce::Graphics &g,
                                              juce::Rectangle<int> bounds,
                                              bool isActive, bool isSelected)
{
  if (isActive && isSelected)
    {
      g.setColour (_channelColour.withAlpha (0.18f));
      g.fillRoundedRectangle (bounds.toFloat (), 4.f);
    }

  auto const iconColour = isSelected ? _channelColour
                                     : juce::Colours::white.withAlpha (0.75f);
  auto const r = static_cast<float> (
                     juce::jmin (bounds.getWidth (), bounds.getHeight ()))
                 * 0.42f;
  auto const centre = bounds.toFloat ().getCentre ();

  // Circle = side-on view of the sphere, north pole at the top, south pole
  // at the bottom (fraction 0..1 of its height maps linearly to that
  // range). clip-top/clip-bottom clamp that range in from each end (see
  // HeightMap::mapTo3D()); the remaining reachable band is [rangeLow,
  // rangeHigh].
  auto const rangeLow = std::clamp (_elevationClipTop, 0.f, 1.f);
  auto const rangeHigh = 1.f - std::clamp (_elevationClipBottom, 0.f, 1.f);
  bool const collapsed = rangeLow >= rangeHigh;
  auto const bandLow = collapsed ? (rangeLow + rangeHigh) * 0.5f
                                 : juce::jmin (rangeLow, rangeHigh);
  auto const bandHigh = collapsed ? bandLow : juce::jmax (rangeLow, rangeHigh);

  // Strictly monotonic model (no base-point interaction): the trajectory's
  // centre (r=0) always maps to one pole (poleFrac), its outer edge (r=1)
  // always maps to reach's point (edgeFrac) — mirror-south flips which
  // pole. Reproduced here in fractional (0..1) form since this view only
  // ever gets the plain elevation values, not a HeightMap.
  auto const poleFracRaw = _elevationMirrorSouth ? 1.f : 0.f;
  auto const edgeFracRaw = _elevationMirrorSouth
                               ? 1.f - _elevationReach
                               : _elevationReach;
  auto const poleFrac = std::clamp (poleFracRaw, bandLow, bandHigh);
  auto const edgeFrac = std::clamp (edgeFracRaw, bandLow, bandHigh);
  auto const sweepLow = juce::jmin (poleFrac, edgeFrac);
  auto const sweepHigh = juce::jmax (poleFrac, edgeFrac);
  auto const flatFrac
      = std::clamp (_elevationFlatElevation, bandLow, bandHigh);

  auto const fracToY
      = [&] (float frac) { return (centre.y - r) + frac * (r * 2.f); };

  juce::Path circlePath;
  circlePath.addEllipse (centre.x - r, centre.y - r, r * 2.f, r * 2.f);

  g.saveState ();
  g.reduceClipRegion (circlePath);

  // Excluded (clipped) zones — zero-height rects when bandLow/bandHigh
  // don't actually clip anything, so no explicit if-guard is needed.
  g.setColour (juce::Colours::black.withAlpha (0.55f));
  g.fillRect (juce::Rectangle<float> (centre.x - r, centre.y - r, r * 2.f,
                                      fracToY (bandLow) - (centre.y - r)));
  g.fillRect (juce::Rectangle<float> (
      centre.x - r, fracToY (bandHigh), r * 2.f,
      (centre.y + r) - fracToY (bandHigh)));

  if (!_elevationFlat)
    {
      g.setColour (iconColour.withAlpha (0.3f));
      g.fillRect (juce::Rectangle<float> (
          centre.x - r, fracToY (sweepLow), r * 2.f,
          juce::jmax (1.f, fracToY (sweepHigh) - fracToY (sweepLow))));
    }

  g.restoreState ();

  g.setColour (juce::Colours::black.withAlpha (0.5f));
  g.drawEllipse (centre.x - r, centre.y - r, r * 2.f, r * 2.f, 2.f);
  g.setColour (iconColour);
  g.drawEllipse (centre.x - r, centre.y - r, r * 2.f, r * 2.f, 1.f);

  auto const drawMarkerChord
      = [&] (float frac, float thinWidth, float boldWidth,
            juce::Colour colour) {
          auto const markerY = fracToY (frac);
          auto const dy = markerY - centre.y;
          if (std::abs (dy) > r)
            return;
          auto const halfWidth = std::sqrt (r * r - dy * dy);
          if (boldWidth > 0.f)
            {
              g.setColour (juce::Colours::black.withAlpha (0.5f));
              g.drawLine (centre.x - halfWidth, markerY + 1.f,
                         centre.x + halfWidth, markerY + 1.f, boldWidth);
            }
          g.setColour (colour);
          g.drawLine (centre.x - halfWidth, markerY, centre.x + halfWidth,
                     markerY, thinWidth);
        };

  if (_elevationFlat)
    {
      // flat marker: a single solid chord at the fixed elevation every
      // point of the trajectory sits at — no pole/reach cone to show.
      drawMarkerChord (flatFrac, 2.f, 3.f, iconColour);
    }
  else
    {
      // pole marker: a thin, neutral-coloured chord where the trajectory's
      // centre (r=0) sits — distinct from reach's bolder, channel-coloured
      // marker, so the two are visually separable at a glance.
      drawMarkerChord (poleFrac, 1.5f, 0.f,
                       juce::Colours::white.withAlpha (0.5f));

      // reach marker: a solid chord line at the pattern's outer-edge
      // position (r=1) — this line's position IS reach's value (mirrored
      // if mirror-south is on); turning reach moves it.
      drawMarkerChord (edgeFrac, 2.f, 3.f, iconColour);
    }

  // Head: a small dot at the centre (the listener, always at the sphere's
  // literal centre regardless of elevation settings).
  auto const headR = r * 0.16f;
  g.setColour (juce::Colours::black.withAlpha (0.6f));
  g.fillEllipse (centre.x - headR - 0.5f, centre.y - headR - 0.5f,
                headR * 2.f + 1.f, headR * 2.f + 1.f);
  g.setColour (iconColour);
  g.fillEllipse (centre.x - headR, centre.y - headR, headR * 2.f, headR * 2.f);
}

void
ClipSettingsComponent::paintMiniKnob (juce::Graphics &g,
                                      juce::Rectangle<int> bounds,
                                      ControlMetrics metrics,
                                      juce::String const &label,
                                      float angleFrac, bool fillFromZero,
                                      bool isActive, bool isSelected,
                                      juce::String const &valueText)
{
  bool const highlight = isActive && isSelected;
  if (highlight)
    {
      g.setColour (_channelColour.withAlpha (0.18f));
      g.fillRoundedRectangle (bounds.toFloat (), 4.f);
    }

  auto content = bounds.reduced (2);

  bool const hasValueText = valueText.isNotEmpty ();
  juce::Rectangle<int> valueArea;
  if (hasValueText)
    valueArea = content.removeFromTop (
        static_cast<int> (theme ().fontSize (FontRole::Value) * 1.25f));

  auto labelArea
      = content.removeFromBottom (labelRowHeight (content, metrics.captionSize));

  auto const knobColour = isSelected ? _channelColour
                                     : juce::Colours::white.withAlpha (0.75f);
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
  auto constexpr sweep = juce::MathConstants<float>::pi * 0.75f; // 135deg
  auto const angleValue = std::clamp (angleFrac, -1.0f, 1.0f) * sweep;

  juce::Path track;
  track.addCentredArc (centre.x, centre.y, r, r, 0.f, -sweep, sweep, true);
  g.setColour (juce::Colours::white.withAlpha (0.18f));
  g.strokePath (track, juce::PathStrokeType (juce::jmax (1.f, r * 0.16f)));

  // Bipolar params (e.g. wrap) fill from the centre out to the value;
  // unipolar params (e.g. clip-top/clip-bottom) fill from the sweep's
  // start, like a standard volume-style knob.
  juce::Path valueArc;
  auto const fromAngle = fillFromZero ? std::min (0.f, angleValue) : -sweep;
  auto const toAngle = fillFromZero ? std::max (0.f, angleValue) : angleValue;
  valueArc.addCentredArc (centre.x, centre.y, r, r, 0.f, fromAngle, toAngle,
                          true);
  g.setColour (knobColour);
  g.strokePath (valueArc, juce::PathStrokeType (juce::jmax (1.5f, r * 0.16f)));

  auto const tip = centre.getPointOnCircumference (r, angleValue);
  g.drawLine (centre.x, centre.y, tip.x, tip.y, juce::jmax (1.5f, r * 0.12f));

  auto const dotR = r * 0.22f;
  g.fillEllipse (juce::Rectangle<float> (dotR, dotR).withCentre (centre));

  if (hasValueText)
    {
      g.setFont (juce::Font (fontFor (FontRole::Value, valueArea, valueText),
                             juce::Font::bold));
      g.setColour (knobColour);
      g.drawFittedText (valueText, valueArea,
                        juce::Justification::centred, 1);
    }

  // The shared size, not this caption's own fit. Its box is only consulted as
  // a floor: a control box too short for the shared size would otherwise have
  // drawFittedText spill the caption over the row beneath it.
  g.setFont (juce::Font (juce::jmin (metrics.captionSize,
                                     static_cast<float> (labelArea.getHeight ())
                                         * 0.85f),
                         juce::Font::plain));
  g.setColour (juce::Colours::white.withAlpha (isSelected ? 0.85f : 0.55f));
  g.drawFittedText (label, labelArea,
                    juce::Justification::centred, 1);
}

void
ClipSettingsComponent::paintMiniToggle (juce::Graphics &g,
                                        juce::Rectangle<int> bounds,
                                        ControlMetrics metrics,
                                        juce::String const &label,
                                        juce::String const &stateText,
                                        bool isActive, bool isSelected)
{
  bool const highlight = isActive && isSelected;
  if (highlight)
    {
      g.setColour (_channelColour.withAlpha (0.18f));
      g.fillRoundedRectangle (bounds.toFloat (), 4.f);
    }

  auto content = bounds.reduced (2);
  auto labelArea
      = content.removeFromBottom (labelRowHeight (content, metrics.captionSize));

  auto const valueColour = isSelected ? _channelColour
                                      : juce::Colours::white.withAlpha (0.75f);
  g.setFont (juce::Font (fontFor (FontRole::Value, content, stateText),
                         juce::Font::bold));
  g.setColour (valueColour);
  g.drawFittedText (stateText, content,
                    juce::Justification::centred, 1);

  // The shared size, not this caption's own fit. Its box is only consulted as
  // a floor: a control box too short for the shared size would otherwise have
  // drawFittedText spill the caption over the row beneath it.
  g.setFont (juce::Font (juce::jmin (metrics.captionSize,
                                     static_cast<float> (labelArea.getHeight ())
                                         * 0.85f),
                         juce::Font::plain));
  g.setColour (juce::Colours::white.withAlpha (isSelected ? 0.85f : 0.55f));
  g.drawFittedText (label, labelArea,
                    juce::Justification::centred, 1);
}

void
ClipSettingsComponent::paintMotionSection (juce::Graphics &g,
                                           juce::Rectangle<int> bounds,
                                           bool isSelected,
                                           ControlMetrics metrics)
{
  g.setColour (isSelected ? _channelColour.withAlpha (0.35f)
                          : juce::Colour (0x14ffffff));
  g.fillRoundedRectangle (bounds.toFloat (), 8.f);
  if (isSelected)
    g.drawRoundedRectangle (bounds.toFloat (), 8.f, 2.f);

  auto content = bounds.reduced (juce::jmax (2, bounds.getWidth () / 12), 4);
  auto labelArea = content.removeFromTop (
      juce::jmax (9, content.getHeight () / 10));
  paintSectionLabel (g, labelArea, parameterNames[motionIndex], isSelected);

  // Single row, no graphic to share space with — speed as a knob,
  // direction/end-action as toggles (discrete, no continuous value).
  auto const gapH = juce::jmax (2, content.getWidth () / 20);
  auto const colW = (content.getWidth () - 2 * gapH) / 3;
  auto speedArea = content.removeFromLeft (colW);
  content.removeFromLeft (gapH);
  auto directionArea = content.removeFromLeft (colW);
  content.removeFromLeft (gapH);
  auto const &endActionArea = content;

  static constexpr char const *directionNames[]
      = { "Forward", "Reverse", "PingPong" };
  static constexpr char const *endActionNames[] = { "Loop", "Stop", "Bounce" };

  paintMiniKnob (g, textCell (speedArea, metrics.knobDiam), metrics,
                 caption::speed, _motionSpeedFrac * 2.f - 1.f, false,
                 _motionSubIndex == 0, isSelected, _motionSpeedLabel);
  paintMiniToggle (g, textCell (directionArea, metrics.knobDiam), metrics,
                   caption::direction, directionNames[_motionDirection],
                   _motionSubIndex == 1, isSelected);
  paintMiniToggle (g, textCell (endActionArea, metrics.knobDiam), metrics,
                   caption::endAction, endActionNames[_motionEndAction],
                   _motionSubIndex == 2, isSelected);
}

void
ClipSettingsComponent::paintFilterSection (juce::Graphics &g,
                                           juce::Rectangle<int> bounds,
                                           bool isSelected,
                                           ControlMetrics metrics)
{
  g.setColour (isSelected ? _channelColour.withAlpha (0.35f)
                          : juce::Colour (0x14ffffff));
  g.fillRoundedRectangle (bounds.toFloat (), 8.f);
  if (isSelected)
    g.drawRoundedRectangle (bounds.toFloat (), 8.f, 2.f);

  auto content = bounds.reduced (juce::jmax (2, bounds.getWidth () / 12), 4);
  auto labelArea = content.removeFromTop (
      juce::jmax (9, content.getHeight () / 10));
  paintSectionLabel (g, labelArea, parameterNames[filterIndex], isSelected);

  auto const gapH = juce::jmax (2, content.getWidth () / 20);
  auto sweepArea = content.removeFromLeft (content.getWidth () / 2 - gapH / 2);
  content.removeFromLeft (gapH);
  auto const &qArea = content;

  paintMiniKnob (g, textCell (sweepArea, metrics.knobDiam), metrics,
                 caption::sweep, _filterSweep * 2.f - 1.f, false,
                 _filterSubIndex == 0, isSelected);
  paintMiniKnob (g, textCell (qArea, metrics.knobDiam), metrics, caption::q,
                 _filterQ * 2.f - 1.f, false, _filterSubIndex == 1,
                 isSelected);
}

}
