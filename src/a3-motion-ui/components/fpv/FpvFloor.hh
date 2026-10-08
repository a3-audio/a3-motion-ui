/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#pragma once

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/flight/FlightField.hh>
#include <a3-motion-ui/components/AppView.hh>
#include <a3-motion-ui/components/fpv/FpvPagePress.hh>

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <optional>

namespace a3
{

/** The decisions behind FPV's floor that MotionComponent and
 *  A3MotionUIComponent act on: which finger is whose, which body it hit,
 *  whom a ship escorts and what the Page pad did. Pure, so the app's wiring
 *  only carries out what is tested here. */

/** Whether a touch lands on the dance floor: inside the sphere as seen
 *  (`screenRadius`, the sphere's radius = 1) and inside the room on the
 *  floor (`floorPoint`, the room's edge = 1). Off it is the rim or the
 *  background, where the camera's own gestures live. */
bool onTheFloor (float screenRadius, Vec2 floorPoint);

enum class FpvFingerDown
{
  Camera,  // turn, pinch, and two taps reset the view -- as in phase 1
  Floor,   // FloorGesture: place, weigh, drag, remove
  PageTap, // the Page pad is held: this finger names its ship's target
};

/** A second finger is always a pinch. A first finger off the floor is the
 *  camera, so the double tap that resets the view lives there; on the floor
 *  it is the group gesture -- or, while a Page pad is held, that pad's tap,
 *  which never reaches the gesture (a hold on a group would remove it). */
constexpr FpvFingerDown
fpvFingerDown (bool alone, bool onFloor, bool pageHeld)
{
  if (!alone || !onFloor)
    return FpvFingerDown::Camera;
  return pageHeld ? FpvFingerDown::PageTap : FpvFingerDown::Floor;
}

/** A body as it stands on the screen, for the hit test. */
struct BodyOnScreen
{
  int id = noBodyId;
  juce::Point<float> centre;
  float hitRadius = 0.f;
};

/** The body whose hit area holds `at`, the nearest centre when several do;
 *  only the first `count` entries count. */
std::optional<int>
bodyUnderFinger (std::array<BodyOnScreen, maxFlightBodies> const &bodies,
                 int count, juce::Point<float> at);

constexpr int fpvShips = 4;

/** Whom each ship escorts, as the DJ set it: the message thread's record,
 *  mirrored into the engine with setFlightTarget. */
class FpvEscorts
{
public:
  void set (int channel, int bodyId);
  int of (int channel) const; // noBodyId when patrolling or out of range

  /** Ids are reused, so a removed body's escorts go back to patrol before
   *  the next group can take its number. Answers which channels changed. */
  std::array<bool, fpvShips> forget (int bodyId);

private:
  std::array<int, fpvShips> _escort{ noBodyId, noBodyId, noBodyId, noBodyId };
};

/** What the strip and the escort line show for one channel. */
struct EscortView
{
  int escort = noBodyId;
  float mass = 1.f;
};

/** The escort as flown: none for a CLIP ship, for a body that is gone and
 *  for a dead zone (the engine patrols instead of circling one); otherwise
 *  the body with its mass as it is now, so a weight cycle shows at once. */
EscortView escortView (int escortId, bool orbit, FlightBodies const &bodies);

/** The Page pad held in FPV: the press is remembered, a floor tap while it
 *  is held names the target, and the release decides (fpvPagePress). */
class FpvPageHold
{
public:
  /** A new hold; any earlier one is forgotten. */
  void press (int channel);
  bool isHeld () const;
  /** A finger on the floor while held: a body's id, or empty floor. The
   *  last tap counts. Ignored with nothing held. */
  void tap (std::optional<int> bodyId);
  /** A tapped body that went is a tap on empty floor. */
  void bodyRemoved (int bodyId);
  std::optional<int> tappedBody () const;
  /** The outcome when `channel`'s pad is the one held, which ends the hold;
   *  nothing otherwise. */
  std::optional<PageOutcome> release (int channel);
  void clear ();

private:
  std::optional<int> _channel;
  bool _tapped = false;
  std::optional<int> _body;
};

/** Whether a channel's clip lets its ship fly: none loaded, loaded but not
 *  running, or running -- the engine's own question (passIsRunning), so a
 *  clip asked to stop still flies until the stop lands. */
enum class FlightClip
{
  None,
  Stopped,
  Running,
};
FlightClip flightClipOf (Pattern const *clip);

/** An ORBIT toggle whose ship cannot fly says why: "CH1 ORBIT . no clip" or
 *  "CH1 ORBIT . stopped".
 *  "CH1 ORBIT", "CH1 CLIP", "CH1 -> G3", "CH1 PATROL": the channel and what
 *  it does now. `orbitNow` is the mode after a toggle. */
juce::String fpvPageReadout (int channel, PageOutcome outcome, bool orbitNow,
                             int bodyId,
                             FlightClip clip = FlightClip::Running);

/** What an Escort release does to the ship: onto a dead zone the engine
 *  patrols, so that is what is reported. */
PageOutcome flownOutcome (PageOutcome outcome, int bodyId,
                          FlightBodies const &bodies);

/** The readout of a pad press. In FULL a Play|Pause or Page pad of a channel
 *  that flies ORBIT says so ("CH2 ORBIT"); everything else keeps `padName`. */
juce::String padPressReadout (int channel, juce::String const &padName,
                              AppView view, bool orbit, bool playOrPagePad);

}
