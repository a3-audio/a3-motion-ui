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
    case EndAction::Pause: return "pause";
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
  if (name == "pause")
    return EndAction::Pause;
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
      // Back to the beginning of the take, whichever way it was running: the
      // next start is a start. "Back to the start" is about the take, not
      // about the direction it happened to be going.
      return { 0.f, sign, true };

    case EndAction::Pause:
      // Where it stood, not where it would have gone: the clip holds here and
      // the channel keeps the position it last had.
      return { current.position, sign, true };

    case EndAction::Bounce:
      {
        // Reflected at the end it ran into, so it comes away at the rate it
        // arrived rather than with a stumble.
        auto const overshoot = sign > 0.f ? next - 1.f : -next;
        auto const reflected = sign > 0.f ? 1.f - overshoot : overshoot;

        // Held inside the pass, not wrapped into it. A step landing exactly on
        // the end reflects to the end itself, and wrapping that put the
        // playhead back on the take's first tick with the sign already turned
        // -- the whole shape crossed in one tick. Not a corner case either:
        // the delta is one tick's share of the pass, so a clip playing at its
        // own length arrives on 1.0 dead on, every pass.
        return { juce::jlimit (0.f, 1.f, reflected), -sign, false };
      }

    case EndAction::Random:
      return { wrapIntoPass (randomPhase), sign, false };
    }

  return { wrapIntoPass (next), sign, false };
}

double
fractionalTickForPlayback (float position, index_t numTicks,
                           EndAction endAction)
{
  if (numTicks <= 1)
    return 0.;

  auto const span = endAction == EndAction::Bounce
                        ? static_cast<double> (numTicks - 1)
                        : static_cast<double> (numTicks);

  return span * static_cast<double> (position);
}

juce::String
playDirectionToName (PlayDirection direction)
{
  return direction == PlayDirection::Reverse ? "rev" : "fwd";
}

PlayDirection
playDirectionFromName (juce::String const &name)
{
  return name == "rev" ? PlayDirection::Reverse : PlayDirection::Forward;
}

juce::String
actModeToName (ActMode mode)
{
  return mode == ActMode::Hold ? "hold" : "one-shot";
}

ActMode
actModeFromName (juce::String const &name)
{
  // Anything unrecognised is a shot, which is what every take written before
  // the mode existed did.
  return name == "hold" ? ActMode::Hold : ActMode::OneShot;
}

}
