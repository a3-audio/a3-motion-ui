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

#pragma once

#include <JuceHeader.h>

#include <optional>

namespace a3
{

/** One finger on the glass and off again: when it came down, when it came
 *  up, where it came down, and whether it moved anything on the way. */
struct TapTouch
{
  juce::int64 downMs = 0;
  juce::int64 upMs = 0;
  juce::Point<int> at;
  bool moved = false;
};

/** Whether a touch that moved something can still be half of a double tap.
 *
 *  Most controls say no: a drag that happened to end near a tap is a drag.
 *  The VU meter says yes -- "has moved müsste egal sein" -- because a finger
 *  tapping twice fast wobbles, and on a control that is also dragged the
 *  wobble is a step, which used to throw the double tap away. */
enum class DoubleTapMovement
{
  MustBeStill,
  MayMove,
};

/** The two taps must come up within this of each other. */
constexpr int doubleTapMs = 400;
/** And come down within this of each other: a finger is not a mouse, so the
 *  second tap lands a few pixels from the first. */
constexpr int doubleTapSlopPx = 24;
/** How long a touch that moved may last and still count as a tap. What keeps
 *  "may move" from turning a drag picked straight back up into a double tap
 *  -- and a channel into full volume in the middle of pulling it down. */
constexpr int shortTouchMs = 400;

/** Whether `touch` can be one of the two taps. */
bool countsAsTap (TapTouch const &touch, DoubleTapMovement movement);

/** Whether `current` completes a double tap begun by `previous`. */
bool isDoubleTap (std::optional<TapTouch> const &previous,
                  TapTouch const &current, DoubleTapMovement movement);

}
