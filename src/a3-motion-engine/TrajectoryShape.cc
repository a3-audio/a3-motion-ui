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
#include <cstdlib>

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

/** How far either side of a step its neighbours are looked at, in ticks.
 *
 *  A step is measured against the movement around it rather than against the
 *  whole take. The touch panel reports more slowly than the clock ticks, so a
 *  drawn stroke holds each position for a few ticks and most of its steps are
 *  zero: measured against the median of the whole take -- zero -- every report
 *  of a fast stroke counted as a teleport, the file cut the line there, and
 *  the take came back full of holes (2026-09-25). Eight ticks either side is
 *  several reports at any tempo the clock runs at. */
constexpr long paceWindow = 8;

/** More held positions than this and it is a drawn trajectory that paused,
 *  not a handful of taps. */
constexpr size_t maxTappedPositions = 16;

/** How long the hand has to stay put for a position to count as a place it
 *  rested at, as a fraction of the take.
 *
 *  Without it a "plateau" was any position differing from the last one, which
 *  for a drawn line is every single tick -- so a take that was still being
 *  played in, with only a handful of ticks written and a motionless remainder,
 *  came out as a handful of places and was read as a tap take. A take with
 *  sixteen taps in it -- the most that still counts as one -- rests a
 *  sixteenth of the ring at each; a sixty-fourth is well under that and well
 *  over anything a moving hand passes through. */
constexpr float plateauMinFraction = 1.f / 64.f;
constexpr size_t plateauMinTicks = 2;

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

/** Each step's own threshold: eight times the pace of the hand around it,
 *  and never less than the floor. Around a tap the hand is standing still, so
 *  the floor decides and the tap stays a jump; around a fast stroke the hand
 *  moves just as fast, so the stroke stays a stroke. */
std::vector<float>
localJumpThresholds (std::vector<float> const &steps)
{
  auto const n = static_cast<long> (steps.size ());
  std::vector<float> thresholds (steps.size (), minJumpDistance);

  std::vector<float> around;
  for (long i = 0; i < n; ++i)
    {
      around.clear ();
      for (long d = -paceWindow; d <= paceWindow; ++d)
        {
          if (d == 0 || std::abs (d) >= n)
            continue;
          auto const step = steps[static_cast<size_t> (((i + d) % n + n) % n)];
          if (step >= holdDistance)
            around.push_back (step);
        }

      if (around.empty ())
        continue;

      auto const middle = around.begin () + static_cast<long> (around.size () / 2);
      std::nth_element (around.begin (), middle, around.end ());
      thresholds[static_cast<size_t> (i)] = trajectoryJumpThreshold (*middle);
    }

  return thresholds;
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

std::vector<float>
trajectoryJumpThresholds (std::vector<Pos> const &ticks)
{
  if (ticks.size () < 2)
    return {};
  return localJumpThresholds (ringSteps (ticks));
}

std::vector<size_t>
trajectoryJumps (std::vector<Pos> const &ticks)
{
  std::vector<size_t> jumps;
  if (ticks.size () < 2)
    return jumps;

  auto const steps = ringSteps (ticks);
  auto const thresholds = localJumpThresholds (steps);

  for (size_t i = 0; i < steps.size (); ++i)
    if (steps[i] > thresholds[i])
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
  // trajectory with a long pause in it does that too, and so does a take that
  // is still being played in, whose unwritten remainder does not move because
  // it does not exist yet. Drawing either as dots throws away the line the
  // user actually drew.
  //
  // What separates a tap take is not how much of it is still but how it got
  // from one place to the next: a tap teleports, a hand travels. So compare
  // the ground covered rather than counting ticks -- a take that spent most of
  // its distance jumping was tapped, and one that spent it moving was drawn,
  // whatever fraction of the ring either of them sat out.
  auto const thresholds = localJumpThresholds (steps);
  float jumped = 0.f;
  float travelled = 0.f;
  for (size_t i = 0; i < steps.size (); ++i)
    {
      auto const step = steps[i];
      if (step < holdDistance)
        continue;
      (step > thresholds[i] ? jumped : travelled) += step;
    }

  // A take that never went anywhere is a held position, and a held position is
  // a dot. There is no line to throw away, so the question above does not
  // arise -- and asking it anyway said "drawn" and drew nothing.
  if (jumped == 0.f && travelled == 0.f)
    return true;

  if (jumped <= travelled)
    return false;

  // And it rests at a handful of places rather than a great many.
  auto const places = trajectoryPlateaus (ticks).size ();
  return places >= 1 && places <= maxTappedPositions;
}

std::vector<Pos>
trajectoryPlateaus (std::vector<Pos> const &ticks)
{
  std::vector<Pos> plateaus;

  auto const minRun = std::max (
      plateauMinTicks,
      static_cast<size_t> (static_cast<float> (ticks.size ())
                           * plateauMinFraction));

  // A run of consecutive ticks that never leaves the spot it started on. Only
  // a run long enough to be a rest is a place; the rest of the ticks are the
  // hand on its way somewhere.
  size_t runStart = 0;
  bool inRun = false;

  auto const closeRun = [&] (size_t runEnd) {
    if (!inRun || runEnd - runStart < minRun)
      return;
    plateaus.push_back (ticks[runStart]);
  };

  for (size_t i = 0; i < ticks.size (); ++i)
    {
      if (!ticks[i].isValid ())
        {
          closeRun (i);
          inRun = false;
          continue;
        }

      if (!inRun)
        {
          runStart = i;
          inRun = true;
          continue;
        }

      if (distance (ticks[runStart], ticks[i]) >= holdDistance)
        {
          closeRun (i);
          runStart = i;
        }
    }
  closeRun (ticks.size ());

  // The loop's last held position and its first are one and the same when the
  // take was still sitting on its opening tap as it came round.
  if (plateaus.size () > 1
      && distance (plateaus.front (), plateaus.back ()) < holdDistance)
    plateaus.pop_back ();

  return plateaus;
}

std::vector<Pos>
trailPoints (std::vector<Pos> const &run, size_t maxPoints)
{
  std::vector<Pos> moved;
  moved.reserve (run.size ());
  for (auto const &pos : run)
    if (moved.empty () || distance (moved.back (), pos) >= holdDistance)
      moved.push_back (pos);

  if (moved.size () <= maxPoints || maxPoints < 2)
    return moved;

  std::vector<Pos> drawn;
  drawn.reserve (maxPoints);
  for (size_t k = 0; k < maxPoints; ++k)
    drawn.push_back (moved[k * (moved.size () - 1) / (maxPoints - 1)]);
  return drawn;
}

std::vector<std::vector<Pos> >
trajectorySegments (std::vector<Pos> const &ticks, BridgePlan const &plan)
{
  std::vector<std::vector<Pos> > segments;
  if (ticks.empty ())
    return segments;

  auto const steps = ringSteps (ticks);
  auto const thresholds = localJumpThresholds (steps);

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

      // A tick the blob never stands on is not on the line either: during a
      // crossing it is somewhere between the two ends, and the ends it gave up
      // are not visited. Asked of the same plan the movement reads, so the two
      // cannot drift apart.
      if (plan.skipsTick (static_cast<index_t> (i),
                          static_cast<index_t> (ticks.size ())))
        continue;

      current.push_back (ticks[i]);

      // Cut after this tick, so the teleport itself is never a drawn edge.
      // The wrapping step is not a cut: it is not drawn either way.
      //
      // A gap the fade draws through is a line, not a break -- and it is the
      // same plan the movement is played from, so the line cannot be cut where
      // the blob runs on.
      if (i + 1 < ticks.size () && steps[i] > thresholds[i]
          && !plan.bridged (static_cast<index_t> (i)))
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
