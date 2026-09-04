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

#include <a3-motion-engine/Envelope.hh>
#include <a3-motion-engine/TempoLfo.hh>

#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-engine/TrajectorySpin.hh>

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
        // Touching anywhere else puts an open list away — the same way
        // tapping beside a menu closes it.
        closeDropdown ();

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
  // Held, not tapped — the accent lasts as long as the finger does, so this
  // needs onRelease rather than onTap. Same as the pad it stands for.
  _accentTouch = std::make_unique<TouchControl> ();
  _accentTouch->onPress = [this] (int, int) {
    _accentHeld = true;
    if (onAccentHeld)
      onAccentHeld (true);
    repaint (_layout.accentButton);
  };
  _accentTouch->onRelease = [this] (int, int) {
    _accentHeld = false;
    if (onAccentHeld)
      onAccentHeld (false);
    repaint (_layout.accentButton);
  };
  addAndMakeVisible (*_accentTouch);

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
  makeTab (_tabControllerTouch, BarPage::Controller);

  // In front of the cards, so it swallows what would otherwise reach the
  // Elevation card. No callbacks: a picture is not a control.
  _elevationGraphicTouch = std::make_unique<TouchControl> ();
  // The graphic is a picture of what the controls under it do, so it names
  // no control — but it is inside the section, and touching a section
  // should select it. -1 says "the section, not one of its controls".
  _elevationGraphicTouch->onPress = [this] (int, int) {
    closeDropdown ();
    if (onControlTapped)
      onControlTapped (elevationIndex, -1);
  };
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

  // As many entries as the longest list has. Which list they belong to and
  // where they sit is set when one opens; they are hidden otherwise.
  // The record length is the longest list: 2^-7 .. 2^4 bars.
  for (int i = 0; i < 12; ++i)
    {
      auto entry = std::make_unique<TouchControl> ();
      entry->setIdentity (i);
      entry->onTap = [this] (int index, int) {
        if (_openDropdown < 0)
          return;

        auto const section = _openDropdownSection;
        auto const sub = _openDropdown;
        auto const delta = index - dropdownCurrentIndex (section, sub);
        closeDropdown ();

        // Motion's two wrap modulo their value count and the record length
        // clamps, so a difference lands exactly on the entry tapped either
        // way — even a negative one.
        if (delta != 0 && onControlDragged)
          onControlDragged (section, sub, delta);
      };
      entry->setVisible (false);
      addChildComponent (*entry);
      _dropdownTouch.push_back (std::move (entry));
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
            // A list control opens its list rather than stepping blindly
            // through it.
            if (opensList (tappedSection, tappedSub))
              {
                if (_openDropdownSection == tappedSection
                    && _openDropdown == tappedSub)
                  closeDropdown ();
                else
                  openDropdown (tappedSection, tappedSub);
                return;
              }

            // A control with few states changes right away: tapping your
            // way to a yes/no and then having to drag it as well would be
            // one move too many. Continuous values are dragged, not tapped.
            if (tapTogglesValue (tappedSection, tappedSub) && onControlToggled)
              onControlToggled (tappedSection, tappedSub);
            else if (tapAdvancesValue (tappedSection, tappedSub)
                     && onControlDragged)
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

  _shiftTouch->setBounds (_layout.shiftButton);
  _accentTouch->setBounds (_layout.accentButton);

  for (int i = 0; i < numTransportKeys; ++i)
    _transportTouch[static_cast<size_t> (i)]->setBounds (
        _layout.transportButtons[static_cast<size_t> (i)]);

  _tabClipTouch->setBounds (_layout.tabClip);
  _tabRecordTouch->setBounds (_layout.tabRecord);
  _tabControllerTouch->setBounds (_layout.tabController);

  for (int col = 0; col < numChannelColumns; ++col)
    for (int row = 0; row < numChannelRows; ++row)
      {
        auto const c = static_cast<size_t> (col);
        auto const r = static_cast<size_t> (row);
        _gridTouch[c][r]->setBounds (_layout.channelGrid[c][r]);
      }
  layOutDropdown ();
  for (size_t i = 0; i < _dropdownTouch.size (); ++i)
    {
      auto const shown = i < _dropdownEntries.size ();
      _dropdownTouch[i]->setVisible (shown);
      if (shown)
        {
          _dropdownTouch[i]->toFront (false);
          _dropdownTouch[i]->setBounds (_dropdownEntries[i]);
        }
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
ClipSettingsComponent::setMotionSpin (int step)
{
  _motionSpin = juce::jlimit (-lfoMaxStep, lfoMaxStep, step);
  repaint ();
}

void
ClipSettingsComponent::setMotionSwell (int step)
{
  _motionSwell = juce::jlimit (-lfoMaxStep, lfoMaxStep, step);
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
ClipSettingsComponent::setChannelValues (int channel, float freq, float q,
                                         float threeD, float threeDEffective)
{
  if (channel < 0 || channel >= numChannelColumns)
    return;

  auto const c = static_cast<size_t> (channel);
  auto const set = std::clamp (threeD, 0.f, 1.f);
  auto const reach = std::clamp (threeDEffective, set, 1.f);

  if (juce::approximatelyEqual (_channelFreq[c], std::clamp (freq, 0.f, 1.f))
      && juce::approximatelyEqual (_channelQ[c], std::clamp (q, 0.f, 1.f))
      && juce::approximatelyEqual (_channelThreeD[c], set)
      && juce::approximatelyEqual (_channelThreeDReach[c], reach))
    return; // nothing moved; this runs on every LED tick

  _channelFreq[c] = std::clamp (freq, 0.f, 1.f);
  _channelQ[c] = std::clamp (q, 0.f, 1.f);
  _channelThreeD[c] = set;
  _channelThreeDReach[c] = reach;
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

  // Not on the controller page, which shows every slot at once: naming one of
  // them there says something untrue about what you are looking at. Both of
  // the clip's faces describe a single slot, and the record face -- the take
  // about to be written -- is the one where being sure which slot it is
  // matters most.
  if (_page != BarPage::Controller)
    {
      auto const slotName = "Slot " + juce::String (_slot + 1);
      g.setFont (juce::Font (
          fontFor (FontRole::Header, _layout.slotLabel, slotName),
          juce::Font::bold));
      g.setColour (_channelColour);
      g.drawText (slotName, _layout.slotLabel,
                  juce::Justification::centredLeft, true);
    }

  paintTabs (g);

  // Over the global strip, on the header row's line: what it reports comes
  // from either page, so it belongs beside the part of the bar that stands on
  // both rather than inside the half that gets swapped out.
  g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName (),
                         fontFor (FontRole::Header, _layout.readout,
                                  _lastControlText),
                         juce::Font::plain));
  g.setColour (toColour (theme ().accent, panelOpacity));
  g.drawText (_lastControlText, _layout.readout,
              juce::Justification::centredRight, true);

  // The global strip stands on both pages: recmode, clock, MENU, REC and TAP
  // belong to the device rather than to the clip, and losing them while you
  // are firing clips is exactly the wrong moment to lose them.
  paintGlobalSection (g, _selectedIndex == globalIndex);

  if (_page == BarPage::Controller)
    return; // ControllerComponent draws the rest

  paintTrajectorySection (g, _selectedIndex == trajectoryIndex);
  paintElevationSection (g, _selectedIndex == elevationIndex);
  paintMotionSection (g, _selectedIndex == motionIndex);

  // Last, so it covers whichever section it belongs to.
  paintDropdown (g);
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

  // The four transport keys. Coloured only while they are doing something,
  // the same rule the global strip's keys follow -- a colour that means
  // nothing makes the ones that mean something harder to read. Record is the
  // exception it is there too: it says what it is in its lettering whether or
  // not it is running.
  for (int i = 0; i < numTransportKeys; ++i)
    {
      auto const bounds = _layout.transportButtons[static_cast<size_t> (i)];
      if (bounds.isEmpty ())
        continue;

      auto const key = transportKeyOrder[i];
      juce::Colour ground{};
      auto text = toColour (theme ().textPrimary, 0.75f);
      juce::String label;

      switch (key)
        {
        case TransportKey::Record:
          label = "REC";
          text = toColour (theme ().danger);
          if (_transportRecording)
            ground = toColour (theme ().danger);
          break;
        case TransportKey::Stop:
          label = "STOP";
          break;
        case TransportKey::PlayPause:
          // One key, two words, because it is one key on the pad too. What it
          // says is what pressing it will do.
          label = _transportPlaying ? "II" : ">";
          if (_transportPlaying)
            ground = toColour (theme ().accent);
          break;
        case TransportKey::Action:
          label = "ACT";
          break;
        }

      g.setColour (ground.isTransparent ()
                       ? toColour (theme ().textPrimary, 0.06f)
                       : ground.withAlpha (0.35f));
      g.fillRoundedRectangle (bounds.toFloat (), 3.f);
      g.setColour (toColour (theme ().textPrimary, 0.15f));
      g.drawRoundedRectangle (bounds.toFloat (), 3.f, 1.f);

      g.setFont (juce::Font (fontFor (FontRole::Body, bounds, label),
                             juce::Font::plain));
      g.setColour (text);
      g.drawFittedText (label, bounds, juce::Justification::centred, 1);
    }

  paintTab (_layout.tabClip, "CLIP", _page == BarPage::Clip);
  paintTab (_layout.tabRecord, "REC", _page == BarPage::Record);
  paintTab (_layout.tabController, "PADS", _page == BarPage::Controller);
}

juce::Rectangle<int>
ClipSettingsComponent::clipContentBounds () const
{
  return _layout.clipContent;
}

void
ClipSettingsComponent::setPage (BarPage page)
{
  if (_page == page)
    return;

  _page = page;
  closeDropdown ();

  // The clip's own controls stop taking touches: they are not drawn on the
  // controller page, and a hit area with nothing under it is how a finger
  // changes a value it cannot see.
  // The record page is the clip page with one section turned over, so every
  // control stays reachable on it; only the pads page takes them away.
  auto const showsClip = _page != BarPage::Controller;

  for (int section = 0; section < numParameters; ++section)
    for (auto &control : _controlTouch[static_cast<size_t> (section)])
      control->setVisible (showsClip);

  for (int section = 0; section < numClipSections; ++section)
    _sectionTouch[static_cast<size_t> (section)]->setVisible (showsClip);

  _elevationGraphicTouch->setVisible (showsClip);
  _accentTouch->setVisible (showsClip);

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
ClipSettingsComponent::paintGlobalSection (juce::Graphics &g,
                                           bool isSelected)
{
  paintSectionCard (g, globalIndex, isSelected);

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
                  false, false, false, colourFor (FunctionKey::RecMode));
  // The clock's own colour, from the same rule as the rest — which for this
  // key is Colours::clockMode, so the status bar reads it the same way: whose
  // tempo this is has one answer, in one colour, wherever it is written.
  paintBarButton (g, _layout.clockModeButton, clockNames[clock], "clock",
                  false, false, false,
                  colourFor (FunctionKey::ClockMode));

  paintActionButton (g, _layout.menuButton, "MENU", false,
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
      paintBarButton (g, bounds, label, {}, isActive, false, false);
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
                                       bool opensList,
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

  if (!opensList)
    {
      g.drawFittedText (label, box, juce::Justification::centred, 1);
      return;
    }

  // The chevron sits directly beside the value rather than out at the box's
  // edge, where it read as belonging to the button next to it.
  auto const font = g.getCurrentFont ();
  auto const textW
      = juce::jmin (static_cast<float> (box.getWidth ()) * 0.7f,
                    juce::GlyphArrangement::getStringWidth (font, label));
  auto const gap = juce::jmax (3.f, valueSize * 0.35f);
  auto const markerW = juce::jmax (5.f, valueSize * 0.4f);

  auto const centre = box.toFloat ().getCentre ();
  auto const textCentre = centre.x - (gap + markerW) * 0.5f;

  g.drawFittedText (
      label,
      juce::Rectangle<int> (juce::roundToInt (textCentre - textW * 0.5f),
                            box.getY (), juce::roundToInt (textW),
                            box.getHeight ()),
      juce::Justification::centred, 1);

  auto const mx = textCentre + textW * 0.5f + gap + markerW * 0.5f;
  auto const my = centre.y;
  auto const w = markerW * 0.5f;

  juce::Path chevron;
  chevron.startNewSubPath (mx - w, my - w * 0.55f);
  chevron.lineTo (mx, my + w * 0.55f);
  chevron.lineTo (mx + w, my - w * 0.55f);

  g.setColour (captionColour (isSelected));
  g.strokePath (chevron,
                juce::PathStrokeType (juce::jmax (1.f, w * 0.35f),
                                      juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
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
  auto const &cells = _layout.controls[trajectoryIndex];
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

      // How much of the take's end is spent travelling back to where it began
      // rather than jumping. It belongs to the recording, so it belongs here
      // rather than among the movements in Motion.
      paintMiniKnob (g, cells[1], metrics, caption::fade,
                     (_motionFade / 16.f) * 2.f - 1.f, false,
                     _trajectorySubIndex == 1, isSelected);
    }
  else
    {
      // Twelve speeds, the whole range, each its own button: a set that could
      // not say every value would leave some of them unreachable.
      for (int i = 0; i < numSpeedButtons; ++i)
        paintBarButton (g, _layout.speedButtons[static_cast<size_t> (i)],
                        speedButtonNames[i], {},
                        _speedLog2 == speedButtonLog2[i], isSelected);

      // Which way the shape faces, and where the spin has carried it: the
      // pointer is the hand's value, the arc is the movement over it.
      // A closed ring: rotation comes round to itself, so its scale has to as
      // well. The pointer is where the hand left it; the blue runs from there
      // to where the spin is holding the shape right now -- the position it is
      // being driven to, not how hard it is being driven.
      // A turn, not a bipolar value: 0 is straight up and the scale runs once
      // round clockwise. (frac * 2 is a whole turn once the ring folds it back
      // into the [-1, 1] the knob speaks; frac * 2 - 1 would put no rotation
      // at six o'clock.)
      paintMiniKnob (g, cells[1], metrics, caption::rotate,
                     _shapeRotate * 2.f, false, _trajectorySubIndex == 1,
                     isSelected, _shapeRotateReach * 2.f, true);
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
  drawTrajectoryIcon (g, iconArea, _trajectoryIcon, _channelColour);

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
ClipSettingsComponent::paintElevationSection (juce::Graphics &g,
                                              bool isSelected)
{
  paintSectionCard (g, elevationIndex, isSelected);

  auto const &metrics = _layout.metrics;
  auto const &cells = _layout.controls[elevationIndex];

  // Graphic on top, then all six controls below it in a 2x3 grid:
  // reach/mirror-south on the first row, clip-top/clip-bottom on the
  // second, flat/flat-elevation on the third. The cells are ordered by
  // sub-index, not by row.
  //
  // The graphic never lights up on its own. It used to, whenever the
  // selected control was one it draws, and that read as "there is something
  // to grab in here" — there is not; it is a picture of what the controls
  // under it do. Selection is the card's job, and the card already shows it.
  paintElevationGraphic (g, _layout.elevationGraphic, isSelected);

  paintMiniKnob (g, cells[0], metrics, caption::reach,
                 _elevationReach * 2.f - 1.f, false, _elevationSubIndex == 0,
                 isSelected);
  paintMiniKnob (g, cells[1], metrics, caption::clipTop,
                 _elevationClipTop * 2.f - 1.f, false, _elevationSubIndex == 1,
                 isSelected);
  paintMiniKnob (g, cells[2], metrics, caption::clipBottom,
                 _elevationClipBottom * 2.f - 1.f, false,
                 _elevationSubIndex == 2, isSelected);
  paintBarButton (g, cells[3],
                  _elevationMirrorSouth ? value::south : value::north,
                  caption::pole, _elevationSubIndex == 3 && isSelected,
                  isSelected);
  paintBarButton (g, cells[4], _elevationFlat ? value::on : value::off,
                  caption::flat, _elevationSubIndex == 4 && isSelected,
                  isSelected);
  paintMiniKnob (g, cells[5], metrics, caption::flatElevation,
                 _elevationFlatElevation * 2.f - 1.f, false,
                 _elevationSubIndex == 5, isSelected);
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
                                      bool isActive, bool isSelected,
                                      float reachFrac, bool wraps)
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

  // The two bipolar knobs first, because they are the pair the section is
  // mostly about: standing still is the middle, and which side of it you are
  // on is which way the thing turns or breathes.
  paintMiniKnob (g, cells[0], metrics, caption::spin,
                 static_cast<float> (_motionSpin)
                     / static_cast<float> (lfoMaxStep),
                 true, _motionSubIndex == 0, isSelected);
  paintMiniKnob (g, cells[1], metrics, caption::swell,
                 static_cast<float> (_motionSwell)
                     / static_cast<float> (lfoMaxStep),
                 true, _motionSubIndex == 1, isSelected);

  // The accent's two times. One-sided, not bipolar: a length has no other
  // direction, so they sweep from the left.
  auto const envFrac = [] (int step) {
    return (static_cast<float> (step) / static_cast<float> (envelopeMaxStep))
               * 2.f
           - 1.f;
  };
  paintMiniKnob (g, cells[2], metrics, caption::attack, envFrac (_motionAttack),
                 false, _motionSubIndex == 2, isSelected);
  paintMiniKnob (g, cells[3], metrics, caption::decay, envFrac (_motionDecay),
                 false, _motionSubIndex == 3, isSelected);
  paintMiniKnob (g, cells[4], metrics, caption::envelopeMax,
                 _motionEnvelopeMax * 2.f - 1.f, false, _motionSubIndex == 4,
                 isSelected);

  // Lists, not values you nudge: a button that opens one.
  paintBarButton (g, cells[5], value::directionNames[_motionDirection],
                  caption::direction, _motionSubIndex == 5 && isSelected,
                  isSelected, true);
  paintBarButton (g, cells[6], value::endActionNames[_motionEndAction],
                  caption::endAction, _motionSubIndex == 6 && isSelected,
                  isSelected, true);

  // The key that plays what the two knobs above it shape. Lit while it is
  // down, in the notice colour the grid draws the accent's reach in — the
  // same colour saying the same thing in two places.
  paintActionButton (g, _layout.accentButton, "ACT", _accentHeld,
                     _accentHeld ? toColour (theme ().notice)
                                 : juce::Colour{});
}

bool
ClipSettingsComponent::opensList (int section, int sub)
{
  if (section == motionIndex)
    return sub == 5 || sub == 6; // direction, end-action
  return false;
}

juce::StringArray
ClipSettingsComponent::dropdownValues (int section, int sub) const
{
  if (section == motionIndex && sub == 5)
    return { value::directionNames[0], value::directionNames[1] };
  if (section == motionIndex && sub == 6)
    {
      juce::StringArray names;
      for (int i = 0; i < value::numEndActions; ++i)
        names.add (value::endActionNames[i]);
      return names;
    }

  return {};
}

int
ClipSettingsComponent::dropdownCurrentIndex (int section, int sub) const
{
  return sub == 1 ? _motionDirection : _motionEndAction;
}

void
ClipSettingsComponent::layOutDropdown ()
{
  _dropdownEntries.clear ();
  if (_openDropdown < 0)
    return;

  auto const count
      = dropdownValues (_openDropdownSection, _openDropdown).size ();
  if (count == 0)
    return;

  auto area
      = _layout.dropdownArea[static_cast<size_t> (_openDropdownSection)];
  auto const gap = juce::jmax (1, area.getHeight () / 40);
  auto const rowH = (area.getHeight () - (count - 1) * gap) / count;

  for (int i = 0; i < count; ++i)
    {
      _dropdownEntries.push_back (area.removeFromTop (rowH));
      area.removeFromTop (gap);
    }
}

void
ClipSettingsComponent::openDropdown (int section, int sub)
{
  _openDropdownSection = section;
  _openDropdown = sub;
  layOutDropdown ();
  resized ();
  repaint ();
}

void
ClipSettingsComponent::closeDropdown ()
{
  if (_openDropdown < 0)
    return;

  _openDropdown = -1;
  _openDropdownSection = -1;
  _dropdownEntries.clear ();
  resized ();
  repaint ();
}

void
ClipSettingsComponent::paintDropdown (juce::Graphics &g)
{
  if (_openDropdown < 0 || _dropdownEntries.empty ())
    return;

  // Opaque, then the card wash on top. cardColour() alone is translucent by
  // design — over the section's own controls that left the list and the
  // controls it covers drawn on top of each other.
  auto const area
      = _layout.dropdownArea[static_cast<size_t> (_openDropdownSection)];

  g.setColour (toColour (theme ().surface));
  g.fillRoundedRectangle (area.toFloat (), 6.f);
  g.setColour (cardColour (true));
  g.fillRoundedRectangle (area.toFloat (), 6.f);

  auto const values = dropdownValues (_openDropdownSection, _openDropdown);
  auto const current = dropdownCurrentIndex (_openDropdownSection, _openDropdown);

  for (int i = 0; i < values.size (); ++i)
    paintBarButton (g, _dropdownEntries[static_cast<size_t> (i)], values[i],
                    {}, i == current, true);
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

      // The channel's own colour says which column is whose; a number over
      // it would only repeat what the colour already tells the eye.
      g.setFont (juce::Font (metrics.captionSize, juce::Font::bold));
      g.setColour (colour);
      g.drawFittedText (juce::String (col + 1), _layout.channelLabels[c],
                        juce::Justification::centred, 1);

      // In channelRow* order — 3d on top, then freq, then Q. Only 3d has
      // anything carrying it past where it was set.
      float const values[numChannelRows]
          = { _channelThreeD[c], _channelFreq[c], _channelQ[c] };
      float const reaches[numChannelRows]
          = { _channelThreeDReach[c], _channelFreq[c], _channelQ[c] };

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
