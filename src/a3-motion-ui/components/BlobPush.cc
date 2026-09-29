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

#include "BlobPush.hh"

#include <cmath>
#include <limits>

namespace a3
{

namespace
{
/** One held blob's say over where `drawn` goes: out onto its circle when
 *  inside it, otherwise a step back towards `home` that stays outside it.
 *  The geometry the push always had, from before #56; only what it writes
 *  changed. The projection follows
 *  https://www.geometrictools.com/Documentation/IntersectionLine2Circle2.pdf */
juce::Point<float>
pushedBy (juce::Point<float> held, juce::Point<float> drawn,
          juce::Point<float> home, float radius)
{
  if (drawn.getDistanceFrom (held) < radius)
    {
      auto offset = drawn - held;
      if (offset.getDistanceFromOrigin () <= 0.f)
        offset = { 1.f, 0.f }; // right on top: any way out will do
      offset *= (radius + 1.f) / offset.getDistanceFromOrigin ();
      return held + offset;
    }

  if (drawn.getDistanceFrom (home) <= 1.f)
    return drawn;

  auto const C = held;
  auto const P = drawn;
  auto const D = home - drawn;
  auto const Delta = P - C;
  auto const dDotDelta = D.getDotProduct (Delta);
  auto const discriminant
      = dDotDelta * dDotDelta
        - D.getDistanceSquaredFromOrigin ()
              * (Delta.getDistanceSquaredFromOrigin () - radius * radius);

  auto t = .01f; // no crossing: slide home with exponential smoothing
  int numValid = 0;
  if (discriminant > 0.f)
    {
      auto const root = std::sqrt (discriminant);
      auto const dd = D.getDistanceSquaredFromOrigin ();
      float const candidates[] = { -(dDotDelta - root) / dd,
                                   -(dDotDelta + root) / dd };
      constexpr auto eps = 0.001f;
      auto best = std::numeric_limits<float>::max ();
      for (auto const candidate : candidates)
        {
          if (candidate < -eps || candidate > 1.f + eps)
            continue;
          ++numValid;
          auto const d = P.getDistanceSquaredFrom (P + candidate * D);
          if (d < best)
            {
              best = d;
              t = candidate;
            }
        }
    }

  auto next = P + t * D;
  if (numValid > 0)
    {
      // Nudged sideways, so it slips round the held blob instead of
      // stopping against it.
      auto side = juce::Point<float> (-D.y, D.x);
      side /= side.getDistanceFromOrigin ();
      if ((P - C).getDotProduct (side) < 0.f)
        side *= -1.f;
      next += .25f * side;
    }
  return next;
}
}

juce::Point<float>
nextPushOffset (juce::Point<float> blob, juce::Point<float> offset,
                std::vector<juce::Point<float> > const &held, float radius)
{
  auto drawn = blob + offset;
  for (auto const &h : held)
    drawn = pushedBy (h, drawn, blob, radius);
  return drawn - blob;
}

juce::Point<float>
easedPushOffset (juce::Point<float> offset)
{
  // About half a second home at the sphere's frame rate; exactly nothing
  // below a pixel, so a blob does not crawl for ever.
  auto const eased = offset * .85f;
  return eased.getDistanceFromOrigin () < 1.f ? juce::Point<float>{} : eased;
}

}
