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

#include <a3-motion-ui/components/ClipSettingsLayout.hh>

#include <a3-motion-ui/theme/Theme.hh>

#include <cmath>

namespace a3
{

namespace
{
// Opacities that describe a structure rather than a state: the panel over the
// sphere, the shading of the elevation graphic, the unlit part of a knob's
// track. State — selected, inactive, disabled — comes from the theme's alphas
// instead.
constexpr float panelOpacity = 0.85f;
constexpr float cardWash = 0.08f;
constexpr float highlightWash = 0.18f;
constexpr float trackWash = 0.18f;
constexpr float clippedZoneOpacity = 0.55f;
constexpr float outlineOpacity = 0.5f;
constexpr float headOpacity = 0.6f;
}

ClipSettingsComponent::ClipSettingsComponent ()
{
  // Not for itself, but for its children: the bar's own surface has nothing
  // to catch, the hit areas over the controls do.
  setInterceptsMouseClicks (false, true);

  // Until setTarget names a channel there is no channel colour to show.
  _channelColour = toColour (theme ().textPrimary);

  createTouchControls ();
}

void
ClipSettingsComponent::createTouchControls ()
{
  // Cards first, controls after: JUCE hit-tests front to back and puts the
  // most recently added child in front, so a tap on a knob reaches the knob
  // rather than the card it lies on.
  for (int section = 0; section < numParameters; ++section)
    {
      auto card = std::make_unique<TouchControl> ();
      // -1, not 0: a card is the section's free surface and names no
      // control. Saying 0 made touching the Elevation graphic arm reach.
      card->setIdentity (section, -1);
      card->onPress = [this] (int tappedSection, int) {
        if (onControlTapped)
          onControlTapped (tappedSection, -1);
      };
      addAndMakeVisible (*card);
      _sectionTouch[static_cast<size_t> (section)] = std::move (card);
    }

  // In front of the cards, so it swallows what would otherwise reach the
  // Elevation card. No callbacks: a picture is not a control.
  _elevationGraphicTouch = std::make_unique<TouchControl> ();
  addAndMakeVisible (*_elevationGraphicTouch);

  auto const makeButton
      = [this] (std::unique_ptr<TouchControl> &into,
                std::function<void ()> ClipSettingsComponent::*callback) {
          into = std::make_unique<TouchControl> ();
          into->onTap = [this, callback] (int, int) {
            if (this->*callback)
              (this->*callback) ();
          };
          addAndMakeVisible (*into);
        };

  // The grid: one hit area per cell, each carrying its channel and row.
  for (int col = 0; col < numChannelColumns; ++col)
    for (int row = 0; row < numChannelRows; ++row)
      {
        auto cell = std::make_unique<TouchControl> ();
        cell->setIdentity (col, row);
        cell->onDragIncrement
            = [this] (int channel, int gridRow, int increment) {
                if (onChannelValueDragged)
                  onChannelValueDragged (channel, gridRow, increment);
              };
        addAndMakeVisible (*cell);
        _gridTouch[static_cast<size_t> (col)][static_cast<size_t> (row)]
            = std::move (cell);
      }

  makeButton (_recModeTouch, &ClipSettingsComponent::onRecModePressed);
  makeButton (_menuTouch, &ClipSettingsComponent::onMenuPressed);
  makeButton (_recTouch, &ClipSettingsComponent::onRecordPressed);
  makeButton (_tapTouch, &ClipSettingsComponent::onTapPressed);

  for (int section = 0; section < numParameters; ++section)
    {
      auto const count = numControlsInSection (section);
      for (int sub = 0; sub < count; ++sub)
        {
          auto control = std::make_unique<TouchControl> ();
          control->setIdentity (section, sub);

          // Selection happens once, when the finger lands — not on every
          // increment. Re-selecting per increment let two fingers on two
          // controls trade the selection back and forth, and the highlight
          // flickered between them.
          control->onPress = [this] (int pressedSection, int pressedSub) {
            if (onControlTapped)
              onControlTapped (pressedSection, pressedSub);
          };

          control->onTap = [this] (int tappedSection, int tappedSub) {
            // A control with two or three states steps on right away:
            // tapping your way to a yes/no and then having to drag it as
            // well would be one move too many. Continuous values are
            // dragged, not tapped.
            if (tapAdvancesValue (tappedSection, tappedSub) && onControlDragged)
              onControlDragged (tappedSection, tappedSub, 1);
          };

          control->onDragIncrement
              = [this] (int draggedSection, int draggedSub, int increment) {
                  if (onControlDragged)
                    onControlDragged (draggedSection, draggedSub, increment);
                };

          addAndMakeVisible (*control);
          _controlTouch[static_cast<size_t> (section)].push_back (
              std::move (control));
        }
    }
}

void
ClipSettingsComponent::resized ()
{
  updateLayout ();

  for (int section = 0; section < numParameters; ++section)
    {
      auto const s = static_cast<size_t> (section);
      _sectionTouch[s]->setBounds (_layout.sectionCards[s]);

      auto const &cells = _layout.controls[s];
      for (size_t sub = 0; sub < _controlTouch[s].size (); ++sub)
        _controlTouch[s][sub]->setBounds (cells[sub]);
    }

  _elevationGraphicTouch->setBounds (_layout.elevationGraphic);

  for (int col = 0; col < numChannelColumns; ++col)
    for (int row = 0; row < numChannelRows; ++row)
      {
        auto const c = static_cast<size_t> (col);
        auto const r = static_cast<size_t> (row);
        _gridTouch[c][r]->setBounds (_layout.channelGrid[c][r]);
      }
  _recModeTouch->setBounds (_layout.recModeButton);
  _menuTouch->setBounds (_layout.menuButton);
  _recTouch->setBounds (_layout.recButton);
  _tapTouch->setBounds (_layout.tapButton);
}

juce::Colour
ClipSettingsComponent::cardColour (bool isSelected) const
{
  return isSelected ? _channelColour.withAlpha (theme ().alphaDisabled)
                    : toColour (theme ().textPrimary, cardWash);
}

juce::Colour
ClipSettingsComponent::controlColour (bool isSelected) const
{
  return isSelected ? _channelColour : toColour (theme ().textMuted);
}

juce::Colour
ClipSettingsComponent::captionColour (bool isSelected) const
{
  return toColour (theme ().textMuted,
                   isSelected ? 1.f : theme ().alphaInactive);
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
  _motionDirection = juce::jlimit (0, 1, direction);
  repaint ();
}

void
ClipSettingsComponent::setMotionEndAction (int endAction)
{
  _motionEndAction = juce::jlimit (0, 3, endAction);
  repaint ();
}



void
ClipSettingsComponent::setMotionFade (int sixteenths)
{
  _motionFade = juce::jlimit (0, 16, sixteenths);
  repaint ();
}

void
ClipSettingsComponent::setRecordLength (juce::String const &label)
{
  _recordLengthLabel = label;
  repaint ();
}

void
ClipSettingsComponent::setTrajectorySubIndex (int subIndex)
{
  _trajectorySubIndex = subIndex;
  repaint ();
}

void
ClipSettingsComponent::setMotionSubIndex (int subIndex)
{
  _motionSubIndex = subIndex;
  repaint ();
}

void
ClipSettingsComponent::setChannelValues (int channel, float freq, float q,
                                         float threeD)
{
  if (channel < 0 || channel >= numChannelColumns)
    return;

  auto const c = static_cast<size_t> (channel);
  _channelFreq[c] = std::clamp (freq, 0.f, 1.f);
  _channelQ[c] = std::clamp (q, 0.f, 1.f);
  _channelThreeD[c] = std::clamp (threeD, 0.f, 1.f);
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
ClipSettingsComponent::setLastControlReadout (juce::String const &text)
{
  _lastControlText = text;
  repaint ();
}

void
ClipSettingsComponent::paint (juce::Graphics &g)
{
  g.fillAll (toColour (theme ().surface, panelOpacity));

  updateLayout ();

  // A hairline, not a border. At height/60 the two panel frames were the
  // heaviest lines on the screen and boxed in what they only had to separate.
  auto const frameThickness = juce::jmax (1, getHeight () / 140);

  // Two panels side by side, not one panel with an odd section on the end.
  // The channel colour says "this is the shown clip's", so it must stop where
  // the clip's settings stop: what is in the strip belongs to all four
  // channels at once and cannot be framed as any one of them.
  g.setColour (_channelColour);
  g.drawRect (_layout.clipBounds, frameThickness);

  g.setColour (toColour (theme ().textPrimary, 0.25f));
  g.drawRect (_layout.globalBounds, frameThickness);

  // Terminal-style readout of the last-operated control, top-right.
  g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName (),
                         fontFor (FontRole::Header, _layout.readout,
                                  _lastControlText),
                         juce::Font::plain));
  g.setColour (toColour (theme ().accent, panelOpacity));
  g.drawText (_lastControlText, _layout.readout,
              juce::Justification::centredRight, true);

  auto const slotName = "Slot " + juce::String (_slot + 1);
  g.setFont (juce::Font (fontFor (FontRole::Header, _layout.slotLabel,
                                  slotName),
                         juce::Font::bold));
  g.setColour (_channelColour);
  g.drawText (slotName, _layout.slotLabel, juce::Justification::centredLeft,
              true);

  paintTrajectorySection (g, _selectedIndex == trajectoryIndex);
  paintElevationSection (g, _selectedIndex == elevationIndex);
  paintMotionSection (g, _selectedIndex == motionIndex);
  paintGlobalSection (g, _selectedIndex == globalIndex);
}

void
ClipSettingsComponent::setRecMode (RecMode mode)
{
  if (mode == _recMode)
    return;
  _recMode = mode;
  repaint ();
}

void
ClipSettingsComponent::applyTheme ()
{
  resized ();
}

void
ClipSettingsComponent::updateLayout ()
{
  // One calculation for the picture and for the hit areas. Two would be two
  // truths, and those drift apart the moment either one is touched.
  _layout = layOutClipSettings (getLocalBounds (),
                                theme ().fontSize (FontRole::Header),
                                theme ().fontSize (FontRole::Body),
                                theme ().potSize);
}

void
ClipSettingsComponent::paintSectionCard (juce::Graphics &g, int sectionIndex,
                                         bool isSelected)
{
  auto const card = _layout.sectionCards[static_cast<size_t> (sectionIndex)];

  g.setColour (cardColour (isSelected));
  g.fillRoundedRectangle (card.toFloat (), 8.f);
  if (isSelected)
    g.drawRoundedRectangle (card.toFloat (), 8.f, 2.f);

  paintSectionLabel (
      g, _layout.sectionLabels[static_cast<size_t> (sectionIndex)],
      parameterNames[sectionIndex], isSelected);
}

void
ClipSettingsComponent::paintGlobalSection (juce::Graphics &g,
                                           bool isSelected)
{
  paintSectionCard (g, globalIndex, isSelected);

  // Through textCell like every other control in the bar: handed the whole
  // remaining column instead, the value floated in the middle and its caption
  // sat pinned to the bottom edge, a finger's width away from what it names.
  paintChannelGrid (g);

  // The rec mode is a button like the three beside it: a tap steps it on.
  paintActionButton (g, _layout.recModeButton, recModeName (_recMode),
                     _recMode != RecMode::Touch);

  paintActionButton (g, _layout.menuButton, "MENU", false);
  paintActionButton (g, _layout.recButton, "REC", _recording);
  paintActionButton (g, _layout.tapButton, "TAP", false);
}

void
ClipSettingsComponent::paintActionButton (juce::Graphics &g,
                                          juce::Rectangle<int> bounds,
                                          juce::String const &label,
                                          bool isActive)
{
  // Filled rather than outlined: these are the only things in the bar that
  // do something when touched rather than hold a value, and they should not
  // read as another knob.
  g.setColour (isActive ? toColour (theme ().danger)
                        : toColour (theme ().textPrimary, cardWash * 2.f));
  g.fillRoundedRectangle (bounds.toFloat (), 5.f);

  g.setColour (toColour (theme ().textPrimary, highlightWash * 2.f));
  g.drawRoundedRectangle (bounds.toFloat (), 5.f, 1.f);

  g.setFont (juce::Font (fontFor (FontRole::Body, bounds.reduced (6, 4), label),
                         juce::Font::bold));
  g.setColour (isActive ? toColour (theme ().textOnAccent)
                        : toColour (theme ().textPrimary));
  g.drawFittedText (label, bounds, juce::Justification::centred, 1);
}

void
ClipSettingsComponent::setRecording (bool recording)
{
  if (recording == _recording)
    return;
  _recording = recording;
  repaint ();
}

void
ClipSettingsComponent::paintSectionLabel (juce::Graphics &g,
                                          juce::Rectangle<int> labelArea,
                                          juce::String const &text,
                                          bool isSelected)
{
  g.setFont (juce::Font (fontFor (FontRole::Header, labelArea, text),
                         juce::Font::plain));
  g.setColour (toColour (theme ().textPrimary, isSelected
                                                    ? theme ().alphaInactive
                                                    : theme ().alphaDisabled));
  g.drawFittedText (text, labelArea, juce::Justification::centredTop, 1);
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
ClipSettingsComponent::preferredHeight (int width) const
{
  // Same geometry the layout uses, asked before there is a layout: the knob
  // follows the section width and Pot Size, the boxes follow the knob and the
  // body font, and the bar follows the boxes.
  juce::ignoreUnused (width);
  auto const knobDiam
      = knobDiameterForFont (theme ().fontSize (FontRole::Body), theme ().potSize);

  return clipSettingsPreferredHeight (theme ().fontSize (FontRole::Header),
                                      theme ().fontSize (FontRole::Body),
                                      knobDiam);
}



void
ClipSettingsComponent::paintTrajectorySection (juce::Graphics &g,
                                               bool isSelected)
{
  paintSectionCard (g, trajectoryIndex, isSelected);

  auto const &metrics = _layout.metrics;

  // The length the next take will have. Not what is in the slot — that is what
  // the pictogram above shows.
  paintMiniToggle (g, _layout.controls[trajectoryIndex][1], metrics,
                   caption::recordLength, _recordLengthLabel,
                   _trajectorySubIndex == 1, isSelected);

  // Pictogram, centred in whatever square area is left above the name.
  auto const iconSize = static_cast<float> (
      juce::jmin (_layout.trajectoryIcon.getWidth (),
                  _layout.trajectoryIcon.getHeight ()));
  auto iconArea = juce::Rectangle<float> (iconSize, iconSize)
                      .withCentre (_layout.trajectoryIcon.toFloat ()
                                       .getCentre ());
  // The channel's colour, selected or not: the pictogram stands for the clip
  // that channel is holding, and it is the same shape in the same colour that
  // is drawn on the sphere. Which section is selected is already said by the
  // card behind it, so the icon does not have to say it again -- and saying it
  // by going white made a tapped take read as somebody else's.
  drawTrajectoryIcon (g, iconArea, _trajectoryIcon, _channelColour);

  // The pattern's name is a value like any other in the bar, and is drawn at
  // the size they share rather than filling whatever room this section has.
  //
  // Plain, not bold: a value already stands out against its caption by being
  // larger and brighter, and bold on top of that reads as shouting.
  g.setFont (juce::Font (
      juce::jmin (metrics.valueSize,
                  static_cast<float> (_layout.trajectoryName.getHeight ())
                      * 0.85f),
      juce::Font::plain));
  g.setColour (controlColour (isSelected));
  g.drawFittedText (_trajectoryName, _layout.trajectoryName,
                    juce::Justification::centred, 1);
}

void
ClipSettingsComponent::paintElevationSection (juce::Graphics &g,
                                              bool isSelected)
{
  paintSectionCard (g, elevationIndex, isSelected);

  auto const &metrics = _layout.metrics;
  auto const &cells = _layout.controls[elevationIndex];

  // Graphic on top (passive visualization, highlighted whenever the active
  // sub-element affects it — reach/mirror-south/flat/flat-elevation) plus
  // all six controls below it in a 2x3 grid: reach/mirror-south on the
  // first row, clip-top/clip-bottom on the second, flat/flat-elevation on
  // the third. The cells are ordered by sub-index, not by row.
  auto const graphicActive = _elevationSubIndex == 0 || _elevationSubIndex == 3
                             || _elevationSubIndex == 4
                             || _elevationSubIndex == 5;
  paintElevationGraphic (g, _layout.elevationGraphic, graphicActive,
                         isSelected);

  paintMiniKnob (g, cells[0], metrics, caption::reach,
                 _elevationReach * 2.f - 1.f, false, _elevationSubIndex == 0,
                 isSelected);
  paintMiniKnob (g, cells[1], metrics, caption::clipTop,
                 _elevationClipTop * 2.f - 1.f, false, _elevationSubIndex == 1,
                 isSelected);
  paintMiniKnob (g, cells[2], metrics, caption::clipBottom,
                 _elevationClipBottom * 2.f - 1.f, false,
                 _elevationSubIndex == 2, isSelected);
  paintMiniToggle (g, cells[3], metrics, caption::pole,
                   _elevationMirrorSouth ? value::south : value::north,
                   _elevationSubIndex == 3, isSelected);
  paintMiniToggle (g, cells[4], metrics, caption::flat,
                   _elevationFlat ? value::on : value::off,
                   _elevationSubIndex == 4, isSelected);
  paintMiniKnob (g, cells[5], metrics, caption::flatElevation,
                 _elevationFlatElevation * 2.f - 1.f, false,
                 _elevationSubIndex == 5, isSelected);
}

void
ClipSettingsComponent::paintElevationGraphic (juce::Graphics &g,
                                              juce::Rectangle<int> bounds,
                                              bool isActive, bool isSelected)
{
  if (isActive && isSelected)
    {
      g.setColour (_channelColour.withAlpha (highlightWash));
      g.fillRoundedRectangle (bounds.toFloat (), 4.f);
    }

  auto const iconColour = controlColour (isSelected);
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
  g.setColour (toColour (theme ().surface, clippedZoneOpacity));
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

  g.setColour (toColour (theme ().surface, outlineOpacity));
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
              g.setColour (toColour (theme ().surface, outlineOpacity));
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
                       toColour (theme ().textPrimary, outlineOpacity));

      // reach marker: a solid chord line at the pattern's outer-edge
      // position (r=1) — this line's position IS reach's value (mirrored
      // if mirror-south is on); turning reach moves it.
      drawMarkerChord (edgeFrac, 2.f, 3.f, iconColour);
    }

  // Head: a small dot at the centre (the listener, always at the sphere's
  // literal centre regardless of elevation settings).
  auto const headR = r * 0.16f;
  g.setColour (toColour (theme ().surface, headOpacity));
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
                                      bool isActive, bool isSelected)
{
  bool const highlight = isActive && isSelected;
  if (highlight)
    {
      g.setColour (_channelColour.withAlpha (highlightWash));
      g.fillRoundedRectangle (bounds.toFloat (), 4.f);
    }

  auto content = bounds.reduced (2);

  auto labelArea
      = content.removeFromBottom (textRowHeight (content, metrics.captionSize));

  auto const knobColour = controlColour (isSelected);
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
  g.setColour (toColour (theme ().textPrimary, trackWash));
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
      g.setColour (_channelColour.withAlpha (highlightWash));
      g.fillRoundedRectangle (bounds.toFloat (), 4.f);
    }

  auto content = bounds.reduced (2);
  auto labelArea
      = content.removeFromBottom (textRowHeight (content, metrics.captionSize));

  auto const valueColour = controlColour (isSelected);
  g.setFont (juce::Font (juce::jmin (metrics.valueSize,
                                     static_cast<float> (content.getHeight ())
                                         * 0.85f),
                         juce::Font::plain));
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
  g.setColour (captionColour (isSelected));
  g.drawFittedText (label, labelArea,
                    juce::Justification::centred, 1);
}

void
ClipSettingsComponent::paintMotionSection (juce::Graphics &g,
                                           bool isSelected)
{
  paintSectionCard (g, motionIndex, isSelected);

  auto const &metrics = _layout.metrics;
  auto const &cells = _layout.controls[motionIndex];

  // Single row, no graphic to share space with. Speed shows a quantized note
  // value, and a knob angle says nothing a reader of "1/4" does not already
  // know — so value and caption only, like the discrete ones beside it.
  paintMiniToggle (g, cells[0], metrics, caption::speed, _motionSpeedLabel,
                   _motionSubIndex == 0, isSelected);
  paintMiniToggle (g, cells[1], metrics, caption::direction,
                   value::directionNames[_motionDirection],
                   _motionSubIndex == 1, isSelected);
  paintMiniToggle (g, cells[2], metrics, caption::endAction,
                   value::endActionNames[_motionEndAction],
                   _motionSubIndex == 2, isSelected);
  // How much time at the take's end is spent travelling back to where it
  // began, rather than jumping there.
  paintMiniToggle (g, cells[3], metrics, caption::fade,
                   value::fadeName (_motionFade), _motionSubIndex == 3,
                   isSelected);
}

void
ClipSettingsComponent::paintChannelGrid (juce::Graphics &g)
{
  auto const &metrics = _layout.metrics;

  static char const *rowCaptions[numChannelRows] = { "freq", "Q", "3d" };

  // The row captions once down the side, rather than under all twelve knobs.
  g.setFont (juce::Font (metrics.captionSize, juce::Font::plain));
  g.setColour (toColour (theme ().textMuted, theme ().alphaInactive));
  for (int row = 0; row < numChannelRows; ++row)
    g.drawFittedText (rowCaptions[row],
                      _layout.channelRowLabels[static_cast<size_t> (row)],
                      juce::Justification::centredRight, 1);

  for (int col = 0; col < numChannelColumns; ++col)
    {
      auto const c = static_cast<size_t> (col);
      auto const colour = toColour (theme ().channel[c]);

      // The channel's own colour says which column is whose; a number over
      // it would only repeat what the colour already tells the eye.
      g.setFont (juce::Font (metrics.captionSize, juce::Font::bold));
      g.setColour (colour);
      g.drawFittedText (juce::String (col + 1), _layout.channelLabels[c],
                        juce::Justification::centred, 1);

      float const values[numChannelRows]
          = { _channelFreq[c], _channelQ[c], _channelThreeD[c] };

      for (int row = 0; row < numChannelRows; ++row)
        paintGridKnob (g, _layout.channelGrid[c][static_cast<size_t> (row)],
                       metrics, values[row], colour);
    }
}

/** A knob without a caption: the column says which channel, the row caption
 *  down the side says which value, so the knob itself has nothing to add. */
void
ClipSettingsComponent::paintGridKnob (juce::Graphics &g,
                                      juce::Rectangle<int> bounds,
                                      ControlMetrics metrics, float value,
                                      juce::Colour colour)
{
  // The same diameter every other knob in the bar is drawn at. Filling the
  // cell instead made these twelve the largest thing on screen, which is
  // not what they are.
  // A fifth over the bar's standard diameter — see the grid's layout: these
  // carry no caption, so at the same size they read smaller than the knobs
  // in the clip's sections.
  auto const size = static_cast<float> (juce::jmin (
      static_cast<int> (metrics.knobDiam * 1.2f),
      juce::jmin (bounds.getWidth (), bounds.getHeight ())));
  auto const centre = bounds.toFloat ().getCentre ();
  auto const r = size * 0.5f * 0.78f;

  auto constexpr sweep = juce::MathConstants<float>::pi * 0.75f;
  auto const angle = (std::clamp (value, 0.f, 1.f) * 2.f - 1.f) * sweep;

  juce::Path track;
  track.addCentredArc (centre.x, centre.y, r, r, 0.f, -sweep, sweep, true);
  g.setColour (toColour (theme ().textPrimary, trackWash));
  g.strokePath (track, juce::PathStrokeType (juce::jmax (1.f, r * 0.18f)));

  juce::Path valueArc;
  valueArc.addCentredArc (centre.x, centre.y, r, r, 0.f, -sweep, angle, true);
  g.setColour (colour);
  g.strokePath (valueArc, juce::PathStrokeType (juce::jmax (1.5f, r * 0.18f)));

  auto const tip = centre.getPointOnCircumference (r, angle);
  g.drawLine (centre.x, centre.y, tip.x, tip.y, juce::jmax (1.5f, r * 0.14f));
}

}
