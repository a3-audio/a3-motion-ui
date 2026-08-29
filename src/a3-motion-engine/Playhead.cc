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

#include "Playhead.hh"

#include <cmath>

namespace a3
{

namespace
{
/** `value` folded into [0, 1) — a step longer than a whole pass must not throw
 *  the playhead outside the loop, and a fast clip can cover several passes in
 *  one tick. */
float
wrapIntoPass (float value)
{
  auto folded = std::fmod (value, 1.f);
  if (folded < 0.f)
    folded += 1.f;
  return folded;
}
}

float
initialSign (PlayDirection direction)
{
  return direction == PlayDirection::Reverse ? -1.f : 1.f;
}

juce::String
endActionToName (EndAction action)
{
  switch (action)
    {
    case EndAction::Loop: return "loop";
    case EndAction::Stop: return "stop";
    case EndAction::Bounce: return "bounce";
    case EndAction::Random: return "random";
    }
  return "loop";
}

EndAction
endActionFromName (juce::String const &name)
{
  if (name == "stop")
    return EndAction::Stop;
  if (name == "bounce")
    return EndAction::Bounce;
  if (name == "random")
    return EndAction::Random;
  return EndAction::Loop;
}

Playhead
advancePlayhead (Playhead current, float delta, EndAction endAction,
                 float randomPhase)
{
  if (current.stopped)
    return current;

  auto const sign = current.sign < 0.f ? -1.f : 1.f;
  auto const next = current.position + delta * sign;

  auto const reachedTheEnd = sign > 0.f ? next >= 1.f : next < 0.f;
  if (!reachedTheEnd)
    return { next, sign, false };

  switch (endAction)
    {
    case EndAction::Loop:
      return { wrapIntoPass (next), sign, false };

    case EndAction::Stop:
      // Where it stood, not where it would have gone: the clip stops here and
      // the channel keeps the position it last had.
      return { current.position, sign, true };

    case EndAction::Bounce:
      {
        // Reflected at the end it ran into, so it comes away at the rate it
        // arrived rather than with a stumble.
        auto const overshoot = sign > 0.f ? next - 1.f : -next;
        return { wrapIntoPass (sign > 0.f ? 1.f - overshoot : overshoot),
                 -sign, false };
      }

    case EndAction::Random:
      return { wrapIntoPass (randomPhase), sign, false };
    }

  return { wrapIntoPass (next), sign, false };
}

}
