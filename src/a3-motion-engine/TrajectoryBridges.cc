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

/** Normalised coordinates run -1 to 1, so the widest gap there can be is 2. */
constexpr float sphereDiameter = 2.f;

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
             juce::int64 seed)
{
  BridgePlan plan;
  if (ticks.size () < 2)
    return plan;

  auto const reach = juce::jlimit (0.f, 1.f, fadeReach) * sphereDiameter;
  if (reach <= 0.f)
    return plan;

  auto const jumps = trajectoryJumps (ticks);
  auto const starts = runStarts (jumps, ticks.size ());

  for (auto const at : jumps)
    {
      auto const next = (at + 1) % ticks.size ();
      if (distanceBetween (ticks[at], ticks[next]) > reach)
        continue;

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

              std::vector<size_t> reachable;
              for (size_t run = 0; run < starts.size (); ++run)
                {
                  if (run == own)
                    continue;
                  if (distanceBetween (ticks[at], ticks[starts[run]]) <= reach)
                    reachable.push_back (starts[run]);
                }

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

      plan.bridges.push_back (
          { static_cast<index_t> (at), static_cast<index_t> (via) });
    }

  return plan;
}

}
