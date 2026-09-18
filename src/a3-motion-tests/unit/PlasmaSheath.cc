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

#include <a3-motion-ui/components/PlasmaSheath.hh>

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include <JuceHeader.h>

namespace a3
{

namespace
{
SheathRing const braid{ /* radius */ 0.01f, /* turns */ 48.f,
                        /* spin */ 0.4f, /* strands */ 3 };
SheathRing const sheath{ 0.09f, 7.f, -0.25f, 3 };
}

namespace
{
/** A line as the renderer hands it over: straight pieces a few thousandths
 *  long, subdivided evenly, meeting at a small corner every `perPiece`
 *  points. That corner is what flattening a curve leaves behind. */
std::vector<SheathPoint>
polylineArc (int pieces, int perPiece, float step, float cornerRadians)
{
  std::vector<SheathPoint> points;
  auto x = 0.f;
  auto y = 0.f;
  auto heading = 0.f;
  points.push_back ({ x, y, true });
  for (int piece = 0; piece < pieces; ++piece)
    {
      for (int k = 0; k < perPiece; ++k)
        {
          x += step * std::cos (heading);
          y += step * std::sin (heading);
          points.push_back ({ x, y, false });
        }
      heading += cornerRadians;
    }
  return points;
}
}

TEST (PlasmaSheath, TheCornersOfAFlattenedCurveAreNotFolds)
{
  // Measured on the device: a figure arrives as straight pieces twenty points
  // long, and the turn between two neighbouring points is zero along a piece
  // and all of it at the corner. Point by point that corner is a huge
  // curvature, so the strands were pulled onto the axis there and let go
  // again one point later -- a strut across the cord at every corner, which
  // is what made the plasma read as DNA.
  // Corner and spacing as logged on the device: guards of 0.04 and 0 at
  // corners of about a tenth of a radian between points 0.0038 apart.
  auto const points = polylineArc (20, 20, 0.0038f, 0.1f);
  auto const guards = foldGuardsAlong (points, 0.04f);

  ASSERT_EQ (guards.size (), points.size ());
  for (std::size_t i = 0; i < guards.size (); ++i)
    EXPECT_FLOAT_EQ (guards[i], 1.f) << "at point " << i;
}

TEST (PlasmaSheath, AHairpinTighterThanTheBraidStillPullsItIn)
{
  // What the guard is for must survive being measured over a stretch: a turn
  // back on itself inside the braid's own radius folds the offset copy.
  std::vector<SheathPoint> points;
  auto constexpr step = 0.002f;
  for (int k = 0; k < 60; ++k)
    points.push_back ({ step * static_cast<float> (k), 0.f, k == 0 });
  auto const tip = step * 59.f;
  for (int k = 1; k < 60; ++k)
    points.push_back ({ tip - step * static_cast<float> (k), 0.004f, false });

  auto const guards = foldGuardsAlong (points, 0.04f);

  EXPECT_FLOAT_EQ (guards[59], 0.f);
  EXPECT_FLOAT_EQ (guards[60], 0.f);
}

TEST (PlasmaSheath, AGuardNeverLooksAcrossAPenLift)
{
  // Two strokes that happen to meet at an angle are two strokes. The corner
  // between them is not a bend in either.
  std::vector<SheathPoint> points;
  for (int k = 0; k < 40; ++k)
    points.push_back ({ 0.003f * static_cast<float> (k), 0.f, k == 0 });
  auto const corner = 0.003f * 39.f;
  for (int k = 0; k < 40; ++k)
    points.push_back ({ corner, 0.003f * static_cast<float> (k + 1), k == 0 });

  auto const guards = foldGuardsAlong (points, 0.04f);

  EXPECT_FLOAT_EQ (guards[39], 1.f);
  EXPECT_FLOAT_EQ (guards[40], 1.f);
}

TEST (PlasmaSheath, TheBraidIsGuardedOverAStretch)
{
  // The fix above is worth nothing if the renderer goes on measuring point by
  // point beside it -- a coupling lost with every test green is how this
  // project breaks.
  juce::File const root (A3_UI_SOURCE_DIR);
  auto callers = 0;
  for (auto const &entry : juce::RangedDirectoryIterator (
           root, true, "*.cc", juce::File::findFiles))
    if (entry.getFile ().loadFileAsString ().contains ("sheathFrames ("))
      ++callers;

  EXPECT_GT (callers, 0)
      << "the braid still takes its across and its guard point by point";
}

TEST (PlasmaSheath, AGentleBendCarriesTheWholeOffset)
{
  // Nearly all of any figure. The guard must be invisible there, or every
  // braid is thinner than it was asked to be.
  EXPECT_FLOAT_EQ (foldGuard (1.f, 0.01f), 1.f);
  EXPECT_FLOAT_EQ (foldGuard (20.f, 0.02f), 1.f);
  EXPECT_FLOAT_EQ (foldGuard (0.f, 0.02f), 1.f);
}

TEST (PlasmaSheath, TheOffsetIsGoneBeforeTheFoldArrives)
{
  // A parallel curve folds at curvature * radius == 1. The strands have to be
  // back on the axis by then, not at it.
  EXPECT_FLOAT_EQ (foldGuard (100.f, 0.01f), 0.f);   // exactly one
  EXPECT_FLOAT_EQ (foldGuard (500.f, 0.01f), 0.f);   // far past it
}

TEST (PlasmaSheath, ItClosesSmoothlyRatherThanSnapping)
{
  // A strand that switched off would read as the braid breaking rather than
  // as it being drawn tight through a cusp.
  auto previous = 1.f;
  for (int step = 0; step <= 100; ++step)
    {
      auto const risk = static_cast<float> (step) / 100.f * 1.2f;
      auto const now = foldGuard (risk, 1.f);
      EXPECT_LE (now, previous + 1e-6f) << "at risk " << risk;
      EXPECT_GE (now, 0.f);
      EXPECT_LE (now, 1.f);
      previous = now;
    }

  // And it is genuinely part way in the middle, not a step in disguise.
  auto const middle = foldGuard (0.78f, 1.f);
  EXPECT_GT (middle, 0.1f);
  EXPECT_LT (middle, 0.9f);
}

TEST (PlasmaSheath, AZeroRadiusHasNothingToFold)
{
  EXPECT_FLOAT_EQ (foldGuard (1e9f, 0.f), 1.f);
}

TEST (PlasmaSheath, ARadiusOfZeroPutsEveryStrandOnTheLine)
{
  // Zero is off, and that is a promise rather than an approximation -- the
  // same one blobSparkle and blobTrail make. "Psychonautic" is a state
  // somebody has to be able to decline.
  auto ring = braid;
  ring.radius = 0.f;

  for (int strand = 0; strand < ring.strands; ++strand)
    for (int step = 0; step <= 40; ++step)
      {
        auto const u = static_cast<float> (step) / 40.f;
        EXPECT_FLOAT_EQ (sheathAt (u, strand, ring, 1.37f).offset, 0.f);
      }
}

TEST (PlasmaSheath, OffsetAndDepthStandInQuadrature)
{
  // Where a strand stands furthest out it is exactly side-on, and where it
  // crosses the axis it is furthest in front or behind. That is the whole
  // difference between a helix and a zigzag, and it is the first thing to go
  // when somebody tunes the numbers.
  for (int step = 0; step <= 200; ++step)
    {
      auto const u = static_cast<float> (step) / 200.f;
      auto const s = sheathAt (u, 0, sheath, 0.f);
      auto const across = s.offset / sheath.radius;
      EXPECT_NEAR (across * across + s.depth * s.depth, 1.f, 1e-4f);
    }
}

TEST (PlasmaSheath, TheHelixAdvancesEvenlyAndMakesExactlyItsTurns)
{
  // A coil, not a mess: the phase grows linearly in u, and over the whole
  // figure there are exactly `turns` of them.
  auto ring = sheath;
  ring.spin = 0.f;
  ring.turns = 6.f;

  // Count how often the first strand crosses the axis going one way. A full
  // turn does that once.
  //
  // The strand starts *on* the axis, so the crossing at u = 0 is the one the
  // figure has not made yet: the count runs over the half-open interval, or
  // six turns come out as seven.
  auto crossings = 0;
  auto previous = sheathAt (0.f, 0, ring, 0.f);
  for (int step = 1; step <= 20000; ++step)
    {
      auto const u = static_cast<float> (step) / 20000.f;
      auto const now = sheathAt (u, 0, ring, 0.f);
      if (previous.offset < 0.f && now.offset >= 0.f)
        ++crossings;
      previous = now;
    }
  EXPECT_EQ (crossings, 6);
}

TEST (PlasmaSheath, TheStrandsAreSpreadEvenlyAndNoTwoCoincide)
{
  auto ring = sheath;
  ring.strands = 3;

  auto const a = sheathAt (0.f, 0, ring, 0.f);
  auto const b = sheathAt (0.f, 1, ring, 0.f);
  auto const c = sheathAt (0.f, 2, ring, 0.f);

  // Three strands at 120 degrees: their depths sum to zero.
  EXPECT_NEAR (a.depth + b.depth + c.depth, 0.f, 1e-5f);

  // And none of the three sits on top of another. Compared as *places*, not
  // as depths: two strands standing symmetrically either side of the axis
  // have exactly the same depth by construction -- cos(120) equals cos(240)
  // -- and are as far apart as they ever get. A test that asked for
  // different depths would be asking the helix not to be a helix.
  auto const apart = [] (SheathSample const &l, SheathSample const &r) {
    return std::hypot (l.offset - r.offset, l.depth - r.depth);
  };
  EXPECT_GT (apart (a, b), 0.1f);
  EXPECT_GT (apart (b, c), 0.1f);
  EXPECT_GT (apart (a, c), 0.1f);
}

TEST (PlasmaSheath, SpinTurnsTheSheathLengthwiseAndItsSignIsTheDirection)
{
  // "die hüllkurve soll sich längs um die trajektorie drehen" -- at a fixed
  // point on the line the phase has to run with the clock, and the sign has
  // to decide which way.
  auto forward = sheath;
  forward.spin = 1.f;
  auto backward = forward;
  backward.spin = -1.f;

  auto const atStart = sheathAt (0.3f, 0, forward, 0.f);
  auto const quarterOn = sheathAt (0.3f, 0, forward, 0.25f);
  auto const quarterBack = sheathAt (0.3f, 0, backward, 0.25f);

  // A quarter turn on: what was across is now along, and the two directions
  // land on opposite sides of where it started.
  EXPECT_NEAR (quarterOn.depth, -atStart.offset / forward.radius, 1e-4f);
  EXPECT_NEAR (quarterBack.depth, atStart.offset / forward.radius, 1e-4f);
}

TEST (PlasmaSheath, AFullTurnBringsTheSheathBackToWhereItWas)
{
  auto ring = sheath;
  ring.spin = 2.f;

  auto const now = sheathAt (0.62f, 1, ring, 3.f);
  auto const later = sheathAt (0.62f, 1, ring, 3.f + 0.5f);

  EXPECT_NEAR (now.offset, later.offset, 1e-4f);
  EXPECT_NEAR (now.depth, later.depth, 1e-4f);
}

TEST (PlasmaSheath, AFlickerIsHeldAcrossItsBucketAndJumpsBetweenThem)
{
  // The corner is the effect. Interpolated, this is a wobble; held, it is a
  // bolt that ran straight for a stretch and then turned.
  // Forty buckets, so each is 0.025 of the figure wide.
  FlickerSettings const flicker{ /* buckets */ 40.f, /* rate */ 12.f };

  auto const firstBucket = flickerAt (0.001f, 0, flicker, 1.f);
  EXPECT_FLOAT_EQ (flickerAt (0.012f, 0, flicker, 1.f), firstBucket);
  EXPECT_FLOAT_EQ (flickerAt (0.024f, 0, flicker, 1.f), firstBucket);

  // The next bucket is somewhere else.
  EXPECT_NE (flickerAt (0.031f, 0, flicker, 1.f), firstBucket);
  EXPECT_NE (flickerAt (0.031f, 0, flicker, 1.f),
             flickerAt (0.056f, 0, flicker, 1.f));
}

TEST (PlasmaSheath, AFlickerStaysWithinItsBoundsAndIsRedrawnOnTheClock)
{
  FlickerSettings const flicker{ 40.f, 12.f };

  for (int step = 0; step <= 400; ++step)
    {
      auto const u = static_cast<float> (step) / 400.f;
      auto const v = flickerAt (u, 1, flicker, 2.f);
      EXPECT_GE (v, -1.f);
      EXPECT_LE (v, 1.f);
    }

  // Same place, a slot later: somewhere else.
  EXPECT_NE (flickerAt (0.4f, 1, flicker, 2.00f),
             flickerAt (0.4f, 1, flicker, 2.09f));
}

TEST (PlasmaSheath, NoFlickerWithoutBuckets)
{
  FlickerSettings const none{ 0.f, 12.f };
  EXPECT_FLOAT_EQ (flickerAt (0.37f, 0, none, 5.f), 0.f);
}

TEST (PlasmaSheath, DensityDecidesHowMuchOfTheLengthIsAlight)
{
  FlickerSettings const flicker{ 200.f, 8.f };

  auto litShare = [&flicker] (float density) {
    auto lit = 0;
    auto total = 0;
    for (int strand = 0; strand < 3; ++strand)
      for (int step = 0; step < 2000; ++step)
        {
          auto const u = static_cast<float> (step) / 2000.f;
          ++total;
          lit += aliveAt (u, strand, flicker, 1.5f, density) > 0.5f ? 1 : 0;
        }
    return static_cast<float> (lit) / static_cast<float> (total);
  };

  EXPECT_FLOAT_EQ (litShare (0.f), 0.f);
  EXPECT_FLOAT_EQ (litShare (1.f), 1.f);
  EXPECT_NEAR (litShare (0.5f), 0.5f, 0.06f);
  EXPECT_NEAR (litShare (0.25f), 0.25f, 0.06f);
}

TEST (PlasmaSheath, WhatIsAlightIsHeldAcrossItsBucketToo)
{
  // Otherwise the sheath is not made of bolts, it is made of noise.
  FlickerSettings const flicker{ 50.f, 8.f };
  auto const at = aliveAt (0.012f, 0, flicker, 1.f, 0.5f);
  EXPECT_FLOAT_EQ (aliveAt (0.004f, 0, flicker, 1.f, 0.5f), at);
  EXPECT_FLOAT_EQ (aliveAt (0.018f, 0, flicker, 1.f, 0.5f), at);
}

TEST (PlasmaSheath, NoArcsAtAllWhenTheyAreTurnedOff)
{
  ArcSettings off{ /* rate */ 0.f, /* density */ 1.f, /* width */ 0.1f };
  ArcSettings quiet{ 6.f, 0.f, 0.1f };

  for (int step = 0; step <= 60; ++step)
    {
      auto const u = static_cast<float> (step) / 60.f;
      for (float t = 0.f; t < 3.f; t += 0.07f)
        {
          EXPECT_FLOAT_EQ (arcAt (u, 0, off, t), 0.f);
          EXPECT_FLOAT_EQ (arcAt (u, 0, quiet, t), 0.f);
        }
    }
}

TEST (PlasmaSheath, AnArcIsATransientWithinItsSlot)
{
  // It comes and goes. A strike that stood there for the whole slot would be
  // a stripe, not lightning.
  ArcSettings arcs{ /* rate */ 4.f, /* density */ 1.f, /* width */ 0.2f };

  // Find where the strike in this slot is anchored by sampling across u.
  auto const slotStart = 0.25f;   // rate 4 -> slot boundaries every 0.25 s
  auto best = 0.f;
  auto anchor = 0.f;
  for (int step = 0; step <= 400; ++step)
    {
      auto const u = static_cast<float> (step) / 400.f;
      auto const v = arcAt (u, 0, arcs, slotStart + 0.125f);
      if (v > best)
        {
          best = v;
          anchor = u;
        }
    }
  ASSERT_GT (best, 0.f) << "density 1 should put a strike in every slot";

  // At the edges of the slot the strike has not risen / has fallen away.
  EXPECT_NEAR (arcAt (anchor, 0, arcs, slotStart + 0.0005f), 0.f, 0.02f);
  EXPECT_NEAR (arcAt (anchor, 0, arcs, slotStart + 0.2495f), 0.f, 0.02f);
  EXPECT_GT (arcAt (anchor, 0, arcs, slotStart + 0.125f), 0.5f);
}

TEST (PlasmaSheath, AStrikeIsOneAnchorPerStrandAndSlotNotAWobble)
{
  // Two strikes standing next to each other in the same slot read as the line
  // shaking rather than as something striking it.
  ArcSettings arcs{ 4.f, 1.f, 0.05f };
  auto const when = 0.6f;

  std::vector<float> lit;
  for (int step = 0; step <= 2000; ++step)
    {
      auto const u = static_cast<float> (step) / 2000.f;
      if (arcAt (u, 2, arcs, when) > 0.25f)
        lit.push_back (u);
    }
  ASSERT_FALSE (lit.empty ());

  // Everything lit belongs to one stretch: no gap wider than a couple of
  // samples anywhere in it.
  for (std::size_t i = 1; i < lit.size (); ++i)
    EXPECT_LT (lit[i] - lit[i - 1], 0.01f)
        << "a second strike at u = " << lit[i];
}

TEST (PlasmaSheath, DifferentStrandsStrikeAtDifferentPlaces)
{
  // Three strands striking in the same place at the same moment is one fat
  // strike, which is not what three strands are for.
  ArcSettings arcs{ 4.f, 1.f, 0.05f };
  auto const when = 0.6f;

  auto peakOf = [&arcs, when] (int strand) {
    auto best = 0.f;
    auto at = -1.f;
    for (int step = 0; step <= 2000; ++step)
      {
        auto const u = static_cast<float> (step) / 2000.f;
        auto const v = arcAt (u, strand, arcs, when);
        if (v > best)
          {
            best = v;
            at = u;
          }
      }
    return at;
  };

  EXPECT_GT (std::abs (peakOf (0) - peakOf (1)), 0.05f);
  EXPECT_GT (std::abs (peakOf (1) - peakOf (2)), 0.05f);
}

TEST (PlasmaSheath, AStillFingerIsNotAFold)
{
  // Every recording holds stretches of repeated points: the finger's position
  // arrives every ten milliseconds or so and a tick is a few, so the same
  // place is written several times over -- up to 55 in a row in the take
  // traced on 2026-09-17. The renderer read a zero-length step as a fold and
  // pulled the strands onto the axis there, which is the DNA strut again:
  // "enthalten sie noch die helixstreben (wie dna) sollte aber ja plasma
  // sein."
  std::vector<SheathPoint> points;
  for (int k = 0; k < 20; ++k)
    points.push_back ({ 0.004f * static_cast<float> (k), 0.f, k == 0 });
  // The finger stands still for a while.
  for (int k = 0; k < 40; ++k)
    points.push_back ({ 0.004f * 19.f, 0.f, false });
  for (int k = 1; k < 20; ++k)
    points.push_back ({ 0.004f * (19.f + static_cast<float> (k)), 0.f, false });

  auto const frames = sheathFrames (points, 0.04f);

  ASSERT_EQ (frames.size (), points.size ());
  for (std::size_t i = 0; i < frames.size (); ++i)
    {
      EXPECT_FLOAT_EQ (frames[i].guard, 1.f) << "at point " << i;
      // Across a line running along x, the normal is y -- including where the
      // finger stood still, or the strands would collapse and spring back.
      EXPECT_NEAR (std::abs (frames[i].acrossY), 1.f, 1e-5f) << "at point " << i;
      EXPECT_NEAR (frames[i].acrossX, 0.f, 1e-5f) << "at point " << i;
    }
}

}
