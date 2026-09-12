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

#include "TrajectoryBridges.hh"

#include <a3-motion-engine/util/SeedSpread.hh>

#include <a3-motion-engine/TrajectoryShape.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
float
distanceBetween (Pos const &a, Pos const &b)
{
  return std::sqrt (std::pow (b.x () - a.x (), 2.f)
                    + std::pow (b.y () - a.y (), 2.f)
                    + std::pow (b.z () - a.z (), 2.f));
}

/** Where a bridge may land: the first tick of every run, which is where the
 *  movement can pick up coherently. Landing in the middle of a run would drop
 *  the blob into a stretch it is about to leave. The run the gap starts in is
 *  left out -- heading back into where you came from is a reversal, not a way
 *  out. */
std::vector<size_t>
runStarts (std::vector<size_t> const &jumps, size_t numTicks)
{
  std::vector<size_t> starts{ 0 };
  for (auto const jump : jumps)
    if (jump + 1 < numTicks)
      starts.push_back (jump + 1);
  return starts;
}

/** Which run a tick belongs to, as an index into runStarts(). */
size_t
runOf (std::vector<size_t> const &starts, size_t tick)
{
  size_t run = 0;
  for (size_t i = 0; i < starts.size (); ++i)
    if (starts[i] <= tick)
      run = i;
  return run;
}

/** How many ticks the run containing `tick` has before it, and after it. The
 *  crossing may only eat into these -- never past the end of a run, or it
 *  would swallow a whole tap. */
size_t
ticksBefore (std::vector<size_t> const &starts, size_t tick)
{
  return tick - starts[runOf (starts, tick)];
}

size_t
ticksAfter (std::vector<size_t> const &starts, size_t tick, size_t numTicks)
{
  auto const run = runOf (starts, tick);
  auto const end = run + 1 < starts.size () ? starts[run + 1] : numTicks;
  return end - 1 - tick;
}
}

Bridge const *
BridgePlan::crossingAt (index_t tick, index_t numTicks) const
{
  if (numTicks == 0)
    return nullptr;

  for (auto const &bridge : bridges)
    {
      // Counted forward from where the blob left, so a crossing that runs off
      // the end of the take and back round to its start is one window, not
      // two.
      auto const since = (tick + numTicks - bridge.leaveTick) % numTicks;
      if (since < bridge.windowTicks)
        return &bridge;
    }

  return nullptr;
}

bool
BridgePlan::skipsTick (index_t tick, index_t numTicks) const
{
  auto const *crossing = crossingAt (tick, numTicks);
  if (crossing == nullptr || numTicks == 0)
    return false;

  auto const since = (tick + numTicks - crossing->leaveTick) % numTicks;

  return since != 0                              // where it leaves
         && since != crossing->windowTicks - 1u  // where it rejoins
         && tick != crossing->viaTick;           // and where it lands
}

bool
BridgePlan::bridged (index_t tick) const
{
  return via (tick).has_value ();
}

std::optional<index_t>
BridgePlan::via (index_t tick) const
{
  auto const found
      = std::find_if (bridges.begin (), bridges.end (),
                      [tick] (Bridge const &b) { return b.fromTick == tick; });

  if (found == bridges.end ())
    return std::nullopt;

  return found->viaTick;
}

juce::int64
seedForTicks (std::vector<Pos> const &ticks)
{
  // Cheap and stable rather than cryptographic: it only has to be the same
  // number for the same movement, on any machine.
  juce::int64 hash = 1469598103934665603LL;
  for (auto const &tick : ticks)
    {
      auto mix = [&hash] (float value) {
        hash ^= static_cast<juce::int64> (std::lround (value * 100000.f));
        hash *= 1099511628211LL;
      };
      mix (tick.x ());
      mix (tick.y ());
      mix (tick.z ());
    }
  return hash;
}

BridgePlan
planBridges (std::vector<Pos> const &ticks, float fadeReach, int bridgeBias,
             juce::int64 seed, bool joinsTheWrap)
{
  BridgePlan plan;
  if (ticks.size () < 2)
    return plan;

  // How much of the two ends a join may take over. This is the whole of what
  // the fade says now.
  //
  // It used to say which gaps were closed instead -- a gap wider than
  // `fade * the sphere's diameter` stayed a jump -- and that made most of the
  // pot's travel do nothing at all and then switch. A fade should fill in:
  // turned up, the joins grow and take more of the trajectory's own length as
  // they go, and how wide a gap happens to be is not the performer's problem.
  //
  // Half at the top rather than all, so a tap is still a tap: the blob has to
  // stand somewhere before it sets off.
  auto const share = juce::jlimit (0.f, 1.f, fadeReach) * 0.5f;
  if (share <= 0.f)
    return plan;

  auto const jumps = trajectoryJumps (ticks);
  auto const starts = runStarts (jumps, ticks.size ());

  for (auto const at : jumps)
    {
      // The step from the last tick to the first is a gap only when the clip
      // loops. A clip that bounces or stops never travels it, and joining it
      // spent the last run's ticks gliding towards a start the blob does not
      // reach -- a bouncing clip was seen setting off for its beginning and
      // coming back.
      auto const wraps = at + 1 >= ticks.size ();
      if (wraps && !joinsTheWrap)
        continue;

      auto const next = wraps ? size_t{ 0 } : at + 1;
      auto via = next;

      if (bridgeBias != 0)
        {
          // One fixed draw per gap, so turning the pot up adds departures
          // instead of redealing the ones already there. A pot that reshuffled
          // on every degree could not be dialled in.
          // Spread, not merely offset: juce::Random's first draw off two
          // neighbouring seeds is the same draw, so `seed + at` decided every
          // gap the same way and the pot was a switch wearing a knob's
          // clothes. See spreadSeed().
          juce::Random perGap (
              spreadSeed (seed + static_cast<juce::int64> (at)));
          auto const strayAt = std::abs (bridgeBias) / 4.f;

          if (perGap.nextFloat () < strayAt)
            {
              auto const own = runOf (starts, at);

              // Every other run is a candidate. There is no distance limit
              // any more: how far a join reaches is the bias's business --
              // "the nearest" or "any of them" -- and the fade's business is
              // how long it takes. One knob, one job.
              std::vector<size_t> reachable;
              for (size_t run = 0; run < starts.size (); ++run)
                if (run != own)
                  reachable.push_back (starts[run]);

              if (!reachable.empty ())
                {
                  if (bridgeBias < 0)
                    via = *std::min_element (
                        reachable.begin (), reachable.end (),
                        [&ticks, at] (size_t a, size_t b) {
                          return distanceBetween (ticks[at], ticks[a])
                                 < distanceBetween (ticks[at], ticks[b]);
                        });
                  else
                    via = reachable[static_cast<size_t> (perGap.nextInt (
                        static_cast<int> (reachable.size ())))];
                }
            }
        }

      // Rounded down, and never more than half of an end: two joins on the
      // same run take from its two ends, and they have to meet at worst --
      // overlapping windows are two crossings claiming the same blob.
      //
      // That rule counts two crossings sharing a run. It says nothing about
      // *one* crossing whose own two ends are the same run, which is exactly
      // what the seam of a continuously recorded take is -- and there half
      // from each end is the whole of it. Measured at a fade reach of 1 on a
      // 48-tick take: 23 + 1 + 23, forty-seven ticks inside one crossing,
      // the recorded figure replaced by two straight lines and the blob
      // arriving at the landing point travelling almost exactly backwards.
      // That was the loop point hopping the wrong way.
      //
      // So when both ends are the same run, the two of them share that run's
      // half rather than taking one each. The crossing still gets a window --
      // shrinking it to nothing would bring back the jump this exists to
      // stop -- and a take made of separate taps, which is what bridges were
      // built for, is untouched.
      auto const endsShareARun
          = runOf (starts, at) == runOf (starts, via);
      auto const endShare = endsShareARun ? share * 0.5f : share;

      auto const before = static_cast<size_t> (std::floor (
          static_cast<float> (ticksBefore (starts, at)) * endShare));
      auto const after = static_cast<size_t> (std::floor (
          static_cast<float> (ticksAfter (starts, via, ticks.size ()))
          * endShare));

      // Nothing reserved is nothing joined: a crossing squeezed into the
      // gap's own single tick is the jump this was built to stop being.
      if (before + after == 0)
        continue;

      plan.bridges.push_back (
          { static_cast<index_t> (at), static_cast<index_t> (via),
            static_cast<index_t> (at - before),
            static_cast<index_t> (before + 1 + after) });
    }

  return plan;
}

}
