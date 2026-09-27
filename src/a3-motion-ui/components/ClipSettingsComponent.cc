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

#include <a3-motion-ui/components/KnobHold.hh>
#include <algorithm>

#include <a3-motion-ui/components/BarButton.hh>
#include <a3-motion-ui/components/BarKnob.hh>

#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/Envelope.hh>
#include <a3-motion-engine/TempoLfo.hh>

#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-engine/TrajectorySpin.hh>

#include <a3-motion-ui/components/ClipSettingsLayout.hh>
#include <a3-motion-ui/components/Listener.hh>

#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/TransportLook.hh>

#include <cmath>

namespace a3
{

namespace
{
// Opacities that describe a structure rather than a state: the panel over the
// sphere, the shading of the elevation graphic, the unlit part of a knob's
// track. State — selected, inactive, disabled — comes from the theme's alphas
// instead.
constexpr float cardWash = 0.08f;
constexpr float highlightWash = 0.18f;
constexpr float trackWash = 0.18f;
/** How much the TAP key comes up on a beat. A fifth of the wash a press
 *  makes: this is the metronome you notice without looking at it, and it was
 *  taken out once already for being louder than that. */
constexpr float beatWash = 0.035f;


/** Where PlayPause sits in the key order -- looked up rather than assumed, so
 *  rearranging transportKeyOrder cannot leave the blink repainting a
 *  neighbour. */
/** How long Stop stays lit under a finger. Same as the tap flash: long enough
 *  to register as a press having landed, short enough not to linger into the
 *  next one. */
constexpr int stopFlashMillis = 110;

int
playPauseIndex ()
{
  for (int i = 0; i < numTransportKeys; ++i)
    if (transportKeyOrder[i] == TransportKey::PlayPause)
      return i;

  return 0;
}

/** And where Stop sits, for the same reason. */
int
stopIndex ()
{
  for (int i = 0; i < numTransportKeys; ++i)
    if (transportKeyOrder[i] == TransportKey::Stop)
      return i;

  return 0;
}
constexpr float clippedZoneOpacity = 0.55f;
constexpr float outlineOpacity = 0.5f;
}

ClipSettingsComponent::ClipSettingsComponent ()
{
  // Not for itself, but for its children: the bar's own surface has nothing
  // to catch, the hit areas over the controls do.
  setInterceptsMouseClicks (false, true);

  // Until setTarget names a channel there is no channel colour to show.
  _channelColour = toColour (theme ().textPrimary);

  _stopFlash.onTick = [this] {
    _stopFlash.stopTimer ();
    _stopPressed = false;
    repaint (_layout.transportButtons[static_cast<size_t> (stopIndex ())]);
  };

  createTouchControls ();
  showControlsOfPage ();
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

      // The meter is drawn in the face and takes no touch: the whole face
      // selects the clip.
      auto &meter = _faceMeter[channel];
      meter = std::make_unique<VuMeterView> ();
      meter->setDirection (VuDirection::Up);
      meter->level = [this, channel] {
        return channelLevel ? channelLevel (static_cast<int> (channel))
                            : VuLevel{};
      };
      meter->setInterceptsMouseClicks (false, false);
      addAndMakeVisible (*meter);

      // Over the face's touch, so a drag turns them; landing on one chooses
      // the face too.
      for (int i = 0; i < numChannelPots; ++i)
        {
          auto const which = channelPotOrder[static_cast<std::size_t> (i)];
          auto &pot = _facePots[channel][static_cast<std::size_t> (i)];
          pot = makeChannelPotKnob (which);
          pot->onValueChange = [this, channel, which, k = pot.get ()] {
            if (onChannelPotChanged)
              onChannelPotChanged (static_cast<int> (channel), which,
                                   static_cast<float> (k->getValue ()));
          };
          pot->onDoubleTapped = [this, channel, which] {
            if (onChannelPotDoubleTapped)
              onChannelPotDoubleTapped (static_cast<int> (channel), which);
          };
          pot->onDragStart = [this, channel] {
            if (onChannelFaceChosen)
              onChannelFaceChosen (static_cast<index_t> (channel));
          };
          addAndMakeVisible (*pot);
        }
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
            // Stop answers with a flash, because it has nothing else to answer
            // with: it acts now and unquantised, so unlike play it can never
            // blink while it waits, and unlike record it is not a state you
            // stay in. On press rather than on tap-complete -- what is being
            // acknowledged is the finger landing.
            if (key == TransportKey::Stop)
              flashStop ();

            if (onTransportTapped)
              onTransportTapped (key);
          };
        }
      addAndMakeVisible (*touch);
    }

  makeTab (_tabClipTouch, BarPage::Clip);
  makeTab (_tabActionTouch, BarPage::Action);
  makeTab (_tabControllerTouch, BarPage::Controller);
  makeTab (_tabMixerTouch, BarPage::Mixer);
  makeTab (_tabBrowserTouch, BarPage::Browser);
  makeTab (_tabRecordTouch, BarPage::Record);
  makeTab (_tabMotionTouch, BarPage::Motion);


  // The elevation picture is selected by a touch anywhere on its field, and
  // selected it hands the big sphere to the camera. A tap, not a drag: the
  // picture shows the view, the sphere is what turns it.
  _elevationPictureTouch = std::make_unique<TouchControl> ();
  _elevationPictureTouch->onTap = [this] (int, int) {
    if (onElevationPictureTapped)
      onElevationPictureTapped ();
  };
  addAndMakeVisible (*_elevationPictureTouch);

  _tabMainMixTouch = std::make_unique<TouchControl> ();
  _tabMainMixTouch->onTap = [this] (int, int) {
    if (onMainMixTapped)
      onMainMixTapped ();
  };
  addAndMakeVisible (*_tabMainMixTouch);


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
      // And a drag over one gives that key another speed. The key keeps it,
      // so a speed the four do not yet name is reached once and stays where
      // it was put — where before a drag walked the shown clip through the
      // range and left nothing behind, which read as jumping between the
      // keys because only the key matching the value ever lit.
      button->onDragIncrement = [this] (int index, int, int increment) {
        _speedDragIndex = index;
        if (onSpeedDragged)
          onSpeedDragged (index, increment);
        repaint ();
      };
      // onRelease rather than onDragEnd: it fires whenever the finger comes
      // up, where onDragEnd is silent unless the control decided a drag had
      // happened. A key left marked as dragged would go on claiming to be the
      // clip's speed until the next gesture, so the clearing has to be the
      // callback that cannot be skipped.
      button->onRelease = [this] (int, int) {
        if (_speedDragIndex == noSpeedKeyDragged)
          return;

        _speedDragIndex = noSpeedKeyDragged;
        repaint ();
      };
      addAndMakeVisible (*button);
      _speedTouch[static_cast<size_t> (i)] = std::move (button);
    }

  makeButton (_recModeTouch, &ClipSettingsComponent::onRecModePressed);

  for (int section = 0; section < numParameters; ++section)
    {
      auto const count = numControlsInSection (section);
      _controlKnob[static_cast<size_t> (section)].resize (
          static_cast<size_t> (count));

      for (int sub = 0; sub < count; ++sub)
        {
          // The Elevation section's three are knobs of their own: sliders,
          // drawn by the LookAndFeel as this device's knob (PotKnob). The
          // rest of the bar still works the way it did -- see ClipKnobs.hh
          // for why a field that is tapped stays a field.
          if (section == elevationSection || section == motionSection)
            {
              auto const spec = section == elevationSection
                                    ? elevationKnobSpec (sub)
                                    : motionKnobSpec (sub);

              auto knob = std::make_unique<PotKnob> ();
              knob->setLabel (spec.label);
              knob->setRange (spec.min, spec.max, spec.interval);
              knob->setFillsFromTheMiddle (spec.bipolar);
              knob->setWraps (spec.wraps);
              knob->onValueChange = [this, section, sub, k = knob.get ()] {
                if (onControlSet)
                  onControlSet (section, sub, k->getValue ());
              };

              // Two taps put it back where the page says -- which for reach
              // depends on where the figure sits, so it is the page's rule
              // rather than a number in the table.
              knob->onDoubleTapped = [this, section, sub] {
                if (onControlReset)
                  onControlReset (section, sub);
              };

              // A press picks the control out, exactly as a hit area did.
              knob->onDragStart = [this, section, sub] {
                if (onControlTapped)
                  onControlTapped (section, sub);
                if (onControlHeld)
                  onControlHeld (section, sub, true);
              };
              knob->onDragEnd = [this, section, sub] {
                if (onControlHeld)
                  onControlHeld (section, sub, false);
              };

              addAndMakeVisible (*knob);
              _controlKnob[static_cast<size_t> (section)]
                          [static_cast<size_t> (sub)] = std::move (knob);
              continue;
            }

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

      for (size_t sub = 0; sub < _controlKnob[s].size (); ++sub)
        if (auto &knob = _controlKnob[s][sub])
          knob->setBounds (cells[sub]);
    }

  for (index_t slot = 0; slot < numPadSlots; ++slot)
    _slotTouch[slot]->setBounds (_layout.slotButtons[slot]);

  for (int i = 0; i < numTransportKeys; ++i)
    _transportTouch[static_cast<size_t> (i)]->setBounds (
        _layout.transportButtons[static_cast<size_t> (i)]);

  _tabClipTouch->setBounds (_layout.tabClip);
  for (size_t channel = 0; channel < numChannelColumns; ++channel)
    {
      _faceTouch[channel]->setBounds (_layout.channelFaces[channel]);
      _faceMeter[channel]->setBounds (_layout.channelFaceMeters[channel]);
      for (std::size_t i = 0; i < numChannelPots; ++i)
        _facePots[channel][i]->setBounds (_layout.channelFacePots[channel][i]);
    }
  _tabActionTouch->setBounds (_layout.tabAction);
  _tabControllerTouch->setBounds (_layout.tabController);
  _tabMixerTouch->setBounds (_layout.tabMixer);
  _tabMainMixTouch->setBounds (_layout.tabMainMix);
  _elevationPictureTouch->setBounds (_layout.elevationFrame);
  _tabRecordTouch->setBounds (_layout.tabRecord);
  _tabMotionTouch->setBounds (_layout.tabMotion);
  _tabBrowserTouch->setBounds (_layout.tabBrowser);

  for (int i = 0; i < numSpeedButtons; ++i)
    _speedTouch[static_cast<size_t> (i)]->setBounds (
        _layout.speedButtons[static_cast<size_t> (i)]);

  _recModeTouch->setBounds (_layout.recModeButton);
}

void
ClipSettingsComponent::setTarget (int channel, int slot,
                                  juce::Colour channelColour)
{
  _channel = channel;
  _slot = slot;
  _channelColour = channelColour;
  markKnobs ();
  repaint ();
}

void
ClipSettingsComponent::setTrajectoryIcon (TrajectoryIconData const &icon)
{
  _trajectoryIcon = icon;
  repaint ();
}

void
ClipSettingsComponent::markKnobs ()
{
  // Which knob is picked out and which section is the live one -- the browse
  // state the encoders move through. It used to be handed to the painter on
  // every frame; a knob that draws itself has to be told instead.
  auto const mark = [this] (int section, int subIndex) {
    auto const s = static_cast<size_t> (section);
    for (size_t sub = 0; sub < _controlKnob[s].size (); ++sub)
      if (auto &knob = _controlKnob[s][sub])
        {
          knob->setActive (static_cast<int> (sub) == subIndex);
          knob->setSelected (_selectedIndex == section);
          knob->setKnobColour (_channelColour);
        }
  };

  mark (elevationIndex, _elevationSubIndex);
  mark (motionIndex, _motionSubIndex);
}

bool
ClipSettingsComponent::isOnTransportKey (juce::Component const *component,
                                         TransportKey key) const
{
  if (component == nullptr)
    return false;

  for (int i = 0; i < numTransportKeys; ++i)
    if (transportKeyOrder[i] == key)
      {
        auto const &touch = _transportTouch[static_cast<size_t> (i)];
        return touch != nullptr
               && (component == touch.get () || touch->isParentOf (component));
      }
  return false;
}

void
ClipSettingsComponent::putOnKnob (int section, int sub, double value)
{
  auto const s = static_cast<size_t> (section);
  if (s >= _controlKnob.size ()
      || static_cast<size_t> (sub) >= _controlKnob[s].size ())
    return;

  auto &knob = _controlKnob[s][static_cast<size_t> (sub)];

  // Not while a finger is on it: writing the value back into the knob that is
  // being turned is the page arguing with the hand.
  if (knob && !knob->isMouseButtonDown ())
    knob->setValue (value, juce::dontSendNotification);
}

void
ClipSettingsComponent::putReachOnKnob (int section, int sub,
                                       std::optional<float> held)
{
  auto const s = static_cast<size_t> (section);
  if (s >= _controlKnob.size ()
      || static_cast<size_t> (sub) >= _controlKnob[s].size ())
    return;

  if (auto &knob = _controlKnob[s][static_cast<size_t> (sub)])
    knob->setReach (reachOnKnob (section == elevationSection
                                     ? elevationKnobSpec (sub)
                                     : motionKnobSpec (sub),
                                 held));
}

void
ClipSettingsComponent::setClipName (juce::String const &name, bool drifted)
{
  if (name == _clipName && drifted == _clipDrifted)
    return;

  _clipName = name;
  _clipDrifted = drifted;
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
  // Signed now, so "not sweeping" cannot be said with a negative number any
  // more: -2 is outside the knob's range and is what the squeezes already use
  // for the same job.
  _elevationReach = std::clamp (reach, -1.0f, 1.0f);
  putOnKnob (motionSection, 2, _elevationReach);
  _elevationReachSwept
      = swept <= -2.f ? -2.f : std::clamp (swept, -1.0f, 1.0f);
  putReachOnKnob (motionSection, 2,
                  _elevationReachSwept <= -2.f
                      ? std::nullopt
                      : std::optional<float> (_elevationReachSwept));
  repaint ();
}

void
ClipSettingsComponent::setElevationBase (float base, float swept)
{
  // On elv, the other way up (clockwise is higher), with the sway's hold on
  // the line as the blue arc every swept knob wears. The graphic draws the
  // base only; the sway is shown on the knob since 2026-09-26.
  _elevationBase = std::clamp (base, 0.f, 1.f);
  putOnKnob (elevationSection, 3, knobForElevationBase (_elevationBase));
  putReachOnKnob (elevationSection, 3,
                  swept < 0.f ? std::nullopt
                              : std::optional<float> (knobForElevationBase (
                                    std::clamp (swept, 0.f, 1.f))));
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
  putOnKnob (elevationSection, 1, _elevationClipTop);
  repaint ();
}

void
ClipSettingsComponent::setElevationClipBottom (float clipBottom)
{
  _elevationClipBottom = std::clamp (clipBottom, 0.0f, 1.0f);
  putOnKnob (elevationSection, 0, _elevationClipBottom);
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

namespace
{
bool
samePoints (std::vector<ElevationSidePoint> const &a,
            std::vector<ElevationSidePoint> const &b)
{
  return a.size () == b.size ()
         && std::equal (a.begin (), a.end (), b.begin (),
                        [] (auto const &p, auto const &q) {
                          return std::abs (p.down - q.down) < 1e-4f
                                 && std::abs (p.across - q.across) < 1e-4f
                                 && p.behind == q.behind
                                 && p.startsStroke == q.startsStroke;
                        });
}

bool
sameChannel (ElevationChannel const &a, ElevationChannel const &b)
{
  return a.colour == b.colour && a.selected == b.selected
         && a.headValid == b.headValid
         && (!a.headValid
             || (std::abs (a.head.down - b.head.down) < 1e-4f
                 && std::abs (a.head.across - b.head.across) < 1e-4f))
         && samePoints (a.figure.line, b.figure.line)
         && samePoints (a.figure.dots, b.figure.dots);
}
}

void
ClipSettingsComponent::setElevationChannels (
    std::vector<ElevationChannel> const &clips)
{
  // Compared before storing: this arrives on every timer tick while a clip
  // plays, and a repaint of the whole bar for figures that have not moved is
  // a repaint the sphere could have had.
  auto same = clips.size () == _elevationChannels.size ();
  for (std::size_t c = 0; same && c < clips.size (); ++c)
    same = sameChannel (clips[c], _elevationChannels[c]);
  if (same)
    return;

  _elevationChannels = clips;
  repaint ();
}

void
ClipSettingsComponent::setSphereCamera (SphereCamera camera)
{
  if (camera.pitch == _sphereCamera.pitch && camera.turn == _sphereCamera.turn)
    return;

  _sphereCamera = camera;
  repaint ();
}

void
ClipSettingsComponent::setElevationSubIndex (int subIndex)
{
  _elevationSubIndex = subIndex;
  markKnobs ();
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
  putOnKnob (motionSection, 9, _motionBridgeBias);
  repaint ();
}

void
ClipSettingsComponent::setMotionSqueeze (float squeezeX, float squeezeY,
                                         float sweptX, float sweptY)
{
  _motionSqueezeX = juce::jlimit (-1.f, 1.f, squeezeX);
  _motionSqueezeY = juce::jlimit (-1.f, 1.f, squeezeY);
  putOnKnob (motionSection, 4, _motionSqueezeX);
  putOnKnob (motionSection, 6, _motionSqueezeY);
  _motionSqueezeXSwept
      = sweptX < -1.5f ? -2.f : juce::jlimit (-1.f, 1.f, sweptX);
  _motionSqueezeYSwept
      = sweptY < -1.5f ? -2.f : juce::jlimit (-1.f, 1.f, sweptY);
  putReachOnKnob (motionSection, 4,
                  _motionSqueezeXSwept < -1.5f
                      ? std::nullopt
                      : std::optional<float> (_motionSqueezeXSwept));
  putReachOnKnob (motionSection, 6,
                  _motionSqueezeYSwept < -1.5f
                      ? std::nullopt
                      : std::optional<float> (_motionSqueezeYSwept));
  repaint ();
}

void
ClipSettingsComponent::setMotionStretch (int x, int y)
{
  auto const heldX = juce::jlimit (-lfoMaxStep, lfoMaxStep, x);
  auto const heldY = juce::jlimit (-lfoMaxStep, lfoMaxStep, y);
  if (heldX == _motionSqueezeXLfo && heldY == _motionSqueezeYLfo)
    return;

  _motionSqueezeXLfo = heldX;
  _motionSqueezeYLfo = heldY;
  putOnKnob (motionSection, 5, _motionSqueezeXLfo);
  putOnKnob (motionSection, 7, _motionSqueezeYLfo);
  repaint ();
}

void
ClipSettingsComponent::setSweeps (int spin, int swell, int sway)
{
  auto const heldSpin = juce::jlimit (-lfoMaxStep, lfoMaxStep, spin);
  auto const heldSwell = juce::jlimit (-lfoMaxStep, lfoMaxStep, swell);
  auto const heldSway = juce::jlimit (-lfoMaxStep, lfoMaxStep, sway);
  if (heldSpin == _motionSpin && heldSwell == _motionSwell
      && heldSway == _elevationSway)
    return;

  _motionSpin = heldSpin;
  _motionSwell = heldSwell;
  putOnKnob (motionSection, 1, _motionSpin);
  putOnKnob (motionSection, 3, _motionSwell);
  _elevationSway = heldSway;
  putOnKnob (elevationSection, 2, _elevationSway);
  repaint ();
}

void
ClipSettingsComponent::setMotionFadeReach (float reach)
{
  _motionFadeReach = juce::jlimit (0.f, 1.f, reach);
  putOnKnob (motionSection, 8, _motionFadeReach);
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
  putOnKnob (motionSection, 0, _shapeRotate);
  // Where the spin is holding the rotation -- the blue arc, which went
  // missing when these became sliders (a3-motion-ui#35).
  putReachOnKnob (motionSection, 0, _shapeRotateReach);
  repaint ();
}

void
ClipSettingsComponent::setKnobsLaneDriven (
    std::array<bool, numKnobs> const &driven)
{
  for (int k = 0; k < numKnobs; ++k)
    {
      auto const place = placeOf (static_cast<Knob> (k));
      auto const s = static_cast<std::size_t> (place.section);
      auto const sub = static_cast<std::size_t> (place.sub);
      if (s < _controlKnob.size () && sub < _controlKnob[s].size ())
        if (auto &knob = _controlKnob[s][sub])
          knob->setLaneDriven (driven[static_cast<std::size_t> (k)]);
    }
}

void
ClipSettingsComponent::setEncoderMarks (
    std::vector<std::pair<int, int> > const &marked)
{
  for (std::size_t s = 0; s < _controlKnob.size (); ++s)
    for (std::size_t sub = 0; sub < _controlKnob[s].size (); ++sub)
      if (auto &knob = _controlKnob[s][sub])
        knob->setEncoderMarked (
            std::find (marked.begin (), marked.end (),
                       std::pair<int, int>{ static_cast<int> (s),
                                            static_cast<int> (sub) })
            != marked.end ());
}

void
ClipSettingsComponent::setKnobsWriting (
    std::array<bool, numKnobs> const &writing)
{
  for (int k = 0; k < numKnobs; ++k)
    {
      auto const place = placeOf (static_cast<Knob> (k));
      auto const s = static_cast<std::size_t> (place.section);
      auto const sub = static_cast<std::size_t> (place.sub);
      if (s < _controlKnob.size () && sub < _controlKnob[s].size ())
        if (auto &knob = _controlKnob[s][sub])
          knob->setWriting (writing[static_cast<std::size_t> (k)]);
    }
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
ClipSettingsComponent::setNextTakeLengthBeats (float beats)
{
  if (juce::approximatelyEqual (beats, _nextTakeLengthBeats))
    return;

  _nextTakeLengthBeats = beats;
  repaint ();
}

void
ClipSettingsComponent::setPatternLengthBeats (float beats)
{
  if (juce::approximatelyEqual (beats, _patternLengthBeats))
    return;

  _patternLengthBeats = beats;
  repaint ();
}

void
ClipSettingsComponent::setBeatsPerBar (int beats)
{
  if (beats == _beatsPerBar || beats < 1)
    return;

  _beatsPerBar = beats;
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
  markKnobs ();
  repaint ();
}

void
ClipSettingsComponent::setSelectedParameterIndex (int index)
{
  jassert (index >= 0 && index < numParameters);
  _selectedIndex = index;
  markKnobs ();
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
  g.fillAll (toColour (theme ().surface, theme ().panelOpacity));

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

  g.setColour (toColour (theme ().textPrimary, theme ().alphaFillEmphasis));
  g.drawRect (_layout.globalBounds, frameThickness);

  // Not on the pages that show every slot at once -- the pads page and the
  // browser -- where choosing one of them says something untrue about what you
  // are looking at. Both of
  // the clip's faces describe a single slot, and the record face -- the take
  // about to be written -- is the one where being sure which slot it is
  // matters most.
  if (!pageCoversClipArea (_page))
    for (index_t slot = 0; slot < numPadSlots; ++slot)
      {
        auto const bounds = _layout.slotButtons[slot];
        if (bounds.isEmpty ())
          continue;

        // The one you are looking at is filled in the channel's colour, the
        // other outlined -- the same way the page tabs beside them say which
        // page you are on, because they answer the same kind of question.
        auto const here = slot == static_cast<index_t> (_slot);

        g.setColour (here ? _channelColour.withAlpha (theme ().alphaDisabled)
                          : toColour (theme ().textPrimary,
                                     theme ().alphaFill));
        g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);
        g.setColour (toColour (theme ().textPrimary,
                               here ? theme ().alphaDisabled
                                    : theme ().alphaOutline));
        g.drawRoundedRectangle (bounds.toFloat (), theme ().radiusControl,
                                theme ().strokeThin);

        // The number alone. "Slot" was three quarters of a key spent saying
        // what two keys side by side already say.
        auto const name = juce::String (slot + 1);
        g.setFont (juce::Font (fontFor (FontRole::Header, bounds, name),
                               here ? juce::Font::bold : juce::Font::plain));
        g.setColour (here ? _channelColour
                          : toColour (theme ().textPrimary,
                                     theme ().alphaInactive));
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

  // The global strip stands on every page: the faces and the transport belong
  // to the device rather than to one view of the clip, and losing them while
  // you are firing clips is exactly the wrong moment to lose them.
  paintGlobalSection (g, _selectedIndex == globalIndex);

  if (pageCoversClipArea (_page))
    return; // ControllerComponent / BrowserComponent draws the rest

  // Which cards a page shows (2026-09-26): CLIP the shape, dir and end, and
  // the lengths; MOTION the movement; REC the shape and the take.
  if (_page == BarPage::Motion)
    {
      paintMotionSection (g, _selectedIndex == motionIndex);
      paintElevationSection (g, _selectedIndex == elevationIndex);
      return;
    }

  paintTrajectorySection (g, _selectedIndex == trajectoryIndex);

  if (_page == BarPage::Record)
    {
      // The take being set up: its settings in the middle, its length on the
      // right, where the lengths stand on CLIP too.
      paintRecordSection (g);
      paintLengthSection (g);
      return;
    }

  paintPlaySection (g);
  paintLengthSection (g);

  // Last, so it covers whichever section it belongs to.
}

void
ClipSettingsComponent::setTakeState (bool unsaved, bool discardArmed)
{
  if (unsaved == _takeUnsaved && discardArmed == _takeDiscardArmed)
    return;

  _takeUnsaved = unsaved;
  _takeDiscardArmed = discardArmed;
  repaint (_layout.transportButtons.front ());
  repaint (_layout.transportButtons.back ());
}

void
ClipSettingsComponent::setRecArmed (bool armed)
{
  if (_transportArmed == armed)
    return;

  _transportArmed = armed;
  repaint ();
}

void
ClipSettingsComponent::setTransportState (bool playing, bool recording,
                                          bool scheduled)
{
  if (playing == _transportPlaying && recording == _transportRecording
      && scheduled == _transportScheduled)
    return;

  _transportPlaying = playing;
  _transportRecording = recording;

  if (scheduled != _transportScheduled)
    {
      _transportScheduled = scheduled;

      // Started bright, so the first thing a press produces is a light rather
      // than a dark half-period -- a blink that begins by going out reads as
      // the key having been missed. From here on the beat turns it over; see
      // pulseOnBeat().
      _waitBlinkOn = scheduled;
    }

  repaint ();
}

void
ClipSettingsComponent::setActionActive (bool active)
{
  if (active == _actionActive)
    return;

  _actionActive = active;
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
    g.setColour (active ? _channelColour.withAlpha (theme ().alphaDisabled)
                        : toColour (theme ().textPrimary, theme ().alphaFill));
    g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);
    g.setColour (toColour (theme ().textPrimary,
                           active ? theme ().alphaDisabled
                                  : theme ().alphaOutline));
    g.drawRoundedRectangle (bounds.toFloat (), theme ().radiusControl,
                            theme ().strokeThin);

    g.setFont (juce::Font (fontFor (FontRole::Header, bounds, label),
                           active ? juce::Font::bold : juce::Font::plain));
    // Full opacity rather than an alpha rung -- the active tab's label has
    // always meant no dimming at all. This used to be `active ? 1.f : 0.55f`;
    // 1.f fits no rung, and full opacity is the absence of an emphasis
    // decision rather than one of its rungs, so it deliberately gets no role
    // of its own. See issues/a3-motion-ui-metric-role-deviations.md (Task
    // 16).
    g.setColour (active ? toColour (theme ().textPrimary)
                        : toColour (theme ().textPrimary,
                                   theme ().alphaInactive));
    g.drawFittedText (label, bounds, juce::Justification::centred, 1);
  };

  // The four transport keys. The mark always carries the action's own colour;
  // the key's ground lights only while it is doing something, so the row reads
  // as four labelled controls with one or two of them active rather than as
  // four colours competing. Empty on the pads page, which is these four
  // controls already.
  // Built once: the four keys read one state, and four copies of it assembled
  // in a loop is four chances for them to disagree about the same moment.
  TransportState transport;
  transport.recording = _transportRecording;
  transport.playing = _transportPlaying;
  transport.scheduled = _transportScheduled;
  transport.actionActive = _actionActive;
  transport.stopPressed = _stopPressed;
  transport.unsaved = _takeUnsaved;
  transport.discardArmed = _takeDiscardArmed;
  transport.armed = _transportArmed;

  for (int i = 0; i < numTransportKeys; ++i)
    {
      auto const bounds = _layout.transportButtons[static_cast<size_t> (i)];
      if (bounds.isEmpty ())
        continue;

      auto const key = transportKeyOrder[i];
      auto const face = transportFace (key, transport);
      auto const mark = transportColour (face);

      // One rule for both screens -- see transportKeyGround().
      auto const ground = transportKeyGround (key, transport);

      // A blink is the lit ground, taken away and put back. Same colour, so
      // the key that waits and the key that runs are plainly the same key at
      // two moments rather than two different signals.
      auto const lit = ground == TransportGround::Lit
                       || (ground == TransportGround::Waiting && _waitBlinkOn);

      g.setColour (lit ? mark.withAlpha (theme ().alphaDisabled)
                       : toColour (theme ().textPrimary, theme ().alphaFill));
      g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);
      g.setColour (toColour (theme ().textPrimary, theme ().alphaOutline));
      g.drawRoundedRectangle (bounds.toFloat (), theme ().radiusControl,
                              theme ().strokeThin);

      // Shapes, not words -- see drawTransportGlyph(), which the pads page
      // draws from as well so the same action is the same mark in both places.
      // The mark never changes with the state; the ground above does.
      g.setColour (mark);
      drawTransportGlyph (g, transportGlyphArea (bounds.toFloat ()), face);
    }

  paintTab (_layout.tabClip, "CLIP",
            pageTabIsLit (BarPage::Clip, _page, _mainMixOpen));
  paintTab (_layout.tabAction, "ACTION",
            pageTabIsLit (BarPage::Action, _page, _mainMixOpen));
  paintTab (_layout.tabController, "PADS",
            pageTabIsLit (BarPage::Controller, _page, _mainMixOpen));
  paintTab (_layout.tabMixer, "CHMIX",
            pageTabIsLit (BarPage::Mixer, _page, _mainMixOpen));
  paintTab (_layout.tabMainMix, "MAINMIX", _mainMixOpen);
  paintTab (_layout.tabRecord, "REC",
            pageTabIsLit (BarPage::Record, _page, _mainMixOpen));
  paintTab (_layout.tabMotion, "MOTION",
            pageTabIsLit (BarPage::Motion, _page, _mainMixOpen));

  // A word like the three beside it. It was a folder mark, on the reasoning
  // that the tabs are views of the clip and this one leaves it -- but once
  // every key in the row became one size, a drawing among words was the odd
  // one out rather than the distinct one, and at this size it read as a
  // smudge.
  paintTab (_layout.tabBrowser, "FILES",
            pageTabIsLit (BarPage::Browser, _page, _mainMixOpen));

}

void
ClipSettingsComponent::paintCameraMark (juce::Graphics &g,
                                        juce::Rectangle<int> bounds) const
{
  if (bounds.isEmpty ())
    return;

  // A camera, drawn: a body, a lens in it and the finder on top. In the
  // accent while camera mode is on, muted while it waits to be chosen.
  auto const area = bounds.toFloat ();
  auto const w = area.getWidth ();
  auto const body = area.withTrimmedTop (w * 0.3f).withTrimmedBottom (w * 0.1f);
  auto const finder = juce::Rectangle<float> (w * 0.3f, w * 0.18f)
                          .withCentre ({ body.getCentreX (),
                                         body.getY () - w * 0.08f });
  auto const lensR = body.getHeight () * 0.3f;

  g.setColour (_cameraMode ? toColour (theme ().accent)
                           : toColour (theme ().textMuted));
  g.fillRoundedRectangle (finder, w * 0.04f);
  g.drawRoundedRectangle (body, w * 0.1f, theme ().strokeThin * 1.5f);
  g.drawEllipse (body.getCentreX () - lensR, body.getCentreY () - lensR,
                 lensR * 2.f, lensR * 2.f, theme ().strokeThin * 1.5f);
}

void
ClipSettingsComponent::setCameraMode (bool on)
{
  if (_cameraMode == on)
    return;

  _cameraMode = on;
  repaint (_layout.elevationFrame);
}

void
ClipSettingsComponent::setMainMixOpen (bool open)
{
  if (_mainMixOpen == open)
    return;

  _mainMixOpen = open;
  repaint (_layout.tabMainMix);
}

juce::Rectangle<int>
ClipSettingsComponent::clipContentBounds () const
{
  return _layout.clipContent;
}

void
ClipSettingsComponent::showControlsOfPage ()
{
  // Every control stands on the pages controlIsOnPage() names and takes no
  // touch anywhere else: a hit area with nothing under it is how a finger
  // changes a value it cannot see. The knobs go with their hit areas, and
  // that half matters as much -- a child is not painted by its parent, so a
  // knob left visible draws itself, caption and all, straight through
  // whatever page is on top ("man sieht die pots im hintergrund").
  for (int section = 0; section < numParameters; ++section)
    {
      auto const s = static_cast<size_t> (section);
      for (size_t sub = 0; sub < _controlTouch[s].size (); ++sub)
        _controlTouch[s][sub]->setVisible (
            controlIsOnPage (section, static_cast<int> (sub), _page));

      for (size_t sub = 0; sub < _controlKnob[s].size (); ++sub)
        if (auto &knob = _controlKnob[s][sub])
          knob->setVisible (
              controlIsOnPage (section, static_cast<int> (sub), _page));
    }

  // A card's own touch stands where the card is drawn: Shape on CLIP and REC,
  // Elevation and Motion on CLIP only -- on REC their columns are the Record
  // card.
  for (int section = 0; section < numClipSections; ++section)
    _sectionTouch[static_cast<size_t> (section)]->setVisible (
        controlIsOnPage (section, 0, _page));

  _recModeTouch->setVisible (_page == BarPage::Record);

  // The lengths stand on CLIP and on REC, where the lit one is the take's
  // length.
  for (auto &button : _speedTouch)
    if (button)
      button->setVisible (lengthKeysStandOn (_page));
}

void
ClipSettingsComponent::setPage (BarPage page)
{
  if (_page == page)
    return;

  _page = page;

  // Laid out again: CLIP and MOTION place their controls in fields and rows
  // of their own since 2026-09-27, so the layout depends on the page.
  resized ();
  showControlsOfPage ();
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
ClipSettingsComponent::setSpeedButtons (
    std::array<int, numSpeedButtons> const &speeds)
{
  if (speeds == _speedButtonLog2)
    return;

  _speedButtonLog2 = speeds;
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

  // No key of its own on screen any more -- MENU is in the status bar -- but
  // the panel's MENU LED still reads this through functionKeyLook().
  _menuOpen = open;
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

  // One wash for every card, whichever section was last touched. The selected
  // one used to be filled with the channel's colour -- a coloured field the
  // size of a third of the bar, laid over the controls you are reading, that
  // moved every time a finger landed somewhere else. It said which section
  // was armed and shouted it, and what actually needs saying is which
  // *control* is, which the pointer and the control's own colour already do.
  g.setColour (toColour (theme ().textPrimary, cardWash));
  g.fillRoundedRectangle (card.toFloat (), theme ().radiusCard);

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

  g.setColour (toColour (theme ().textPrimary, theme ().alphaFill));
  g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);
  g.setColour (toColour (theme ().textPrimary, theme ().alphaOutline));
  g.drawRoundedRectangle (bounds.toFloat (), theme ().radiusControl,
                          theme ().strokeThin);
}

void
ClipSettingsComponent::setChannelPots (int channel,
                                       ChannelPotValues const &values)
{
  if (channel < 0 || channel >= static_cast<int> (numChannelColumns))
    return;

  auto const c = static_cast<std::size_t> (channel);
  for (std::size_t i = 0; i < numChannelPots; ++i)
    showChannelPot (*_facePots[c][i], values.set[i], values.effective[i],
                    _channelFaceColours[c]);
}

void
ClipSettingsComponent::setChannelProgress (
    std::array<float, numChannelColumns> const &progress)
{
  if (progress == _channelProgress)
    return;

  _channelProgress = progress;
  repaint (_layout.channelFacesFrame);
}

void
ClipSettingsComponent::repaintChannelMeters ()
{
  for (auto &meter : _faceMeter)
    meter->repaint ();
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
                         && !pageCoversClipArea (_page);
      auto const colour = _channelFaceColours[channel];

      // The face carries its channel's colour always, filled when it is the
      // one the bar describes and washed when it is not. A colour that came
      // and went would make finding a channel a matter of remembering which
      // one you were on -- exactly what the single CLIP tab made you do.
      g.setColour (shown ? colour.withAlpha (theme ().alphaInactive)
                        : colour.withAlpha (theme ().alphaOutline));
      g.fillRoundedRectangle (face.toFloat (), theme ().radiusControl);
      g.setColour (shown ? colour : colour.withAlpha (theme ().alphaDisabled));
      g.drawRoundedRectangle (face.toFloat (), theme ().radiusControl,
                              shown ? theme ().strokeThick
                                    : theme ().strokeThin);

      // And the number in it is the slot. Not the channel: which channel this
      // is, is what the colour says, and it says it without being read. Which
      // slot cannot be a colour, so it is the one thing here worth a glyph --
      // and touching the face you are already on turns it over.
      auto const slotName
          = juce::String (juce::jlimit (0, static_cast<int> (numPadSlots) - 1,
                                        _channelFaceSlots[channel])
                          + 1);

      // The rest of the face is the clip's progress, filled from the left in
      // the channel's colour as a clip slot fills in a DAW, with the slot
      // number at its start.
      auto const bar = _layout.channelFaceProgress[channel];
      g.setColour (colour.withAlpha (theme ().alphaOutline));
      g.fillRect (bar);
      auto const fill = progressFill (bar, _channelProgress[channel]);
      g.setColour (colour);
      g.fillRect (fill);

      // Black or white once the fill runs under the number, or it vanishes in
      // its own colour; the channel's colour on the washed bar otherwise.
      auto const numberArea = bar.withWidth (juce::jmin (
          bar.getWidth (), bar.getHeight () * 3 / 2));
      auto const onFill = fill.getRight () > numberArea.getCentreX ();
      g.setFont (juce::Font (fontFor (FontRole::Header, numberArea, slotName),
                             shown ? juce::Font::bold : juce::Font::plain));
      g.setColour (onFill ? padGlyphInk (colour)
                          : readableInk (colour, toColour (theme ().background),
                                         toColour (theme ().textPrimary)));
      g.drawText (slotName, numberArea, juce::Justification::centred);
    }
}

void
ClipSettingsComponent::paintPlaySection (juce::Graphics &g)
{
  // CLIP's middle card: dir over end, each a field that steps on a tap -- no
  // chevron, because nothing opens.
  auto const isSelected = _selectedIndex == trajectoryIndex;
  g.setColour (toColour (theme ().textPrimary, cardWash));
  g.fillRoundedRectangle (_layout.playCard.toFloat (), theme ().radiusCard);
  paintSectionLabel (g, _layout.playLabel, "dir / end", false);

  paintBarButton (g, _layout.directionButton,
                  value::directionNames[_motionDirection], caption::direction,
                  _trajectorySubIndex == 2 && isSelected, false);
  paintBarButton (g, _layout.endActionButton,
                  value::endActionNames[_motionEndAction], caption::endAction,
                  _trajectorySubIndex == 3 && isSelected, false);
}

void
ClipSettingsComponent::paintLengthSection (juce::Graphics &g)
{
  // CLIP's right card: the four lengths, two by two. What each is is the
  // performer's: tapped for the speed it carries, dragged to give it another,
  // and named from its value, so a key retells itself the moment it is
  // dragged.
  auto const isSelected = _selectedIndex == trajectoryIndex;
  g.setColour (toColour (theme ().textPrimary, cardWash));
  g.fillRoundedRectangle (_layout.lengthCard.toFloat (), theme ().radiusCard);
  paintSectionLabel (g, _layout.lengthLabel, "length", false);

  for (int i = 0; i < numSpeedButtons; ++i)
    paintBarButton (
        g, _layout.speedButtons[static_cast<size_t> (i)],
        speedKeyName (_speedButtonLog2[static_cast<size_t> (i)],
                      _patternLengthBeats),
        {},
        speedKeyIsActive (_speedButtonLog2, i, _speedLog2, _speedDragIndex),
        isSelected);
}

void
ClipSettingsComponent::paintRecordSection (juce::Graphics &g)
{
  // No card of its own since 2026-09-27: REC is one area of fields, and
  // fade|bias share one, grounded like the keys around it.
  if (!_layout.pageFields[5].isEmpty ())
    paintBarButton (g, _layout.pageFields[5], {}, {}, false, false);

  // The rec mode in its own colour: how much of an old take this pass will
  // destroy, on the same scale the rest of the device uses. It carries a
  // value and names it, so it does not light.
  auto const look = functionKeyLook ();
  paintBarButton (g, _layout.recModeButton, recModeName (_recMode), "recmode",
                  false, false, functionKeyColour (FunctionKey::RecMode, look));

  // Fade and bias are knobs and draw themselves (PotKnob).
}

void
ClipSettingsComponent::paintGlobalSection (juce::Graphics &g,
                                           bool isSelected)
{
  paintSectionCard (g, globalIndex, isSelected);

  // Two blocks, each in a frame of its own: whose clip above, what to do to
  // it below. Set off from the card rather than boxed in it -- a heavier edge
  // would make the strip read as two panels that happen to touch.
  // The elevation picture heads the strip, so it stands on every page: where
  // the shown clip sits and how high it may go is worth seeing while the
  // pads or the mixer are up, too.
  // In a grey field of its own like the faces and the transport, washed in
  // the accent while it is selected -- camera mode, where the big sphere turns
  // the view -- the way a lit key says what it is doing.
  paintSetOffFrame (g, _layout.elevationFrame);
  if (_cameraMode)
    {
      g.setColour (toColour (theme ().accent, theme ().alphaFillEmphasis));
      g.fillRoundedRectangle (_layout.elevationFrame.toFloat (),
                              theme ().radiusControl);
    }
  paintElevationGraphic (g, _layout.elevationGraphic, _cameraMode);
  paintCameraMark (g, _layout.elevationCameraMark);

  paintSetOffFrame (g, _layout.channelFacesFrame);
  paintSetOffFrame (g, _layout.transportFrame);
  paintChannelFaces (g);

  // Every key's colour from the one rule (theme/FunctionKeyColours.hh), which
  // is what makes it one rule. Each of these used to carry its own copy —
  // REC's said "orange armed, red running" long after the rule had been
  // changed to say red always, and nothing was wrong anywhere: the screen
  // simply was not asking. Two displays reading one rule only works if both
  // of them read it.

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
  g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);

  // The same grey outline paintBarButton draws, so all six keys of the global
  // section have one face. The outline used to take the tint, which is what
  // the paragraph above says it must not: a blue line is brighter than the
  // grey and a dark red one is dimmer, so on this ground MENU stood proud and
  // REC sat sunk, and the six read as keys at three different depths. The
  // word carries the colour. The ground carries what is happening. The frame
  // carries neither and is therefore the same everywhere.
  g.setColour (toColour (theme ().textPrimary, trackWash));
  g.drawRoundedRectangle (bounds.toFloat (), theme ().radiusControl,
                          theme ().strokeThin);

  g.setFont (juce::Font (fontFor (FontRole::Body, bounds, label),
                         juce::Font::plain));
  g.setColour (tint);
  g.drawFittedText (label, bounds, juce::Justification::centred, 1);
}

/** The bar's one button face — the global section's four, Elevation's flat
 *  and pole, and Motion's two lists. The drawing lives in BarButton so the
 *  mixer's keys can use the same one: the overlay covers the sphere and this
 *  bar stays visible under it, so a second face would be a second face on
 *  screen at the same moment. */
void
ClipSettingsComponent::paintBarButton (juce::Graphics &g,
                                       juce::Rectangle<int> bounds,
                                       juce::String const &label,
                                       juce::String const &caption,
                                       bool isActive, bool isSelected,
                                       juce::Colour valueColour)
{
  // Qualified, because the member name hides the one in the namespace.
  a3::paintBarButton (g, bounds, _layout.metrics, _channelColour, label,
                      caption, isActive, isSelected, valueColour);
}

void
ClipSettingsComponent::flashStop ()
{
  _stopPressed = true;
  repaint (_layout.transportButtons[static_cast<size_t> (stopIndex ())]);

  // One shot, and the same length as the tap flash: both say "your finger
  // landed" and nothing else, so they should last the same.
  _stopFlash.startTimer (stopFlashMillis);
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

  // The panel's SHIFT LED reads this; the screen has no SHIFT key since
  // 2026-09-26.
  _shiftHeld = held;
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
ClipSettingsComponent::pulseOnBeat ()
{
  // The play key's wait, turned over once a beat.
  //
  // On the clock rather than on a timer of its own, because what it is waiting
  // for *is* the clock -- and because the wait is no longer half a beat. Since
  // pressing play on a running clip means "finish this lap", it can last a
  // whole phrase: a 32-bar clip at 100 BPM is seventy-seven seconds, and a
  // 120ms blink over seventy-seven seconds is a strobe. On the beat it reads
  // as counting down to the end, and it can never run faster than the music.
  if (_transportScheduled)
    {
      _waitBlinkOn = !_waitBlinkOn;
      repaint (
          _layout.transportButtons[static_cast<size_t> (playPauseIndex ())]);
    }

  // A press owns the key and its timer while it lasts. Without this a beat
  // landing under the finger restarted the timer at 70ms and cut the press's
  // 110ms flash short — the one feedback that says the tap was taken.
  if (_tapLit)
    return;

  // For the panel's TAP LED: the screen's TAP is the status bar's beat
  // display since 2026-09-26.
  _tapBeat = true;

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

  // The row of channel faces on top, unscaled: it is a row of keys, sized
  // like the bar's buttons, not a share of the bar.
  return juce::jmax (
             1, juce::roundToInt (static_cast<float> (needed)
                                  * juce::jlimit (
                                      0.5f, 2.f,
                                      theme ().clipSettingsHeightScale)))
         + channelRowHeight (knobDiam, width);
}



void
ClipSettingsComponent::paintTrajectorySection (juce::Graphics &g,
                                               bool isSelected)
{
  paintSectionCard (g, trajectoryIndex, isSelected);

  auto const &metrics = _layout.metrics;

  {
      // The clip field: which settings the slot is played with, and a place to
      // push through them with a thumb. Not the shape's name -- that is over
      // the picture, beside the control that changes it.
      // The caption carries the next take's length. There is no page to read
      // it off any more, and a length you only learn after recording is one
      // you learn too late.
      auto const clipCaption
          = _nextTakeLengthBeats > 0.f
                ? juce::String (caption::clip) + "  "
                      + beatsName (_nextTakeLengthBeats)
                : juce::String (caption::clip);

      paintBarButton (g, _layout.clipField,
                      _clipName.isEmpty () ? juce::String ("--") : _clipName,
                      clipCaption, false,
                      isSelected && _trajectorySubIndex == 1);

      // Turned since it was loaded. The warning colour rather than the danger
      // one: nothing is lost yet, something is merely waiting to be written --
      // and it is the same dot the slot keys used to carry, in the same
      // colour, because it answers the same question.
      if (_clipDrifted && !_layout.clipField.isEmpty ())
        {
          g.setColour (toColour (theme ().warning));
          g.fillEllipse (driftMark (_layout.clipField).toFloat ());
        }
    }

  // On a page of fields the picture has a field of its own, grounded like the
  // seven keys around it.
  if (!_layout.pageFields[4].isEmpty ())
    paintBarButton (g, _layout.trajectoryIcon, {}, {}, false, false);

  // Pictogram, in the middle of the field and off its edge -- see
  // shapeFieldIconArea().
  auto iconArea = shapeFieldIconArea (_layout.trajectoryIcon).toFloat ();
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
  g.setColour (Colours::barText (isSelected && _trajectorySubIndex == 0));
  g.drawFittedText (_trajectoryName, _layout.trajectoryName,
                    juce::Justification::centred, 1);
}

void
ClipSettingsComponent::paintMotionSection (juce::Graphics &g,
                                           bool isSelected)
{
  paintSectionCard (g, motionIndex, isSelected);

  // One field per encoder, grounded like CLIP's and REC's keys; the knobs
  // draw themselves on top.
  for (auto const &field : _layout.pageFields)
    if (!field.isEmpty ())
      paintBarButton (g, field, {}, {}, false, false);

  auto const &metrics = _layout.metrics;
  auto const &cells = _layout.controls[motionIndex];

  // A painter that outlives a renumbering reads past the end of the vector,
  // and what that looks like on screen is white rectangles flickering in and
  // out -- not a crash, and nothing that says where it came from. Checked
  // rather than trusted: the sections have been renumbered three times.
  if (cells.size ()
      < static_cast<size_t> (numControlsInSection (motionIndex)))
    return;

  // In reading order, which is also sub-index order: rot with its spin, reach
  // with its swell, each squeeze with its own stretch, the fade with the bias.
  // Every row a standing value beside the movement that works on it.
  //
  // rot is a closed ring: rotation comes round to itself, so its scale has to
  // as well. The pointer is where the hand left it; the blue runs from there
  // to where the spin is holding the shape right now.
  // The ten knobs draw themselves (PotKnob, from the table in ClipKnobs.hh);
  // the arcs that say where a sweep is holding a value travel with them.

}

void
ClipSettingsComponent::paintElevationSection (juce::Graphics &g,
                                              bool isSelected)
{
  // On MOTION since 2026-09-27 Elevation shares Motion's one area: a second
  // wash over it would darken the whole page.
  if (_layout.sectionCards[elevationIndex]
      != _layout.sectionCards[motionIndex])
    paintSectionCard (g, elevationIndex, isSelected);

  auto const &metrics = _layout.metrics;
  auto const &cells = _layout.controls[elevationIndex];

  // See paintMotionSection(): a painter left behind by a renumbering reads
  // past the end and draws garbage rather than failing.
  if (cells.size ()
      < static_cast<size_t> (numControlsInSection (elevationIndex)))
    return;

  // The picture of what these set stands at the top of the global strip since
  // 2026-09-26 -- see paintGlobalSection.
  // Bottom then top, left to right. They were the other way round, which is
  // the one order that has nothing to say for it: the graphic above them is a
  // room seen from the side, and in a room seen from the side the floor is not
  // on the right of the ceiling.
  // The three knobs draw themselves (PotKnob); what is above them is the
  // page's.

  // The graphic's own slow movement -- how fast the line travels and towards
  // which pole -- is the third of those knobs.

}

void
ClipSettingsComponent::paintElevationGraphic (juce::Graphics &g,
                                              juce::Rectangle<int> bounds,
                                              bool isSelected)
{
  // Nothing in here reads differently for being the selected section: it is a
  // picture of where the sound is, and where the sound is does not depend on
  // which card a finger last touched. Kept in the signature because every
  // painter in this bar takes it.
  juce::ignoreUnused (isSelected);

  // The instrument keeps the channel's colour -- it is a picture of where the
  // sound is, not a word about a setting.
  auto const iconColour = _channelColour;
  auto const r = static_cast<float> (
                     juce::jmin (bounds.getWidth (), bounds.getHeight ()))
                 * 0.42f;
  auto const centre = bounds.toFloat ().getCentre ();

  // A view of the sphere, kept a quarter turn from the one above it -- see
  // elevationSideCamera(). clip-top/clip-bottom clamp the reachable range in
  // from each end (see HeightMap::mapTo3D()); what is left is [rangeLow,
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

  // Everything in here is projected, not ruled. The circle is a second view
  // of the room, kept a quarter turn from the sphere above: overhead up there
  // is a side view down here, and a side view up there is an overhead down
  // here, so the pair of them always shows the room from two directions at
  // once and what one loses the other has.
  //
  // Which means a height is no longer a horizontal line. The set of points at
  // one height is a circle of latitude, and a circle of latitude seen from
  // anywhere but its own plane is an ellipse -- so the base and the two cuts
  // are drawn as the rings they are.
  auto const place = [&] (ElevationSidePoint const &point) {
    return juce::Point<float> (centre.x + point.across * r,
                               centre.y + point.down * r);
  };

  auto const ringPoints = [&] (float frac) {
    std::vector<juce::Point<float> > points;
    points.reserve (65);

    auto const theta = std::clamp (frac, 0.f, 1.f)
                       * juce::MathConstants<float>::pi;
    auto const sinT = std::sin (theta);
    auto const cosT = std::cos (theta);

    for (int step = 0; step <= 64; ++step)
      {
        auto const phi = juce::MathConstants<float>::twoPi
                         * static_cast<float> (step) / 64.f;
        points.push_back (place (elevationSideView (
            Pos::fromCartesian (sinT * std::cos (phi), sinT * std::sin (phi),
                                cosT),
            _sphereCamera)));
      }

    return points;
  };

  // The stretch of room between two heights, as a shape rather than as a pile
  // of thick rings. Stacked strokes were what drew a flat bar across the top
  // of the picture: near a pole a ring is a few pixels across and a stroke
  // wide enough to close the gap to the next one overshoots it by its own
  // width in every direction.
  auto const bandBetween = [&] (float from, float to) {
    juce::Path shape;

    auto const outer = ringPoints (juce::jmax (from, to));
    auto const inner = ringPoints (juce::jmin (from, to));

    shape.startNewSubPath (outer.front ());
    for (size_t i = 1; i < outer.size (); ++i)
      shape.lineTo (outer[i]);
    for (auto i = inner.size (); i-- > 0;)
      shape.lineTo (inner[i]);
    shape.closeSubPath ();

    return shape;
  };

  auto const latitude = [&] (float frac) {
    juce::Path ring;
    auto const theta = std::clamp (frac, 0.f, 1.f)
                       * juce::MathConstants<float>::pi;
    auto const sinT = std::sin (theta);
    auto const cosT = std::cos (theta);

    for (int step = 0; step <= 64; ++step)
      {
        auto const phi = juce::MathConstants<float>::twoPi
                         * static_cast<float> (step) / 64.f;
        auto const at = place (elevationSideView (
            Pos::fromCartesian (sinT * std::cos (phi), sinT * std::sin (phi),
                                cosT),
            _sphereCamera));

        if (step == 0)
          ring.startNewSubPath (at);
        else
          ring.lineTo (at);
      }

    return ring;
  };

  juce::Path circlePath;
  circlePath.addEllipse (centre.x - r, centre.y - r, r * 2.f, r * 2.f);

  g.saveState ();
  g.reduceClipRegion (circlePath);

  // What the clips have taken away, shaded rather than merely edged: the eye
  // should find the reachable part without reading a number. Rings rather than
  // rectangles, because a height is a ring here.
  {
    g.setColour (toColour (theme ().surface, clippedZoneOpacity));
    if (bandLow > 1e-3f)
      g.fillPath (bandBetween (0.f, bandLow));
    if (bandHigh < 1.f - 1e-3f)
      g.fillPath (bandBetween (bandHigh, 1.f));
  }

  // ── The figure ────────────────────────────────────────────────────────
  //
  // Where the sound actually goes. The sphere above says where in the room the
  // figure is and this says what the sphere above has lost, whichever way it
  // is turned.
  // In the order given: the selected clip last and full, the others muted
  // under it, so the clip the bar describes stays the one that stands out.
  auto const shade = [] (ElevationChannel const &clip) {
    return clip.selected ? 1.f : 0.45f;
  };
  for (auto const &channel : _elevationChannels)
    {
      auto const &line = channel.figure.line;
      auto const near
          = channel.colour.withMultipliedAlpha (theme ().alphaTextStrong
                                                * shade (channel));
      auto const far
          = channel.colour.withMultipliedAlpha (theme ().alphaFillEmphasis
                                                * shade (channel));

      if (line.size () > 1)
        {
          auto previous = place (line.front ());
          for (size_t i = 1; i < line.size (); ++i)
            {
              auto const point = place (line[i]);

              // The pen lift is decided in the room, not here -- see
              // ElevationSidePoint::startsStroke.
              if (!line[i].startsStroke)
                {
                  g.setColour (line[i].behind ? far : near);
                  g.drawLine (previous.x, previous.y, point.x, point.y,
                              theme ().strokeMedium);
                }

              previous = point;
            }
        }

      // A shape of dots is its dots here as on the sphere.
      auto const dotR = juce::jmax (1.5f, r * 0.05f);
      for (auto const &dot : channel.figure.dots)
        {
          auto const at = place (dot);
          g.setColour (dot.behind ? far : near);
          g.fillEllipse (at.x - dotR, at.y - dotR, dotR * 2.f, dotR * 2.f);
        }
    }

  // The two cuts, as the rings they are: a boundary you can see is a boundary
  // you can aim a finger at.
  g.setColour (toColour (theme ().textPrimary, theme ().alphaFillEmphasis));
  g.strokePath (latitude (bandLow), juce::PathStrokeType (1.f));
  g.strokePath (latitude (bandHigh), juce::PathStrokeType (1.f));

  // Ear height, the one ring worth having whatever else is set.
  g.setColour (toColour (theme ().textPrimary, theme ().alphaGuide));
  g.strokePath (latitude (0.5f), juce::PathStrokeType (1.f));

  // And a graticule every thirty degrees, so the picture says how far it has
  // been turned as well as how high things are: rings that are straight lines
  // in the side view and circles in the overhead one, which is the whole
  // difference between the two views said without a word.
  g.setColour (toColour (theme ().textPrimary, theme ().alphaFill));
  for (int degrees = 30; degrees < 180; degrees += 30)
    if (degrees != 90)
      g.strokePath (latitude (static_cast<float> (degrees) / 180.f),
                    juce::PathStrokeType (1.f));

  // The base: where the middle of the trajectory sits, and the one ring in
  // here a finger sets. Drawn boldest and last, so the sway's own mark never
  // covers it.
  g.setColour (toColour (theme ().surface, outlineOpacity));
  g.strokePath (latitude (baseFrac), juce::PathStrokeType (4.f));
  g.setColour (iconColour);
  g.strokePath (latitude (baseFrac), juce::PathStrokeType (2.5f));

  g.restoreState ();

  g.setColour (toColour (theme ().surface, outlineOpacity));
  g.drawEllipse (centre.x - r, centre.y - r, r * 2.f, r * 2.f,
                 theme ().strokeThick);
  g.setColour (iconColour);
  g.drawEllipse (centre.x - r, centre.y - r, r * 2.f, r * 2.f,
                 theme ().strokeThin);

  // The listener, in the middle of the room they are listening to, and the
  // same figure the sphere above and the little ball in its corner carry -- so
  // all three pictures say which way they are facing in the same words.
  //
  // A dot used to stand here and was taken out for saying nothing: a mark that
  // never moves is a thing the eye has to rule out every time it looks. This
  // one moves. It turns with the view, which is the one thing in this circle
  // that says which of the room's two directions you are looking from.
  {
    auto figure
        = listenerSilhouette (elevationSideCamera (_sphereCamera), r * 0.55f);
    figure.applyTransform (
        juce::AffineTransform::translation (centre.x, centre.y));

    g.setColour (toColour (theme ().textPrimary, theme ().alphaMuted));
    g.fillPath (figure);
    g.setColour (toColour (theme ().surface, outlineOpacity));
    g.strokePath (figure, juce::PathStrokeType (1.f));
  }

  // And the sound itself, running along the figure it was drawn from. Last of
  // everything, and outlined, because in a picture this small it is the only
  // mark that moves and it has to be findable at a glance -- the whole reason
  // to look here mid-set is "how high is it right now".
  for (auto const &channel : _elevationChannels)
    {
      if (!channel.headValid)
        continue;

      auto const at = place (channel.head);
      auto const ballR = juce::jmax (2.f, r * 0.11f);

      g.setColour (toColour (theme ().surface, outlineOpacity));
      g.fillEllipse (at.x - ballR - 1.f, at.y - ballR - 1.f,
                     (ballR + 1.f) * 2.f, (ballR + 1.f) * 2.f);
      g.setColour (channel.head.behind
                       ? channel.colour.withAlpha (theme ().alphaInactive)
                       : channel.colour.withMultipliedAlpha (shade (channel)));
      g.fillEllipse (at.x - ballR, at.y - ballR, ballR * 2.f, ballR * 2.f);
    }
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
      g.fillRoundedRectangle (bounds.toFloat (), theme ().radiusControl);
    }

  auto content = bounds.reduced (juce::roundToInt (theme ().paddingTight));
  auto labelArea
      = content.removeFromBottom (textRowHeight (content, metrics.captionSize));

  auto const valueColour = Colours::barText (isSelected);
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
  g.setColour (Colours::barText (isSelected));
  g.drawFittedText (label, labelArea,
                    juce::Justification::centred, 1);
}

}
