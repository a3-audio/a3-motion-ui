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

#include <a3-motion-ui/components/BarKnob.hh>

#include <a3-motion-engine/Envelope.hh>
#include <a3-motion-engine/TempoLfo.hh>

#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-engine/TrajectorySpin.hh>

#include <a3-motion-ui/components/ClipSettingsLayout.hh>

#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/TransportLook.hh>

#include <cmath>

namespace a3
{

namespace
{
/** Pages that cover the clip area with something of their own. The bar's
 *  sections must not be drawn under them -- a page that does not fill every
 *  pixel would otherwise show the clip settings through its own gaps, which is
 *  what the ACTION page did on its first evening. */
bool
isFullPage (BarPage page)
{
  return page == BarPage::Controller || page == BarPage::Browser
         || page == BarPage::Action;
}
}

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
/** How much the TAP key comes up on a beat. A fifth of the wash a press
 *  makes: this is the metronome you notice without looking at it, and it was
 *  taken out once already for being louder than that. */
constexpr float beatWash = 0.035f;
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

  auto const makeTab = [this] (std::unique_ptr<TouchControl> &into,
                               BarPage page) {
    into = std::make_unique<TouchControl> ();
    into->onTap = [this, page] (int, int) {
      if (onPageSelected)
        onPageSelected (page);
    };
    addAndMakeVisible (*into);
  };

  for (index_t slot = 0; slot < numPadSlots; ++slot)
    {
      auto &touch = _slotTouch[slot];
      touch = std::make_unique<TouchControl> ();
      touch->onTap = [this, slot] (int, int) {
        if (onSlotSelected)
          onSlotSelected (slot);
      };
      addAndMakeVisible (*touch);
    }

  for (size_t channel = 0; channel < numChannelColumns; ++channel)
    {
      auto &face = _faceTouch[channel];
      face = std::make_unique<TouchControl> ();
      face->onTap = [this, channel] (int, int) {
        if (onChannelFaceTapped)
          onChannelFaceTapped (static_cast<index_t> (channel));
      };
      addAndMakeVisible (*face);

    }

  for (int i = 0; i < numTransportKeys; ++i)
    {
      auto const key = transportKeyOrder[i];
      auto &touch = _transportTouch[static_cast<size_t> (i)];
      touch = std::make_unique<TouchControl> ();

      if (key == TransportKey::Action)
        {
          // Held, like the pad it stands for: the accent lasts as long as the
          // finger does.
          touch->onPress = [this] (int, int) {
            if (onTransportActionHeld)
              onTransportActionHeld (true);
          };
          touch->onRelease = [this] (int, int) {
            if (onTransportActionHeld)
              onTransportActionHeld (false);
          };
        }
      else
        {
          touch->onTap = [this, key] (int, int) {
            if (onTransportTapped)
              onTransportTapped (key);
          };
        }
      addAndMakeVisible (*touch);
    }

  makeTab (_tabClipTouch, BarPage::Clip);
  makeTab (_tabRecordTouch, BarPage::Record);
  makeTab (_tabActionTouch, BarPage::Action);
  makeTab (_tabControllerTouch, BarPage::Controller);
  makeTab (_tabBrowserTouch, BarPage::Browser);

  // In front of the cards, so it swallows what would otherwise reach the
  // Elevation card. No callbacks: a picture is not a control.
  _elevationGraphicTouch = std::make_unique<TouchControl> ();
  // The graphic is a picture of what the controls under it do, so it names
  // no control — but it is inside the section, and touching a section
  // should select it. -1 says "the section, not one of its controls".
  _elevationGraphicTouch->onPress = [this] (int, int) {
    if (onControlTapped)
      onControlTapped (elevationIndex, -1);
  };

  // The graphic is a control the finger carries: the axis sits under it and
  // follows it, which is what a line you can see and touch has to do.
  //
  // Not increments. Those arrive once per drag threshold, twelve pixels
  // apart, so the line lurched a step at a time and never sat where the
  // finger was -- and a step small enough to be fine made the whole sphere
  // take thousands of pixels to cross.
  auto const setBaseAt = [this] (juce::Point<int> at) {
    if (!onElevationBaseSet)
      return;

    auto const low = std::clamp (_elevationClipTop, 0.f, 1.f);
    auto const high = 1.f - std::clamp (_elevationClipBottom, 0.f, 1.f);

    onElevationBaseSet (snapElevationBase (elevationBaseAt (
        _layout.elevationGraphic.withZeroOrigin (), at.y,
        juce::jmin (low, high), juce::jmax (low, high))));
  };

  _elevationGraphicTouch->onTapAt
      = [setBaseAt] (int, int, juce::Point<int> at) { setBaseAt (at); };
  _elevationGraphicTouch->onDragTo
      = [setBaseAt] (int, int, juce::Point<int> at) { setBaseAt (at); };
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

  for (int i = 0; i < numRecordLengths; ++i)
    {
      auto button = std::make_unique<TouchControl> ();
      button->setIdentity (i);
      button->onTap = [this] (int index, int) {
        if (onRecordLengthChosen)
          onRecordLengthChosen (index);
      };
      addAndMakeVisible (*button);
      _lengthTouch[static_cast<size_t> (i)] = std::move (button);
    }

  // The speeds sit in the same room as the lengths, on the section's other
  // face — see setPage(), which is what decides who may be touched.
  for (int i = 0; i < numSpeedButtons; ++i)
    {
      auto button = std::make_unique<TouchControl> ();
      button->setIdentity (i);
      button->onTap = [this] (int index, int) {
        if (onSpeedChosen)
          onSpeedChosen (index);
      };
      addAndMakeVisible (*button);
      _speedTouch[static_cast<size_t> (i)] = std::move (button);
    }

  makeButton (_recModeTouch, &ClipSettingsComponent::onRecModePressed);
  makeButton (_clockModeTouch, &ClipSettingsComponent::onClockModePressed);
  makeButton (_menuTouch, &ClipSettingsComponent::onMenuPressed);
  makeButton (_recTouch, &ClipSettingsComponent::onRecordPressed);
  makeButton (_tapTouch, &ClipSettingsComponent::onTapPressed);
  _tapTouch->onPress = [this] (int, int) { flashTap (); };

  // Held, not tapped, and so it needs onRelease rather than onTap — see
  // TouchControl, where the two are deliberately different things.
  _shiftTouch = std::make_unique<TouchControl> ();
  _shiftTouch->onPress = [this] (int, int) {
    setShiftHeld (true);
    if (onShiftHeld)
      onShiftHeld (true);
  };
  _shiftTouch->onRelease = [this] (int, int) {
    setShiftHeld (false);
    if (onShiftHeld)
      onShiftHeld (false);
  };
  addAndMakeVisible (*_shiftTouch);

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
            // A control with few states changes right away: tapping your
            // way to a yes/no and then having to drag it as well would be
            // one move too many. Continuous values are dragged, not tapped.
            if (tapTogglesValue (tappedSection, tappedSub) && onControlToggled)
              onControlToggled (tappedSection, tappedSub);
            else if (tapAdvancesValue (tappedSection, tappedSub)
                     && onControlDragged)
              onControlDragged (tappedSection, tappedSub, 1);
          };

          // Two taps put a knob back where it started. Only the ones you
          // turn: a control that steps on a tap has no middle to go back to.
          control->onDoubleTap = [this] (int tappedSection, int tappedSub) {
            if (tapTogglesValue (tappedSection, tappedSub)
                || tapAdvancesValue (tappedSection, tappedSub))
              return;

            if (onControlReset)
              onControlReset (tappedSection, tappedSub);
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

  _shiftTouch->setBounds (_layout.shiftButton);

  for (index_t slot = 0; slot < numPadSlots; ++slot)
    _slotTouch[slot]->setBounds (_layout.slotButtons[slot]);

  for (int i = 0; i < numTransportKeys; ++i)
    _transportTouch[static_cast<size_t> (i)]->setBounds (
        _layout.transportButtons[static_cast<size_t> (i)]);

  _tabClipTouch->setBounds (_layout.tabClip);
  for (size_t channel = 0; channel < numChannelColumns; ++channel)
    _faceTouch[channel]->setBounds (_layout.channelFaces[channel]);
  _tabRecordTouch->setBounds (_layout.tabRecord);
  _tabActionTouch->setBounds (_layout.tabAction);
  _tabControllerTouch->setBounds (_layout.tabController);
  _tabBrowserTouch->setBounds (_layout.tabBrowser);

  for (int col = 0; col < numChannelColumns; ++col)
    for (int row = 0; row < numChannelRows; ++row)
      {
        auto const c = static_cast<size_t> (col);
        auto const r = static_cast<size_t> (row);
        _gridTouch[c][r]->setBounds (_layout.channelGrid[c][r]);
      }
  for (int i = 0; i < numRecordLengths; ++i)
    _lengthTouch[static_cast<size_t> (i)]->setBounds (
        _layout.lengthButtons[static_cast<size_t> (i)]);

  for (int i = 0; i < numSpeedButtons; ++i)
    _speedTouch[static_cast<size_t> (i)]->setBounds (
        _layout.speedButtons[static_cast<size_t> (i)]);

  _recModeTouch->setBounds (_layout.recModeButton);
  _clockModeTouch->setBounds (_layout.clockModeButton);
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
ClipSettingsComponent::setElevationReach (float reach, float swept)
{
  _elevationReach = std::clamp (reach, 0.05f, 1.0f);
  _elevationReachSwept = swept < 0.f ? -1.f : std::clamp (swept, 0.05f, 1.0f);
  repaint ();
}

void
ClipSettingsComponent::setElevationBase (float base)
{
  _elevationBase = std::clamp (base, 0.f, 1.f);
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
ClipSettingsComponent::setMotionActMode (int mode)
{
  if (mode == _motionActMode)
    return;

  _motionActMode = mode;
  repaint ();
}



void
ClipSettingsComponent::setMotionBridgeBias (int bias)
{
  auto const held = juce::jlimit (-4, 4, bias);
  if (held == _motionBridgeBias)
    return;
  _motionBridgeBias = held;
  repaint ();
}

void
ClipSettingsComponent::setMotionSqueeze (float squeezeX, float squeezeY)
{
  auto const heldX = juce::jlimit (-1.f, 1.f, squeezeX);
  auto const heldY = juce::jlimit (-1.f, 1.f, squeezeY);
  if (juce::approximatelyEqual (heldX, _motionSqueezeX)
      && juce::approximatelyEqual (heldY, _motionSqueezeY))
    return;

  _motionSqueezeX = heldX;
  _motionSqueezeY = heldY;
  repaint ();
}

void
ClipSettingsComponent::setMotionFadeReach (float reach)
{
  _motionFadeReach = juce::jlimit (0.f, 1.f, reach);
  repaint ();
}

void
ClipSettingsComponent::setMotionEnvelopeMax (float value)
{
  auto const clamped = juce::jlimit (0.f, 1.f, value);
  if (juce::approximatelyEqual (clamped, _motionEnvelopeMax))
    return;

  _motionEnvelopeMax = clamped;
  repaint ();
}

void
ClipSettingsComponent::setShapeSpeed (int speedLog2)
{
  if (speedLog2 == _speedLog2)
    return;

  _speedLog2 = speedLog2;
  repaint ();
}

void
ClipSettingsComponent::setShapeRotate (float rotate, float reach)
{
  if (juce::approximatelyEqual (rotate, _shapeRotate)
      && juce::approximatelyEqual (reach, _shapeRotateReach))
    return;

  _shapeRotate = rotate;
  _shapeRotateReach = reach;
  repaint ();
}

void
ClipSettingsComponent::setMotionEnvelope (int attackStep, int decayStep)
{
  auto const attack = juce::jlimit (0, envelopeMaxStep, attackStep);
  auto const decay = juce::jlimit (0, envelopeMaxStep, decayStep);
  if (attack == _motionAttack && decay == _motionDecay)
    return;

  _motionAttack = attack;
  _motionDecay = decay;
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
ClipSettingsComponent::setChannelValues (int channel, float freq,
                                         float freqEffective, float q,
                                         float qEffective, float threeD,
                                         float threeDEffective)
{
  if (channel < 0 || channel >= numChannelColumns)
    return;

  auto const c = static_cast<size_t> (channel);

  // Through gridKnobReach() for all three, so no row can quietly be given its
  // own value as its reach again -- which is how freq and Q came to be drawn
  // standing still while the engine was sending them moving.
  auto const set = std::array<float, 3>{ std::clamp (freq, 0.f, 1.f),
                                         std::clamp (q, 0.f, 1.f),
                                         std::clamp (threeD, 0.f, 1.f) };
  auto const reach = std::array<float, 3>{
    gridKnobReach (freq, freqEffective), gridKnobReach (q, qEffective),
    gridKnobReach (threeD, threeDEffective)
  };

  if (juce::approximatelyEqual (_channelFreq[c], set[0])
      && juce::approximatelyEqual (_channelQ[c], set[1])
      && juce::approximatelyEqual (_channelThreeD[c], set[2])
      && juce::approximatelyEqual (_channelFreqReach[c], reach[0])
      && juce::approximatelyEqual (_channelQReach[c], reach[1])
      && juce::approximatelyEqual (_channelThreeDReach[c], reach[2]))
    return; // nothing moved; this runs on every LED tick

  _channelFreq[c] = set[0];
  _channelQ[c] = set[1];
  _channelThreeD[c] = set[2];
  _channelFreqReach[c] = reach[0];
  _channelQReach[c] = reach[1];
  _channelThreeDReach[c] = reach[2];
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

  // Not on the pages that show every slot at once -- the pads page and the
  // browser -- where choosing one of them says something untrue about what you
  // are looking at. Both of
  // the clip's faces describe a single slot, and the record face -- the take
  // about to be written -- is the one where being sure which slot it is
  // matters most.
  if (!isFullPage (_page))
    for (index_t slot = 0; slot < numPadSlots; ++slot)
      {
        auto const bounds = _layout.slotButtons[slot];
        if (bounds.isEmpty ())
          continue;

        // The one you are looking at is filled in the channel's colour, the
        // other outlined -- the same way the page tabs beside them say which
        // page you are on, because they answer the same kind of question.
        auto const here = slot == static_cast<index_t> (_slot);

        g.setColour (here ? _channelColour.withAlpha (0.35f)
                          : toColour (theme ().textPrimary, 0.06f));
        g.fillRoundedRectangle (bounds.toFloat (), 3.f);
        g.setColour (toColour (theme ().textPrimary, here ? 0.35f : 0.15f));
        g.drawRoundedRectangle (bounds.toFloat (), 3.f, 1.f);

        // The number alone. "Slot" was three quarters of a key spent saying
        // what two keys side by side already say.
        auto const name = juce::String (slot + 1);
        g.setFont (juce::Font (fontFor (FontRole::Header, bounds, name),
                               here ? juce::Font::bold : juce::Font::plain));
        g.setColour (here ? _channelColour
                          : toColour (theme ().textPrimary, 0.55f));
        g.drawFittedText (name, bounds, juce::Justification::centred, 1);

        // Unsaved. In the warning colour rather than the danger one: nothing
        // is lost yet, something is merely waiting to be written.
        if (_slotDrifted[slot])
          {
            g.setColour (toColour (theme ().warning));
            g.fillEllipse (driftMark (bounds).toFloat ());
          }
      }

  paintTabs (g);

  // The readout has left this band. It is in the status bar now, beside the
  // tempo and the beat -- the row the device says what it is doing on -- and
  // the band it stood in is where the transport keys go.

  // The global strip stands on both pages: recmode, clock, MENU, REC and TAP
  // belong to the device rather than to the clip, and losing them while you
  // are firing clips is exactly the wrong moment to lose them.
  paintGlobalSection (g, sectionCarriesSelection (globalIndex)
                             && _selectedIndex == globalIndex);

  if (isFullPage (_page))
    return; // ControllerComponent / BrowserComponent draws the rest

  paintTrajectorySection (g, _selectedIndex == trajectoryIndex);
  paintElevationSection (g, _selectedIndex == elevationIndex);
  paintMotionSection (g, _selectedIndex == motionIndex);

  // Last, so it covers whichever section it belongs to.
}

void
ClipSettingsComponent::setTransportState (bool playing, bool recording)
{
  if (playing == _transportPlaying && recording == _transportRecording)
    return;

  _transportPlaying = playing;
  _transportRecording = recording;
  repaint ();
}

void
ClipSettingsComponent::paintTabs (juce::Graphics &g)
{
  auto const paintTab = [&] (juce::Rectangle<int> bounds,
                             juce::String const &label, bool active) {
    if (bounds.isEmpty ())
      return;

    // The active one is filled and the other is outlined: which page you are
    // on has to be answerable with a glance, not by reading two words and
    // working out which is bolder.
    g.setColour (active ? _channelColour.withAlpha (0.35f)
                        : toColour (theme ().textPrimary, 0.06f));
    g.fillRoundedRectangle (bounds.toFloat (), 3.f);
    g.setColour (toColour (theme ().textPrimary, active ? 0.35f : 0.15f));
    g.drawRoundedRectangle (bounds.toFloat (), 3.f, 1.f);

    g.setFont (juce::Font (fontFor (FontRole::Header, bounds, label),
                           active ? juce::Font::bold : juce::Font::plain));
    g.setColour (toColour (theme ().textPrimary, active ? 1.f : 0.55f));
    g.drawFittedText (label, bounds, juce::Justification::centred, 1);
  };

  // The four transport keys. The mark always carries the action's own colour;
  // the key's ground lights only while it is doing something, so the row reads
  // as four labelled controls with one or two of them active rather than as
  // four colours competing. Empty on the pads page, which is these four
  // controls already.
  for (int i = 0; i < numTransportKeys; ++i)
    {
      auto const bounds = _layout.transportButtons[static_cast<size_t> (i)];
      if (bounds.isEmpty ())
        continue;

      auto const key = transportKeyOrder[i];
      auto const mark = transportColour (key);
      auto const lit = (key == TransportKey::Record && _transportRecording)
                       || (key == TransportKey::PlayPause && _transportPlaying);

      g.setColour (lit ? mark.withAlpha (0.35f)
                       : toColour (theme ().textPrimary, 0.06f));
      g.fillRoundedRectangle (bounds.toFloat (), 3.f);
      g.setColour (toColour (theme ().textPrimary, 0.15f));
      g.drawRoundedRectangle (bounds.toFloat (), 3.f, 1.f);

      // Shapes, not words -- see drawTransportGlyph(), which the pads page
      // draws from as well so the same action is the same mark in both places.
      g.setColour (mark);
      drawTransportGlyph (g,
                          bounds.toFloat ().reduced (bounds.getWidth () * 0.28f),
                          key, _transportPlaying);
    }

  // The faces stand in a frame of their own: the five keys beside them choose
  // what the settings area shows, these four choose which clip it is showing,
  // and nine keys in an unbroken row would read as one kind of thing.
  paintSetOffFrame (g, _layout.channelFacesFrame);
  paintChannelFaces (g);

  paintTab (_layout.tabClip, "CLIP", _page == BarPage::Clip);
  paintTab (_layout.tabRecord, "REC", _page == BarPage::Record);
  paintTab (_layout.tabAction, "ACTION", _page == BarPage::Action);
  paintTab (_layout.tabController, "PADS", _page == BarPage::Controller);

  // A word like the three beside it. It was a folder mark, on the reasoning
  // that the tabs are views of the clip and this one leaves it -- but once
  // every key in the row became one size, a drawing among words was the odd
  // one out rather than the distinct one, and at this size it read as a
  // smudge.
  paintTab (_layout.tabBrowser, "FILES", _page == BarPage::Browser);

}

juce::Rectangle<int>
ClipSettingsComponent::clipContentBounds () const
{
  return _layout.clipContent;
}

juce::Rectangle<int>
ClipSettingsComponent::globalGridRowsBounds () const
{
  // The row captions rather than the cells: the cells give a pixel back on
  // each side, so a page lining up with them would sit one pixel out.
  auto bounds = _layout.channelRowLabels.front ();
  for (auto const &label : _layout.channelRowLabels)
    bounds = bounds.getUnion (label);

  return bounds;
}

void
ClipSettingsComponent::setPage (BarPage page)
{
  if (_page == page)
    return;

  _page = page;

  // The clip's own controls stop taking touches: they are not drawn on the
  // controller page, and a hit area with nothing under it is how a finger
  // changes a value it cannot see.
  // The record page is the clip page with one section turned over, so every
  // control stays reachable on it; the pads page and the browser take them
  // away, because neither draws them.
  auto const showsClip
      = !isFullPage (_page);

  for (int section = 0; section < numParameters; ++section)
    for (auto &control : _controlTouch[static_cast<size_t> (section)])
      control->setVisible (showsClip);

  for (int section = 0; section < numClipSections; ++section)
    _sectionTouch[static_cast<size_t> (section)]->setVisible (showsClip);

  _elevationGraphicTouch->setVisible (showsClip);

  // The two faces' button rows sit in the same room, so only one may take
  // touches: hit areas left behind by the hidden face would answer for
  // buttons nobody can see.
  for (auto &button : _lengthTouch)
    if (button)
      button->setVisible (_page == BarPage::Record);
  for (auto &button : _speedTouch)
    if (button)
      button->setVisible (_page == BarPage::Clip);

  repaint ();
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
ClipSettingsComponent::setSlotDrifted (index_t slot, bool drifted)
{
  if (slot >= numPadSlots || drifted == _slotDrifted[slot])
    return;

  _slotDrifted[slot] = drifted;
  repaint (_layout.slotButtons[slot]);
}

void
ClipSettingsComponent::setMenuOpen (bool open)
{
  if (open == _menuOpen)
    return;

  _menuOpen = open;
  repaint (_layout.menuButton);
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
                                theme ().potSize, _page);
}

void
ClipSettingsComponent::paintSectionCard (juce::Graphics &g, int sectionIndex,
                                         bool isSelected)
{
  auto const card = _layout.sectionCards[static_cast<size_t> (sectionIndex)];

  g.setColour (cardColour (isSelected));
  g.fillRoundedRectangle (card.toFloat (), 8.f);
  // A hairline like every other edge in the bar. At 2px the selected card
  // read as a heavier object than the others rather than the same object
  // lit up, and it is the fill that says "selected" anyway.
  if (isSelected)
    g.drawRoundedRectangle (card.toFloat (), 8.f, 1.f);

  paintSectionLabel (
      g, _layout.sectionLabels[static_cast<size_t> (sectionIndex)],
      parameterNames[sectionIndex], isSelected);
}

void
ClipSettingsComponent::setChannelFaces (
    std::array<juce::Colour, numChannelColumns> colours,
    std::array<int, numChannelColumns> slots, int shownChannel)
{
  if (colours == _channelFaceColours && slots == _channelFaceSlots
      && shownChannel == _shownChannel)
    return;

  _channelFaceColours = colours;
  _channelFaceSlots = slots;
  _shownChannel = shownChannel;
  repaint ();
}

void
ClipSettingsComponent::paintSetOffFrame (juce::Graphics &g,
                                         juce::Rectangle<int> bounds)
{
  if (bounds.isEmpty ())
    return;

  g.setColour (toColour (theme ().textPrimary, 0.04f));
  g.fillRoundedRectangle (bounds.toFloat (), 4.f);
  g.setColour (toColour (theme ().textPrimary, 0.12f));
  g.drawRoundedRectangle (bounds.toFloat (), 4.f, 1.f);
}

void
ClipSettingsComponent::paintChannelFaces (juce::Graphics &g)
{
  for (size_t channel = 0; channel < numChannelColumns; ++channel)
    {
      auto const face = _layout.channelFaces[channel];
      if (face.isEmpty ())
        continue;

      auto const shown = static_cast<int> (channel) == _shownChannel
                         && !isFullPage (_page);
      auto const colour = _channelFaceColours[channel];

      // The face carries its channel's colour always, filled when it is the
      // one the bar describes and washed when it is not. A colour that came
      // and went would make finding a channel a matter of remembering which
      // one you were on -- exactly what the single CLIP tab made you do.
      g.setColour (shown ? colour.withAlpha (0.55f) : colour.withAlpha (0.16f));
      g.fillRoundedRectangle (face.toFloat (), 3.f);
      g.setColour (shown ? colour : colour.withAlpha (0.4f));
      g.drawRoundedRectangle (face.toFloat (), 3.f, shown ? 2.f : 1.f);

      // And the number in it is the slot. Not the channel: which channel this
      // is, is what the colour says, and it says it without being read. Which
      // slot cannot be a colour, so it is the one thing here worth a glyph --
      // and touching the face you are already on turns it over.
      auto const slotName
          = juce::String (juce::jlimit (0, static_cast<int> (numPadSlots) - 1,
                                        _channelFaceSlots[channel])
                          + 1);

      g.setFont (juce::Font (fontFor (FontRole::Header, face, slotName),
                             shown ? juce::Font::bold : juce::Font::plain));
      g.setColour (readableInk (colour, toColour (theme ().background),
                                toColour (theme ().textPrimary)));
      g.drawText (slotName, face, juce::Justification::centred);
    }
}

void
ClipSettingsComponent::paintGlobalSection (juce::Graphics &g,
                                           bool isSelected)
{
  paintSectionCard (g, globalIndex, isSelected);

  // Two blocks, each in a frame of its own: the values above, the things you
  // do below. Set off from the card rather than boxed in it -- a heavier edge
  // would make the strip read as two panels that happen to touch.
  paintSetOffFrame (g, _layout.channelGridFrame);
  paintSetOffFrame (g, _layout.transportFrame);

  // Through textCell like every other control in the bar: handed the whole
  // remaining column instead, the value floated in the middle and its caption
  // sat pinned to the bottom edge, a finger's width away from what it names.
  paintChannelGrid (g);

  // Every key's colour from the one rule (theme/FunctionKeyColours.hh), which
  // is what makes it one rule. Each of these used to carry its own copy —
  // REC's said "orange armed, red running" long after the rule had been
  // changed to say red always, and nothing was wrong anywhere: the screen
  // simply was not asking. Two displays reading one rule only works if both
  // of them read it.
  auto const look = functionKeyLook ();
  auto const colourFor = [&look] (FunctionKey key) {
    return functionKeyColour (key, look);
  };

  // Both carry a value, so both name it: two lines, like every other button
  // in the bar that stands for something rather than doing something.
  static char const *clockNames[] = { "INT", "EXT", "PIO" };
  auto const clock = juce::jlimit (0, 2, _clockMode);

  // Neither lights up. They carry a value, and the value is written on them —
  // a wash that comes and goes says the same thing a second time, in grey,
  // and reads as a button that is somehow half-pressed. REC and TAP still
  // light, because what they show is momentary and has no label of its own.
  // The mode in its own colour: how much of an old take this pass will
  // destroy, on the same scale the rest of the device uses.
  paintBarButton (g, _layout.recModeButton, recModeName (_recMode), "recmode",
                  false, false, colourFor (FunctionKey::RecMode));
  // The clock's own colour, from the same rule as the rest — which for this
  // key is Colours::clockMode, so the status bar reads it the same way: whose
  // tempo this is has one answer, in one colour, wherever it is written.
  paintBarButton (g, _layout.clockModeButton, clockNames[clock], "clock",
                  false, false, colourFor (FunctionKey::ClockMode));

  paintActionButton (g, _layout.menuButton, "MENU", _menuOpen,
                     colourFor (FunctionKey::Menu));
  paintActionButton (g, _layout.recButton, "REC", _recording,
                     colourFor (FunctionKey::Record));

  // TAP lights under a finger, and breathes with the beat — but not through
  // the same door. Routed through the button's own "active" look the beat
  // more than doubled the key's brightness, which is a blink you watch
  // instead of one you catch out of the corner of an eye. It is a wash laid
  // over the finished button instead, a fraction of the press's.
  paintActionButton (g, _layout.tapButton, "TAP", _tapLit,
                     _tapLit ? colourFor (FunctionKey::Tap) : juce::Colour{});
  if (_tapBeat && !_tapLit)
    {
      g.setColour (toColour (theme ().textPrimary, beatWash));
      g.fillRoundedRectangle (_layout.tapButton.toFloat (), 4.f);
    }

  // Lit in the accent while it is down. A modifier you cannot see at a glance
  // is a modifier you will get wrong, and this one decides what the next pad
  // press means.
  paintActionButton (g, _layout.shiftButton, "SHIFT", _shiftHeld,
                     colourFor (FunctionKey::Shift));
}

void
ClipSettingsComponent::paintActionButton (juce::Graphics &g,
                                          juce::Rectangle<int> bounds,
                                          juce::String const &label,
                                          bool isActive, juce::Colour tint)
{
  // Not "selected": these belong to no channel, so they must not carry the
  // shown clip's colour — the same reason the global panel's frame is grey.
  if (tint.isTransparent ())
    {
      paintBarButton (g, bounds, label, {}, isActive, false);
      return;
    }

  // A tinted key says what it is with its *word*, and keeps the bar's own grey
  // face until something is actually happening. REC is red lettering on grey
  // while it waits and a red face while it runs, so the colour says what the
  // key is and the ground says what it is doing — two questions, two places,
  // rather than one colour asked to answer both.
  g.setColour (isActive ? tint.withAlpha (highlightWash * 2.f)
                        : toColour (theme ().textPrimary, cardWash));
  g.fillRoundedRectangle (bounds.toFloat (), 4.f);
  g.setColour (tint.withAlpha (trackWash));
  g.drawRoundedRectangle (bounds.toFloat (), 4.f, 1.f);

  g.setFont (juce::Font (fontFor (FontRole::Body, bounds, label),
                         juce::Font::plain));
  g.setColour (tint);
  g.drawFittedText (label, bounds, juce::Justification::centred, 1);
}

/** The one button face the bar uses — the global section's four, Elevation's
 *  flat and pole, and Motion's two lists. Quiet, like everything else here:
 *  a wash and a thin edge, not a filled slab. Only an active one carries
 *  colour, and that is the state talking, not the button. */
void
ClipSettingsComponent::paintBarButton (juce::Graphics &g,
                                       juce::Rectangle<int> bounds,
                                       juce::String const &label,
                                       juce::String const &caption,
                                       bool isActive, bool isSelected,
                                       juce::Colour valueColour)
{
  // An active button lights in the shown clip's colour, except in the global
  // section — nothing there belongs to a channel, so it lights grey.
  g.setColour (isActive ? (isSelected
                               ? _channelColour.withAlpha (highlightWash * 2.f)
                               : toColour (theme ().textPrimary,
                                           highlightWash * 2.f))
                        : toColour (theme ().textPrimary, cardWash));
  g.fillRoundedRectangle (bounds.toFloat (), 4.f);

  g.setColour (toColour (theme ().textPrimary, trackWash));
  g.drawRoundedRectangle (bounds.toFloat (), 4.f, 1.f);

  // Two lines, both inside the box: the caption on top, the value under it.
  // The caption used to sit below the button, which made a button a
  // different height from the box it looked like and left the name floating
  // between two of them.
  auto box = bounds.reduced (4, 2);
  auto const captionArea
      = caption.isEmpty ()
            ? juce::Rectangle<int>{}
            : box.removeFromTop (box.getHeight () * 2 / 5);

  if (caption.isNotEmpty ())
    {
      g.setFont (juce::Font (
          juce::jmin (_layout.metrics.captionSize,
                      static_cast<float> (captionArea.getHeight ()) * 0.95f),
          juce::Font::plain));
      g.setColour (captionColour (isSelected));
      g.drawFittedText (caption, captionArea, juce::Justification::centred, 1);
    }

  auto const valueSize
      = juce::jmin (_layout.metrics.valueSize,
                    static_cast<float> (box.getHeight ()) * 0.9f);
  g.setFont (juce::Font (valueSize, juce::Font::plain));
  // A value that has a colour of its own — the clock's mode — writes itself
  // in it. Everything else takes the bar's.
  g.setColour (valueColour.isTransparent () ? controlColour (isSelected)
                                            : valueColour);

  g.drawFittedText (label, box, juce::Justification::centred, 1);
}

void
ClipSettingsComponent::flashTap ()
{
  _tapLit = true;
  repaint ();

  // Long enough to register as a press having landed, short enough not to
  // linger into the next one.
  startTimer (110);
}

void
ClipSettingsComponent::setShiftHeld (bool held)
{
  if (_shiftHeld == held)
    return;

  _shiftHeld = held;
  repaint (_layout.shiftButton);
}

FunctionKeyLook
ClipSettingsComponent::functionKeyLook () const
{
  FunctionKeyLook look;
  look.clockMode = _clockMode;
  look.recording = _recording;
  look.shiftHeld = _shiftHeld;
  look.menuOpen = _menuOpen;
  look.tapPressed = _tapLit;
  look.tapBeat = _tapBeat;
  look.recMode = static_cast<int> (_recMode);

  return look;
}

void
ClipSettingsComponent::pulseTapOnBeat ()
{
  // A press owns the key and its timer while it lasts. Without this a beat
  // landing under the finger restarted the timer at 70ms and cut the press's
  // 110ms flash short — the one feedback that says the tap was taken.
  if (_tapLit)
    return;

  _tapBeat = true;
  repaint (_layout.tapButton);

  // Shorter than the touch flash and never in place of it: a finger on the
  // key must still read as a press even if a beat lands under it.
  startTimer (70);
}

void
ClipSettingsComponent::timerCallback ()
{
  stopTimer ();
  _tapLit = false;
  _tapBeat = false;
  repaint ();
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
ClipSettingsComponent::setClockMode (int mode)
{
  if (mode == _clockMode)
    return;
  _clockMode = mode;
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

  // Scaled by the skin's clipSettingsHeightScale: what the contents ask for
  // is a floor for legibility, not a law, and how much of the screen the bar
  // may take from the sphere is a matter of taste.
  auto const wanted
      = clipSettingsPreferredHeight (theme ().fontSize (FontRole::Header),
                                     theme ().fontSize (FontRole::Body),
                                     knobDiam);

  // Both pages share this one area, so it has to satisfy the hungrier of
  // them: on the controller page a pad that is under a fingertip is a fault,
  // and it cannot be fixed by switching tabs.
  auto const needed = juce::jmax (
      wanted, controllerPreferredHeight (theme ().fontSize (FontRole::Header),
                                         fingertipSize));

  return juce::jmax (
      1, juce::roundToInt (static_cast<float> (needed)
                           * juce::jlimit (0.5f, 2.f,
                                           theme ().clipSettingsHeightScale)));
}



void
ClipSettingsComponent::paintTrajectorySection (juce::Graphics &g,
                                               bool isSelected)
{
  paintSectionCard (g, trajectoryIndex, isSelected);

  auto const &metrics = _layout.metrics;
  auto const recording = _page == BarPage::Record;

  if (recording)
    {
      // The take's length, on the face that is about the take. Not what is in
      // the slot — that is the picture above, which on this side is the take
      // appearing as you play it in.
      for (int i = 0; i < numRecordLengths; ++i)
        paintBarButton (g, _layout.lengthButtons[static_cast<size_t> (i)],
                        recordLengthNames[i], {},
                        _recordLengthLabel == recordLengthNames[i], isSelected);

    }
  else
    {
      // Twelve speeds, the whole range, each its own button: a set that could
      // not say every value would leave some of them unreachable.
      for (int i = 0; i < numSpeedButtons; ++i)
        paintBarButton (g, _layout.speedButtons[static_cast<size_t> (i)],
                        speedButtonNames[i], {},
                        _speedLog2 == speedButtonLog2[i], isSelected);

      // rot, fade and bias left for Motion. What is left of this section is
      // the picture and the buttons under it.
    }

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
  // Turned by what the hand set, not by what the spin is doing: the picture is
  // of this clip, and the spin's movement belongs on the sphere and on the
  // rotate knob's arc, not on a second thing to watch.
  drawTrajectoryIcon (g, iconArea, _trajectoryIcon, _channelColour,
                      _shapeRotate);

  // The name, now on its own to the left of the knob rather than lying across
  // the picture. It is the field you tap to choose a trajectory, so it reads
  // as a field: the picture above is what the choice looks like.
  //
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
ClipSettingsComponent::paintMotionSection (juce::Graphics &g,
                                           bool isSelected)
{
  paintSectionCard (g, motionIndex, isSelected);

  auto const &metrics = _layout.metrics;
  auto const &cells = _layout.controls[motionIndex];

  // A painter that outlives a renumbering reads past the end of the vector,
  // and what that looks like on screen is white rectangles flickering in and
  // out -- not a crash, and nothing that says where it came from. Checked
  // rather than trusted: the sections have been renumbered three times.
  if (cells.size ()
      < static_cast<size_t> (numControlsInSection (motionIndex)))
    return;

  // rot above, the two squeezes under it, the fade with the bias that says
  // where a drawn-through gap leads below them, and the two lists along the
  // floor. rot and the squeezes are one group -- what is done to the recorded
  // figure in its own plane -- and the spin that drives rot has gone to
  // ACTION, beside the other two slow sweeps.
  //
  // rot is a closed ring: rotation comes round to itself, so its scale has to
  // as well. The pointer is where the hand left it; the blue runs from there
  // to where the spin is holding the shape right now -- the position it is
  // being driven to, not how hard it is being driven. The spin's *control*
  // moved; what it does to this knob did not.
  paintMiniKnob (g, cells[0], metrics, caption::rotate, _shapeRotate * 2.f,
                 false, _motionSubIndex == 0, isSelected,
                 _shapeRotateReach * 2.f, true);

  // How far a gap may be for the fade to draw through it, and where a
  // drawn-through gap leads. Both read the take's holes rather than changing
  // them.
  paintMiniKnob (g, cells[1], metrics, caption::fade,
                 _motionFadeReach * 2.f - 1.f, false, _motionSubIndex == 1,
                 isSelected);
  paintMiniKnob (g, cells[2], metrics, caption::bias,
                 static_cast<float> (_motionBridgeBias) / 4.f, true,
                 _motionSubIndex == 2, isSelected);

  // The two squeezes, between rot and the fade: bipolar, so the ring runs
  // from twelve o'clock either way and the middle of the travel is the take
  // as it was recorded. The pot's own value is drawn, not the factor it comes
  // to -- half and double are the same distance from the middle by feel, and a
  // ring drawn on the factor would put unity a third of the way round.
  paintMiniKnob (g, cells[5], metrics, caption::squeezeX, _motionSqueezeX,
                 true, _motionSubIndex == 5, isSelected);
  paintMiniKnob (g, cells[6], metrics, caption::squeezeY, _motionSqueezeY,
                 true, _motionSubIndex == 6, isSelected);

  // They step on a tap -- no chevron, because nothing opens any more.
  paintBarButton (g, cells[3], value::directionNames[_motionDirection],
                  caption::direction, _motionSubIndex == 3 && isSelected,
                  false);
  paintBarButton (g, cells[4], value::endActionNames[_motionEndAction],
                  caption::endAction, _motionSubIndex == 4 && isSelected,
                  false);
}

void
ClipSettingsComponent::paintElevationSection (juce::Graphics &g,
                                              bool isSelected)
{
  paintSectionCard (g, elevationIndex, isSelected);

  auto const &metrics = _layout.metrics;
  auto const &cells = _layout.controls[elevationIndex];

  // See paintMotionSection(): a painter left behind by a renumbering reads
  // past the end and draws garbage rather than failing.
  if (cells.size ()
      < static_cast<size_t> (numControlsInSection (elevationIndex)))
    return;

  // The graphic on top, which is a control now: a finger on it sets where the
  // middle of the trajectory sits, and the line it draws is that value. Under
  // it the two clips, then reach on its own -- the swell that sweeps it is on
  // the ACTION page now, with the other two slow sweeps.
  paintElevationGraphic (g, _layout.elevationGraphic, isSelected);

  paintMiniKnob (g, cells[0], metrics, caption::clipTop,
                 _elevationClipTop * 2.f - 1.f, false, _elevationSubIndex == 0,
                 isSelected);
  paintMiniKnob (g, cells[1], metrics, caption::clipBottom,
                 _elevationClipBottom * 2.f - 1.f, false,
                 _elevationSubIndex == 1, isSelected);

  // Where the sweep has carried the coverage, if it is moving.
  paintMiniKnob (g, cells[2], metrics, caption::reach,
                 _elevationReach * 2.f - 1.f, false, _elevationSubIndex == 2,
                 isSelected,
                 _elevationReachSwept < 0.f
                     ? -2.f
                     : _elevationReachSwept * 2.f - 1.f);
}

void
ClipSettingsComponent::paintElevationGraphic (juce::Graphics &g,
                                              juce::Rectangle<int> bounds,
                                              bool isSelected)
{
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

  // One value is drawn in here and one only: where the middle of the
  // trajectory sits. The band the clips leave is a fixed frame around it --
  // it does not move, the line does.
  //
  // reach used to be drawn too, as a second chord with the sweep between
  // them shaded in. That read as "elevation is a band that moves with the
  // clips", which is not what any of it does: reach is how far the
  // trajectory spreads from the line, and it has its own knob to say so.
  auto const baseFrac = std::clamp (std::clamp (_elevationBase, 0.f, 1.f),
                                    bandLow, bandHigh);

  auto const fracToY
      = [&] (float frac) { return (centre.y - r) + frac * (r * 2.f); };

  juce::Path circlePath;
  circlePath.addEllipse (centre.x - r, centre.y - r, r * 2.f, r * 2.f);

  g.saveState ();
  g.reduceClipRegion (circlePath);

  // The band the clips leave is the sphere the sound can still use, so it is
  // the lit part; everything they take is cut away to the bar's own surface.
  // The eye should find the reachable band without reading a number, and a
  // wash close to the ground did not say which was which. Zero-height rects
  // where nothing is clipped, so no guard is needed.
  g.setColour (toColour (theme ().textPrimary, 0.05f));
  g.fillRect (juce::Rectangle<float> (
      centre.x - r, fracToY (bandLow), r * 2.f,
      juce::jmax (0.f, fracToY (bandHigh) - fracToY (bandLow))));

  g.setColour (toColour (theme ().surface, clippedZoneOpacity));
  g.fillRect (juce::Rectangle<float> (centre.x - r, centre.y - r, r * 2.f,
                                      fracToY (bandLow) - (centre.y - r)));
  g.fillRect (juce::Rectangle<float> (
      centre.x - r, fracToY (bandHigh), r * 2.f,
      (centre.y + r) - fracToY (bandHigh)));

  g.restoreState ();

  // The two cuts as edges, not only as a change of shade: a boundary you can
  // see is a boundary you can aim a finger at.
  auto const drawCut = [&] (float frac) {
    auto const y = fracToY (frac);
    auto const dy = y - centre.y;
    if (std::abs (dy) > r)
      return;

    auto const halfWidth = std::sqrt (r * r - dy * dy);
    g.setColour (toColour (theme ().textPrimary, 0.25f));
    g.drawLine (centre.x - halfWidth, y, centre.x + halfWidth, y, 1.f);
  };

  drawCut (bandLow);
  drawCut (bandHigh);

  // ── The instrument ────────────────────────────────────────────────────
  //
  // The circle reads like a horizon seen from the middle of the room, so it
  // is given the marks that make one readable. They are not decoration: what
  // a performer wants to know at a glance is whether the sound is above or
  // below the ears, and by roughly how much.

  // Ear height. The equator is where a sound is level with the listener, and
  // it is the one line worth having whatever else is set -- dashed, so it
  // never competes with the axis a finger put somewhere.
  {
    auto const y = fracToY (0.5f);
    g.setColour (toColour (theme ().textPrimary, 0.22f));

    auto const dash = juce::jmax (2.f, r / 12.f);
    for (auto x = centre.x - r; x < centre.x + r; x += dash * 2.f)
      g.drawLine (x, y, juce::jmin (x + dash, centre.x + r), y, 1.f);
  }

  // Elevation marks every thirty degrees up the left edge, longer at the
  // poles and the equator. No numbers: the circle is small, and the pattern
  // of ticks says how far up or down something is without any reading.
  {
    g.setColour (toColour (theme ().textPrimary, 0.35f));

    for (int degrees = 0; degrees <= 180; degrees += 30)
      {
        auto const frac = static_cast<float> (degrees) / 180.f;
        auto const y = fracToY (frac);
        auto const dy = y - centre.y;
        if (std::abs (dy) > r)
          continue;

        auto const halfWidth = std::sqrt (r * r - dy * dy);
        auto const major = degrees % 90 == 0;
        auto const len = r * (major ? 0.22f : 0.12f);

        g.drawLine (centre.x - halfWidth, y, centre.x - halfWidth + len, y,
                    major ? 1.5f : 1.f);
      }
  }

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

  // The base: where the middle of the trajectory sits, and the one line in
  // here a finger sets. Drawn boldest and in the channel's colour because it
  // is the control -- the reach chord is a reading of what follows from it.
  drawMarkerChord (baseFrac, 2.5f, 4.f, iconColour);

  // With the end caps a horizon bar has. They turn a chord into something
  // aimed at rather than merely drawn, which is the whole difference between
  // a picture and an instrument.
  {
    auto const y = fracToY (baseFrac);
    auto const dy = y - centre.y;
    if (std::abs (dy) <= r)
      {
        auto const halfWidth = std::sqrt (r * r - dy * dy);
        auto const cap = juce::jmax (2.f, r * 0.12f);

        g.setColour (iconColour);
        g.drawLine (centre.x - halfWidth, y, centre.x - halfWidth, y + cap,
                    2.f);
        g.drawLine (centre.x + halfWidth, y, centre.x + halfWidth, y + cap,
                    2.f);
      }
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
                                      bool isActive, bool isSelected,
                                      float reachFrac, bool wraps)
{
  // The drawing lives in BarKnob so the ACTION page can use the same one. Two
  // knobs that are nearly the same knob is how a bar stops looking like one
  // instrument.
  paintBarKnob (g, bounds, metrics, _channelColour, label, angleFrac,
                fillFromZero, isActive, isSelected, reachFrac, wraps);
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
ClipSettingsComponent::paintChannelGrid (juce::Graphics &g)
{
  auto const &metrics = _layout.metrics;

  static char const *rowCaptions[numChannelRows] = { "3d", "freq", "Q" };

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

      // No number over the column: the channel's own colour says which is
      // whose, and it says it without being read. The row it took is a row
      // the twelve knobs wanted.

      // In channelRow* order — 3d on top, then freq, then Q. All three have
      // something carrying them past where they were set: the accent, and the
      // cutoff's and resonance's own envelopes.
      float const values[numChannelRows]
          = { _channelThreeD[c], _channelFreq[c], _channelQ[c] };
      float const reaches[numChannelRows]
          = { _channelThreeDReach[c], _channelFreqReach[c],
              _channelQReach[c] };

      for (int row = 0; row < numChannelRows; ++row)
        paintGridKnob (g, _layout.channelGrid[c][static_cast<size_t> (row)],
                       metrics, values[row], reaches[row], colour);
    }
}

/** A knob without a caption: the column says which channel, the row caption
 *  down the side says which value, so the knob itself has nothing to add. */
void
ClipSettingsComponent::paintGridKnob (juce::Graphics &g,
                                      juce::Rectangle<int> bounds,
                                      ControlMetrics metrics, float value,
                                      float reach, juce::Colour colour)
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

  auto const thickness = juce::jmax (1.5f, r * 0.18f);

  juce::Path valueArc;
  valueArc.addCentredArc (centre.x, centre.y, r, r, 0.f, -sweep, angle, true);
  g.setColour (colour);
  g.strokePath (valueArc, juce::PathStrokeType (thickness));

  // What a modulation is doing right now: the stretch from the pointer to
  // where the value has actually been carried. It grows out of the pointer
  // and shrinks back into it, so the knob shows the floor and the movement at
  // once — the pointer stays where the hand put it while the arc moves.
  auto const reachAngle
      = (std::clamp (reach, 0.f, 1.f) * 2.f - 1.f) * sweep;
  if (reachAngle > angle)
    {
      juce::Path reachArc;
      reachArc.addCentredArc (centre.x, centre.y, r, r, 0.f, angle, reachAngle,
                              true);
      g.setColour (toColour (theme ().notice));
      g.strokePath (reachArc, juce::PathStrokeType (thickness));
    }

  auto const tip = centre.getPointOnCircumference (r, angle);
  g.drawLine (centre.x, centre.y, tip.x, tip.y, juce::jmax (1.5f, r * 0.14f));
}

}
