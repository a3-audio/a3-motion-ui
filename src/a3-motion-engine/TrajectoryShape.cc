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

#include "TrajectoryShape.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{

/** A step this many times the trajectory's usual step is a teleport. */
constexpr float jumpFactor = 8.f;

/** ...and it has to be a real distance as well. Without this floor a pattern
 *  that barely moves would have every one of its steps called a jump, because
 *  eight times almost nothing is still almost nothing. */
constexpr float minJumpDistance = 0.15f;

/** Below this, two ticks are the same held position rather than a movement. */
constexpr float holdDistance = 1e-4f;

/** More held positions than this and it is a drawn trajectory that paused,
 *  not a handful of taps. */
constexpr size_t maxTappedPositions = 16;

float
distance (Pos const &a, Pos const &b)
{
  auto const d = a - b;
  return std::sqrt (d.x () * d.x () + d.y () * d.y () + d.z () * d.z ());
}

/** Every step around the loop, or -1 where either end is missing. */
std::vector<float>
ringSteps (std::vector<Pos> const &ticks)
{
  std::vector<float> steps (ticks.size (), -1.f);
  for (size_t i = 0; i < ticks.size (); ++i)
    {
      auto const &from = ticks[i];
      auto const &to = ticks[(i + 1) % ticks.size ()];
      if (from.isValid () && to.isValid ())
        steps[i] = distance (from, to);
    }
  return steps;
}

/** What this trajectory calls an ordinary step. The median rather than the
 *  mean, so the jumps we are trying to find do not drag the yardstick they
 *  are measured against. */
float
typicalStep (std::vector<float> const &steps)
{
  std::vector<float> present;
  for (auto const step : steps)
    if (step >= 0.f)
      present.push_back (step);

  if (present.empty ())
    return 0.f;

  auto const middle = present.begin () + static_cast<long> (present.size () / 2);
  std::nth_element (present.begin (), middle, present.end ());
  return *middle;
}

float
jumpThreshold (std::vector<float> const &steps)
{
  return trajectoryJumpThreshold (typicalStep (steps));
}

}

float
typicalTrajectoryStep (std::vector<Pos> const &ticks)
{
  if (ticks.size () < 2)
    return 0.f;
  return typicalStep (ringSteps (ticks));
}

float
typicalTrajectorySpeed (std::vector<Pos> const &ticks)
{
  if (ticks.size () < 2)
    return 0.f;

  std::vector<float> moving;
  for (auto const step : ringSteps (ticks))
    if (step >= holdDistance)
      moving.push_back (step);

  if (moving.empty ())
    return 0.f;

  auto const middle = moving.begin () + static_cast<long> (moving.size () / 2);
  std::nth_element (moving.begin (), middle, moving.end ());
  return *middle;
}

float
trajectoryJumpThreshold (float typicalStep)
{
  return std::max (minJumpDistance, jumpFactor * typicalStep);
}

std::vector<size_t>
trajectoryJumps (std::vector<Pos> const &ticks)
{
  std::vector<size_t> jumps;
  if (ticks.size () < 2)
    return jumps;

  auto const steps = ringSteps (ticks);
  auto const threshold = jumpThreshold (steps);

  for (size_t i = 0; i < steps.size (); ++i)
    if (steps[i] > threshold)
      jumps.push_back (i);

  return jumps;
}

bool
isTappedTrajectory (std::vector<Pos> const &ticks)
{
  if (ticks.size () < 2)
    return false;

  auto const steps = ringSteps (ticks);

  int held = 0;
  int moving = 0;

  for (auto const step : steps)
    {
      if (step < 0.f)
        continue;
      if (step < holdDistance)
        ++held;
      else
        ++moving;
    }

  if (held <= moving)
    return false;

  // Standing still most of the time is not enough on its own: a drawn
  // trajectory with a long pause in it does that too, and drawing it as dots
  // would throw away the line the user actually drew. What separates a tap
  // take is that all that standing still happens at a handful of places.
  auto const places = trajectoryPlateaus (ticks).size ();
  return places >= 1 && places <= maxTappedPositions;
}

std::vector<Pos>
trajectoryPlateaus (std::vector<Pos> const &ticks)
{
  std::vector<Pos> plateaus;

  for (auto const &tick : ticks)
    {
      if (!tick.isValid ())
        continue;
      if (plateaus.empty () || distance (plateaus.back (), tick) >= holdDistance)
        plateaus.push_back (tick);
    }

  // The loop's last held position and its first are one and the same when the
  // take was still sitting on its opening tap as it came round.
  if (plateaus.size () > 1
      && distance (plateaus.front (), plateaus.back ()) < holdDistance)
    plateaus.pop_back ();

  return plateaus;
}

std::vector<std::vector<Pos> >
trajectorySegments (std::vector<Pos> const &ticks)
{
  std::vector<std::vector<Pos> > segments;
  if (ticks.empty ())
    return segments;

  auto const steps = ringSteps (ticks);
  auto const threshold = jumpThreshold (steps);

  std::vector<Pos> current;
  for (size_t i = 0; i < ticks.size (); ++i)
    {
      if (!ticks[i].isValid ())
        {
          if (!current.empty ())
            segments.push_back (std::move (current));
          current.clear ();
          continue;
        }

      current.push_back (ticks[i]);

      // Cut after this tick, so the teleport itself is never a drawn edge.
      // The wrapping step is not a cut: it is not drawn either way.
      if (i + 1 < ticks.size () && steps[i] > threshold)
        {
          segments.push_back (std::move (current));
          current.clear ();
        }
    }

  if (!current.empty ())
    segments.push_back (std::move (current));

  return segments;
}

}
