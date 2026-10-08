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

#include <optional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace a3
{

/** What one finger on the floor turned out to be. */
enum class FloorAction
{
  None,
  Camera,      // turn the view, as in phase 1
  Place,       // a new group where the finger was
  Drag,        // move the body under the finger
  CycleWeight, // the next weight for the body under the finger
  Remove,      // the body under the finger goes
};

/** One finger on the floor, decided as it goes: still and short is a tap,
 *  past the slop it is a drag (of the body it started on) or the camera,
 *  still and long on a body removes it.
 *
 *  Remove is the only gesture that destroys something, so it is the only slow
 *  one: holdProgress () fills a ring towards it, and lifting before it is full
 *  aborts. A long press on empty floor does nothing, so a hesitant finger
 *  never places a group. Times are in milliseconds from any one clock. */
class FloorGesture
{
public:
  /** `body` = the id of the body under the finger, if any (hit-tested by
   *  the caller). */
  void down (juce::Point<float> at, double ms, std::optional<int> body);
  /** Camera or Drag once past the slop, from then on until the lift. */
  FloorAction move (juce::Point<float> at, double ms);
  /** Place, CycleWeight or Remove for a touch still undecided; None else. */
  FloorAction up (juce::Point<float> at, double ms);
  /** Remove, once, when a body is held still past longPressMs. */
  FloorAction held (double ms);
  /** A second finger took over (a pinch): the touch decides nothing more. */
  void cancel ();

  std::optional<int> body () const;
  /** 0..1 towards the removal while a body is held still; 0 otherwise. */
  float holdProgress (double ms) const;

  static constexpr double longPressMs = 600.;
  /** Movement that is still a tap, as a fraction of the blob diameter. */
  static constexpr float slopOfBlob = 0.5f;
  void setBlobDiameter (float pixels);

private:
  enum class State
  {
    Idle,
    Undecided,
    Camera,
    Drag,
    Finished, // removed or cancelled: the lift does nothing more
  };

  bool heldLongEnough (double ms) const;
  bool pastTheSlop (juce::Point<float> at) const;

  State _state = State::Idle;
  juce::Point<float> _start;
  double _downMs = 0.;
  std::optional<int> _body;
  float _blobDiameter = 0.f;
};

}
