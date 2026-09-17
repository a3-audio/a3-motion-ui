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

#include <cmath>
#include <cstddef>
#include <vector>

namespace a3
{

/** Strands wound helically around a line, and the lightning that leaves them.
 *
 *  **Used for the trajectory's braid.** It was built for a braid of three
 *  hairlines inside a coil of bolts, drawn as strands offset along the line's
 *  normal, and that whole approach was taken out again: an offset copy of a
 *  curve folds where its curvature times the offset passes one, so at the
 *  pole -- where every azimuth of the figure meets at one point -- it fanned
 *  out into straight spokes across the middle of the sphere. The glow is a
 *  field in the fragment shader now, and a level set of a field cannot do
 *  that.
 *
 *  The helix itself was never the problem, and foldGuard() below is what
 *  makes it safe: it shuts the offset off exactly where a parallel curve
 *  would fold, which is what drew the spokes.
 *
 *
 *  One unit for two things that look nothing alike and are the same
 *  arithmetic: the trajectory's core is a *braid* -- three hairlines at a
 *  small radius with many turns -- and around it stands a *plasma sheath*,
 *  strands at a large radius with few turns. Two parameter sets, one helix.
 *  Written twice they would eventually disagree about which way round a coil
 *  goes, and that is exactly the kind of difference nobody sees until the two
 *  are next to each other on a screen.
 *
 *  Nothing here draws, opens a window, reads a clock or rolls a die: the
 *  caller hands in where along the line it is and what time it is, and gets a
 *  number back. That is what lets the look be tuned against a test rather
 *  than by rebuilding and squinting.
 *
 *  Coordinates: `u` runs 0..1 along the whole figure, `offset` comes back in
 *  the same units as `radius` and is measured across the line, and `depth`
 *  is -1 behind the line to +1 in front of it. What "across" means -- which
 *  way the normal points -- is the caller's business, because only the
 *  caller knows how the line was projected. */

/** One ring of strands around the line. */
struct SheathRing
{
  /** How far the strands stand off the axis. Zero is off, exactly. */
  float radius = 0.f;
  /** Turns over the whole figure. Many for a braid, few for a sheath. */
  float turns = 0.f;
  /** Turns per second, signed -- this is the ring rotating *lengthwise*
   *  around the line, which is what makes it read as a coil being driven
   *  rather than as a fixed piece of wire. */
  float spin = 0.f;
  int strands = 3;
};

/** Where one strand is at one point of the line. */
struct SheathSample
{
  /** Across the line, in the ring's own units. */
  float offset = 0.f;
  /** -1 behind the line, +1 in front of it. Decides which layer the piece is
   *  drawn in, and that ordering is what makes the coil pass behind the wire
   *  and come out the other side. */
  float depth = 0.f;
};

/** Two pi, once. */
constexpr float sheathTwoPi = 6.28318530718f;

inline SheathSample
sheathAt (float u, int strand, SheathRing const &ring, float seconds)
{
  auto const strands = ring.strands > 0 ? ring.strands : 1;
  auto const phase
      = sheathTwoPi
        * (u * ring.turns + seconds * ring.spin
           + static_cast<float> (strand) / static_cast<float> (strands));

  // Sine across and cosine along: in quadrature, which is the difference
  // between a helix and a zigzag. Where a strand stands furthest out it is
  // exactly side-on; where it crosses the axis it is furthest in front or
  // behind.
  return { ring.radius * std::sin (phase), std::cos (phase) };
}

/** How often, how many and how long the lightning is. */
struct ArcSettings
{
  /** Strikes a second per strand. Zero is off. */
  float rate = 0.f;
  /** How many of those slots actually carry one, 0..1. Zero is off. */
  float density = 0.f;
  /** How long a strike is, as a share of the whole figure. */
  float width = 0.f;
};

/** A hash with no state and no library behind it.
 *
 *  Three integers in, one number in 0..1 out. Deterministic on purpose: a
 *  strike that came from a random number generator could not be tested, and
 *  could not be the same on two devices drawing the same set. */
inline float
sheathHash (int a, int b, int c)
{
  auto h = static_cast<unsigned> (a) * 374761393u
           + static_cast<unsigned> (b) * 668265263u
           + static_cast<unsigned> (c) * 2246822519u;
  h ^= h >> 13;
  h *= 1274126177u;
  h ^= h >> 16;
  return static_cast<float> (h & 0xffffffu) / static_cast<float> (0x1000000u);
}

/** How much of an offset a curve will carry here, 0..1.
 *
 *  An offset copy of a curve is a *parallel curve*, and a parallel curve folds
 *  where the curvature times the offset passes one: past that the outside of
 *  the bend has overtaken itself and the copy crosses through its own centre.
 *
 *  On this sphere that is not a rare corner case, it is the middle of the
 *  picture. Where a figure runs through the pole -- the Rose and the Heart
 *  both do -- every azimuth meets at a single point, so the projected points
 *  crowd together and the tangent swings through half a turn in almost no
 *  distance. The curvature there is effectively unbounded, and a braid built
 *  without this fanned out into straight grey spokes across the centre of the
 *  sphere. The maintainer saw them: *"ich verstehe die random geraden linien
 *  zur mitte der sphäre nicht. die sollen weg."*
 *
 *  So the strands are pulled back onto the axis as the fold is approached, and
 *  are on it before it arrives. A braid that closes to a single thread through
 *  a cusp is what a braid pulled tight does; a braid that fans is a fault.
 *
 *  `curvature` is in the reciprocal of whatever units `radius` is in -- turn
 *  per unit length against the offset -- so the product is dimensionless and
 *  the thresholds below mean the same thing at any scale. */
inline float
foldGuard (float curvature, float radius)
{
  if (!(radius > 0.f) || !(curvature > 0.f))
    return 1.f;

  auto const risk = curvature * radius;
  if (risk <= 0.55f)
    return 1.f;
  if (risk >= 1.f)
    return 0.f;

  // Smooth, because a strand that switched off would read as the braid
  // snapping rather than as it being drawn tight.
  auto const t = (risk - 0.55f) / 0.45f;
  auto const eased = t * t * (3.f - 2.f * t);
  return 1.f - eased;
}

/** How the sheath kinks and flickers.
 *
 *  A smooth helix is a spring, and no amount of adding more smooth helices to
 *  it changes that -- they sum to a smooth curve. A bolt is not smooth: it
 *  holds a direction for a stretch and then jumps, and it is alight here and
 *  dark there. Both are steps, so both come from a held value rather than
 *  from a wave. */
struct FlickerSettings
{
  /** How finely the length is cut. A bolt's straight stretch is one bucket. */
  float buckets = 0.f;
  /** How often the whole thing is re-drawn, per second. */
  float rate = 0.f;
};

/** A value held across one bucket of the line and re-rolled on a clock, -1..1.
 *
 *  Held, not interpolated: the corner *is* the effect. */
inline float
flickerAt (float u, int strand, FlickerSettings const &flicker, float seconds)
{
  if (flicker.buckets <= 0.f)
    return 0.f;

  auto const bucket = static_cast<int> (std::floor (u * flicker.buckets));
  auto const slot = flicker.rate > 0.f
                        ? static_cast<int> (std::floor (seconds * flicker.rate))
                        : 0;

  return sheathHash (strand * 7919 + bucket, slot, 11) * 2.f - 1.f;
}

/** Whether this strand is alight at this point at all, 0 or 1.
 *
 *  `density` is how much of the length carries a bolt: at 1 the sheath is a
 *  continuous coil, at 0 it is dark, and in between it is what the maintainer
 *  asked for -- an envelope *made of* bolts rather than a wire bent into a
 *  spiral. */
inline float
aliveAt (float u, int strand, FlickerSettings const &flicker, float seconds,
         float density)
{
  if (density >= 1.f)
    return 1.f;
  if (density <= 0.f || flicker.buckets <= 0.f)
    return 0.f;

  auto const bucket = static_cast<int> (std::floor (u * flicker.buckets));
  auto const slot = flicker.rate > 0.f
                        ? static_cast<int> (std::floor (seconds * flicker.rate))
                        : 0;

  return sheathHash (strand * 6271 + bucket, slot, 23) < density ? 1.f : 0.f;
}

/** How hard this strand is striking at this point of the line, 0..1.
 *
 *  Time is cut into slots, and a slot holds *at most one* strike per strand,
 *  anchored somewhere along the figure. Two strikes standing next to each
 *  other in one slot read as the line shaking rather than as something
 *  striking it, which is why the anchor is one number and not a field.
 *
 *  Within its slot the strike rises and falls to nothing, so it is an event
 *  rather than a stripe that happens to be lit. */
inline float
arcAt (float u, int strand, ArcSettings const &arcs, float seconds)
{
  if (arcs.rate <= 0.f || arcs.density <= 0.f || arcs.width <= 0.f)
    return 0.f;

  auto const slotF = seconds * arcs.rate;
  auto const slot = static_cast<int> (std::floor (slotF));
  auto const withinSlot = slotF - std::floor (slotF);

  if (sheathHash (strand, slot, 1) > arcs.density)
    return 0.f;

  // Where it stands. The strand is part of the hash, so three strands strike
  // in three places rather than in one fat one.
  auto const anchor = sheathHash (strand, slot, 2);

  auto const along = std::abs (u - anchor) / arcs.width;
  if (along >= 1.f)
    return 0.f;

  // Along the line: full at the anchor, gone at the ends.
  auto const reach = 1.f - along;
  // In time: nothing at either edge of the slot, most in the middle.
  auto const flash = std::sin (withinSlot * 3.14159265f);

  return reach * reach * flash * flash;
}


/** One point of a line as it is drawn, and whether a new stroke starts at it. */
struct SheathPoint
{
  float x = 0.f;
  float y = 0.f;
  bool startsRun = false;
};

/** foldGuard() at every point of a line, with the bend measured over a
 *  stretch as long as the braid is wide.
 *
 *  Measured point by point it was measured on the wrong thing. A figure
 *  arrives flattened into straight pieces, so the turn between two neighbours
 *  is nothing along a piece and all of it at the corner -- and a corner of a
 *  tenth of a radian over one step of four thousandths reads as a bend far
 *  tighter than the braid. The strands were pulled onto the axis at every
 *  corner and let go one point later: a strut across the cord every twenty
 *  points, and the plasma read as DNA. *"das sieht aus wie dna soll aber ja
 *  plasma sein."*
 *
 *  What folds an offset copy is a bend tighter than the offset, and that is a
 *  question about a stretch of about the offset's own length. Over that
 *  stretch a flattening corner is a gentle bend and a real hairpin is still a
 *  hairpin.
 *
 *  Never across a pen lift: the neighbour there belongs to another stroke. */
inline std::vector<float>
foldGuardsAlong (std::vector<SheathPoint> const &points, float radius)
{
  std::vector<float> guards (points.size (), 1.f);
  if (!(radius > 0.f))
    return guards;

  auto const distance = [&points] (std::size_t a, std::size_t b) {
    auto const dx = points[a].x - points[b].x;
    auto const dy = points[a].y - points[b].y;
    return std::sqrt (dx * dx + dy * dy);
  };

  for (std::size_t i = 0; i < points.size (); ++i)
    {
      auto before = i;
      while (!points[before].startsRun && before > 0
             && distance (before, i) < radius)
        --before;

      auto after = i;
      while (after + 1 < points.size () && !points[after + 1].startsRun
             && distance (after, i) < radius)
        ++after;

      auto const ix = points[i].x - points[before].x;
      auto const iy = points[i].y - points[before].y;
      auto const ox = points[after].x - points[i].x;
      auto const oy = points[after].y - points[i].y;
      auto const lin = std::sqrt (ix * ix + iy * iy);
      auto const lout = std::sqrt (ox * ox + oy * oy);
      if (lin < 1e-6f || lout < 1e-6f)
        continue;

      auto const turn = std::abs (std::atan2 (ix * oy - iy * ox,
                                              ix * ox + iy * oy));
      guards[i] = foldGuard (turn / (0.5f * (lin + lout)), radius);
    }
  return guards;
}

}
