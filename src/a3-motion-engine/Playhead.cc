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

float
initialPosition (PlayDirection direction, float randomPhase)
{
  switch (direction)
    {
    case PlayDirection::Reverse: return 1.f;
    case PlayDirection::Random: return wrapIntoPass (randomPhase);
    case PlayDirection::Forward:
    case PlayDirection::Bounce: return 0.f;
    }
  return 0.f;
}

bool
travelsTheWrap (PlayDirection direction, EndAction endAction)
{
  return endAction == EndAction::Loop
         && (direction == PlayDirection::Forward
             || direction == PlayDirection::Reverse);
}

PlaybackMode
playbackModeFromNames (juce::String const &direction,
                       juce::String const &endAction)
{
  // Bounce and Random were end actions until 2026-09-26. A file naming them
  // as the end played them looping, so that is what they become.
  if (endAction == "bounce")
    return { PlayDirection::Bounce, EndAction::Loop };
  if (endAction == "random")
    return { PlayDirection::Random, EndAction::Loop };

  return { playDirectionFromName (direction), endActionFromName (endAction) };
}

juce::String
endActionToName (EndAction action)
{
  switch (action)
    {
    case EndAction::Loop: return "loop";
    case EndAction::Stop: return "stop";
    case EndAction::Pause: return "pause";
    case EndAction::Clip: return "clip";
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
  if (name == "clip")
    return EndAction::Clip;
  // "bounce" and "random" are directions now -- see playbackModeFromNames,
  // which is what a file is read through.
  return EndAction::Loop;
}

namespace
{
/** The travel is over and the end action says what happens: stop at the
 *  take's start, hold where it got to, or -- looping -- whatever `onLoop`
 *  says. `stopAtEnd` is a press asking the clip to finish, and overrules a
 *  loop. */
template <typename OnLoop>
Playhead
endTheTravel (Playhead current, float sign, EndAction endAction,
              bool stopAtEnd, OnLoop onLoop)
{
  // Clip ends the pass the way Stop does: what takes over is the engine's to
  // start (see MotionEngine::performPlayback()), and with nothing to take
  // over a stop is what is left.
  if (stopAtEnd || endAction == EndAction::Stop || endAction == EndAction::Clip)
    // Back to the beginning of the take, whichever way it was running: the
    // next start is a start. Outwards, since that is how every direction
    // but Reverse sets off, and Reverse sets its own sign when it starts.
    return { 0.f, sign, true };

  if (endAction == EndAction::Pause)
    // Where it stood, not where it would have gone: the clip holds here and
    // the channel keeps the position it last had.
    return { current.position, sign, true };

  return onLoop ();
}

/** A bounce reflected at the end it ran into, so it comes away at the rate it
 *  arrived rather than with a stumble.
 *
 *  Held inside the pass, not wrapped into it. A step landing exactly on the
 *  end reflects to the end itself, and wrapping that put the playhead back on
 *  the take's first tick with the sign already turned -- the whole shape
 *  crossed in one tick. Not a corner case either: the delta is one tick's
 *  share of the pass, so a clip playing at its own length arrives on 1.0 dead
 *  on, every pass. */
Playhead
reflected (float next, float sign)
{
  auto const overshoot = sign > 0.f ? next - 1.f : -next;
  auto const back = sign > 0.f ? 1.f - overshoot : overshoot;
  return { juce::jlimit (0.f, 1.f, back), -sign, false };
}
}

Playhead
advancePlayhead (Playhead current, float delta, PlayDirection direction,
                 EndAction endAction, float randomPhase, bool stopAtEnd)
{
  if (current.stopped)
    return current;

  auto const sign = current.sign < 0.f ? -1.f : 1.f;
  auto const next = current.position + delta * sign;

  auto const reachedTheEnd = sign > 0.f ? next >= 1.f : next < 0.f;
  if (!reachedTheEnd)
    return { next, sign, false };

  if (direction == PlayDirection::Bounce)
    {
      // The far end only turns it: its travel is the whole round, so the
      // end action -- and a press asking it to finish -- waits until it is
      // home again.
      if (sign > 0.f)
        return reflected (next, sign);

      return endTheTravel (current, 1.f, endAction, stopAtEnd,
                           [next, sign] { return reflected (next, sign); });
    }

  if (direction == PlayDirection::Random)
    return endTheTravel (current, sign, endAction, stopAtEnd,
                         [randomPhase, sign] {
                           return Playhead{ wrapIntoPass (randomPhase), sign,
                                            false };
                         });

  return endTheTravel (current, sign, endAction, stopAtEnd, [next, sign] {
    return Playhead{ wrapIntoPass (next), sign, false };
  });
}

double
fractionalTickForPlayback (float position, index_t numTicks,
                           PlayDirection direction)
{
  if (numTicks <= 1)
    return 0.;

  auto const span = direction == PlayDirection::Bounce
                        ? static_cast<double> (numTicks - 1)
                        : static_cast<double> (numTicks);

  return span * static_cast<double> (position);
}

juce::String
playDirectionToName (PlayDirection direction)
{
  switch (direction)
    {
    case PlayDirection::Forward: return "fwd";
    case PlayDirection::Reverse: return "rev";
    case PlayDirection::Bounce: return "bounce";
    case PlayDirection::Random: return "random";
    }
  return "fwd";
}

PlayDirection
playDirectionFromName (juce::String const &name)
{
  if (name == "rev")
    return PlayDirection::Reverse;
  if (name == "bounce")
    return PlayDirection::Bounce;
  if (name == "random")
    return PlayDirection::Random;
  return PlayDirection::Forward;
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

std::optional<Playhead>
rewoundPlayhead (Playhead current, index_t ticks, float delta,
                 PlayDirection direction, EndAction endAction)
{
  if (ticks == 0)
    return current;

  auto const loops = endAction == EndAction::Loop;
  auto const back = static_cast<float> (ticks) * delta;

  if (direction == PlayDirection::Bounce)
    {
      // Unfolded into one round: out over [0, 1), back over [1, 2).
      auto const round = current.sign > 0.f ? current.position
                                            : 2.f - current.position;
      auto earlier = round - back;
      if (earlier < 0.f && !loops)
        return std::nullopt;
      earlier = std::fmod (earlier, 2.f);
      if (earlier < 0.f)
        earlier += 2.f;
      if (earlier < 1.f)
        return Playhead{ earlier, 1.f, false };
      return Playhead{ 2.f - earlier, -1.f, false };
    }

  auto const sign = current.sign < 0.f ? -1.f : 1.f;
  auto const earlier = current.position - sign * back;
  if (earlier >= 0.f && earlier <= 1.f)
    return Playhead{ earlier, sign, false };
  if (!loops)
    return std::nullopt;
  return Playhead{ wrapIntoPass (earlier), sign, false };
}

}
