/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-engine/PatternGenerator.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <cmath>
#include <functional>
#include <vector>

using namespace a3;

// The shapes added on 2026-09-28 for the fifty-shape library. Each one is a
// musical idea -- a rhythm cell, a gesture from Smalley's motion typology, a
// dub move -- so each test pins the idea, not the drawing: a tresillo that
// jumped on the wrong sixteenths would still look like a triangle.

namespace
{
constexpr float radius = 0.9f;
HeightMapSphere const heightMap;

int
ticksPerSixteenth ()
{
  return TempoClock::getTicksPerBeat () / 4;
}

float
planar (Pos const &p)
{
  return std::hypot (p.x (), p.y ());
}

bool
samePlace (Pos const &a, Pos const &b)
{
  return std::abs (a.x () - b.x ()) < 1e-4f
         && std::abs (a.y () - b.y ()) < 1e-4f;
}

// The sixteenths at which a stepped figure lands somewhere new.
std::vector<int>
onsetsOf (Pattern const &pattern)
{
  std::vector<int> onsets;
  auto const step = ticksPerSixteenth ();
  auto const sixteenths
      = static_cast<int> (pattern.getNumTicks ()) / step;
  Pos last = Pos::invalid;
  for (int s = 0; s < sixteenths; ++s)
    {
      // Read the middle of the sixteenth: a jump may leave a gap tick at its
      // end, but never in its middle.
      auto const p = pattern.getTick (
          static_cast<index_t> (s * step + step / 2));
      if (!p.isValid ())
        continue;
      if (!last.isValid () || !samePlace (p, last))
        onsets.push_back (s);
      last = p;
    }
  return onsets;
}

std::vector<float>
radii (Pattern const &pattern)
{
  std::vector<float> out;
  for (index_t t = 0; t < pattern.getNumTicks (); ++t)
    if (pattern.getTick (t).isValid ())
      out.push_back (planar (pattern.getTick (t)));
  return out;
}
}

TEST (PatternShapes, TresilloLandsOnThreeThreeTwo)
{
  auto const p = PatternGenerator::createTresillo (4, radius, heightMap);
  ASSERT_EQ (p->getNumTicks (),
             static_cast<index_t> (4 * TempoClock::getTicksPerBeat ()));
  EXPECT_EQ (onsetsOf (*p), (std::vector<int>{ 0, 6, 12 }));
}

TEST (PatternShapes, ClaveThreeTwoHitsFiveTimesOverTwoBars)
{
  auto const p = PatternGenerator::createClave32 (8, radius, heightMap);
  EXPECT_EQ (onsetsOf (*p), (std::vector<int>{ 0, 6, 12, 20, 24 }));
}

TEST (PatternShapes, PingPongAnswersLeftAndRightOnEveryBeat)
{
  auto const p = PatternGenerator::createPingPong (4, radius, heightMap);
  auto const beat = TempoClock::getTicksPerBeat ();
  for (int b = 0; b < 4; ++b)
    {
      auto const x = p->getTick (static_cast<index_t> (b * beat + beat / 2)).x ();
      if (b % 2 == 0)
        EXPECT_LT (x, -0.5f * radius) << b;
      else
        EXPECT_GT (x, 0.5f * radius) << b;
    }
}

TEST (PatternShapes, TheRiserOnlyOpens)
{
  auto const r = radii (*PatternGenerator::createRiser (32, radius, heightMap));
  ASSERT_GT (r.size (), 100u);
  for (size_t i = 1; i < r.size (); ++i)
    ASSERT_GE (r[i] + 1e-5f, r[i - 1]) << i;
  EXPECT_LT (r.front (), 0.05f * radius);
  EXPECT_GT (r.back (), 0.95f * radius);
}

TEST (PatternShapes, TheCollapseOnlyCloses)
{
  auto const r
      = radii (*PatternGenerator::createCollapse (16, radius, heightMap));
  ASSERT_GT (r.size (), 100u);
  for (size_t i = 1; i < r.size (); ++i)
    ASSERT_LE (r[i], r[i - 1] + 1e-5f) << i;
  EXPECT_GT (r.front (), 0.95f * radius);
  EXPECT_LT (r.back (), 0.05f * radius);
}

TEST (PatternShapes, TheVortexTurnsFasterTheTighterItGets)
{
  auto const p = PatternGenerator::createVortex (32, radius, heightMap);
  auto const n = p->getNumTicks ();
  auto const turn = [&] (index_t a, index_t b) {
    auto const pa = p->getTick (a), pb = p->getTick (b);
    auto d = std::atan2 (pb.y (), pb.x ()) - std::atan2 (pa.y (), pa.x ());
    while (d < 0.f)
      d += 2.f * juce::MathConstants<float>::pi;
    return d;
  };
  EXPECT_GT (turn (n * 7 / 8, n * 7 / 8 + 8), turn (n / 8, n / 8 + 8));
  EXPECT_LT (planar (p->getTick (n * 7 / 8)), planar (p->getTick (n / 8)));
}

TEST (PatternShapes, EachEchoIsHalfTheOneBefore)
{
  auto const p = PatternGenerator::createEcho (16, radius, heightMap);
  std::vector<float> throws;
  for (auto const s : onsetsOf (*p))
    {
      auto const r = planar (p->getTick (static_cast<index_t> (
          s * ticksPerSixteenth () + ticksPerSixteenth () / 2)));
      if (r > 1e-3f)
        throws.push_back (r);
    }
  ASSERT_EQ (throws.size (), 4u);
  for (size_t i = 1; i < throws.size (); ++i)
    EXPECT_NEAR (throws[i], throws[i - 1] / 2.f, 1e-3f) << i;
}

TEST (PatternShapes, ThePulseBreathesFourTimesAround)
{
  auto const r = radii (*PatternGenerator::createPulse (16, radius, heightMap));
  int peaks = 0;
  for (size_t i = 1; i + 1 < r.size (); ++i)
    if (r[i] > r[i - 1] && r[i] >= r[i + 1])
      ++peaks;
  // The start is a peak too, counted where the loop closes.
  EXPECT_EQ (peaks + 1, 4);
}

TEST (PatternShapes, EveryNewShapeStaysInsideItsRadius)
{
  using Make = std::function<std::unique_ptr<Pattern> ()>;
  std::vector<std::pair<char const *, Make> > const shapes{
    { "Tresillo", [] { return PatternGenerator::createTresillo (4, radius, heightMap); } },
    { "Clave 3-2", [] { return PatternGenerator::createClave32 (8, radius, heightMap); } },
    { "Ping Pong", [] { return PatternGenerator::createPingPong (4, radius, heightMap); } },
    { "Riser", [] { return PatternGenerator::createRiser (32, radius, heightMap); } },
    { "Collapse", [] { return PatternGenerator::createCollapse (16, radius, heightMap); } },
    { "Vortex", [] { return PatternGenerator::createVortex (32, radius, heightMap); } },
    { "Echo", [] { return PatternGenerator::createEcho (16, radius, heightMap); } },
    { "Pulse", [] { return PatternGenerator::createPulse (16, radius, heightMap); } },
    { "Astroid", [] { return PatternGenerator::createAstroid (16, radius, heightMap); } },
    { "Trefoil", [] { return PatternGenerator::createTrefoil (16, radius, heightMap); } },
    { "Drift", [] { return PatternGenerator::createDrift (32, radius, heightMap); } },
  };
  for (auto const &[name, make] : shapes)
    {
      auto const p = make ();
      EXPECT_EQ (juce::String (p->getName ()), juce::String (name));
      auto const r = radii (*p);
      ASSERT_FALSE (r.empty ()) << name;
      for (auto const v : r)
        ASSERT_LE (v, radius + 1e-4f) << name;
    }
}

// The smooth ones come back to where they started, so the loop has no seam.
TEST (PatternShapes, TheSmoothOnesCloseOnThemselves)
{
  for (auto const &p : { PatternGenerator::createPulse (16, radius, heightMap),
                         PatternGenerator::createAstroid (16, radius, heightMap),
                         PatternGenerator::createTrefoil (16, radius, heightMap),
                         PatternGenerator::createDrift (32, radius, heightMap) })
    {
      auto const first = p->getTick (0);
      auto const last = p->getTick (p->getNumTicks () - 1);
      EXPECT_LT (std::hypot (first.x () - last.x (), first.y () - last.y ()),
                 0.05f * radius)
          << p->getName ();
    }
}

// A wander, not a scribble: the bearing keeps going round the one way. A
// sway larger than the turn made it back up on itself and draw a spur.
TEST (PatternShapes, TheDriftNeverTurnsBack)
{
  auto const p = PatternGenerator::createDrift (32, radius, heightMap);
  auto const step = 16u;
  for (index_t t = step; t < p->getNumTicks (); t += step)
    {
      auto const a = p->getTick (t - step), b = p->getTick (t);
      auto const cross = a.x () * b.y () - a.y () * b.x ();
      ASSERT_GT (cross, 0.f) << "at tick " << t;
    }
}

// -- Library v2 rhythm cells (2026-09-28) --------------------------------------
// The first read of a lap counts as a landing, so a figure whose last place is
// held over the loop starts its list with 0.

TEST (PatternShapes, FourFloorLandsOnEveryBeat)
{
  auto const p = PatternGenerator::createFourFloor (4, radius, heightMap);
  EXPECT_EQ (p->getName (), "Four Floor");
  EXPECT_EQ (onsetsOf (*p), (std::vector<int>{ 0, 4, 8, 12 }));
}

TEST (PatternShapes, OffbeatLandsBetweenTheBeats)
{
  auto const p = PatternGenerator::createOffbeat (4, radius, heightMap);
  EXPECT_EQ (onsetsOf (*p), (std::vector<int>{ 0, 2, 6, 10, 14 }));
}

TEST (PatternShapes, ClaveTwoThreeIsTheOtherWayRound)
{
  auto const p = PatternGenerator::createClave23 (8, radius, heightMap);
  EXPECT_EQ (onsetsOf (*p), (std::vector<int>{ 0, 4, 8, 16, 22, 28 }));
}

TEST (PatternShapes, GallopIsOneTwoThreeOnEveryBeat)
{
  auto const p = PatternGenerator::createGallop (4, radius, heightMap);
  EXPECT_EQ (onsetsOf (*p),
             (std::vector<int>{ 0, 2, 3, 4, 6, 7, 8, 10, 11, 12, 14, 15 }));
}

// Swing sits between the sixteenths: the second eighth of each beat lands two
// thirds of the way through it, not half.
TEST (PatternShapes, ShuffleSwingsTheSecondEighth)
{
  auto const p = PatternGenerator::createShuffle (4, radius, heightMap);
  auto const beat = static_cast<index_t> (TempoClock::getTicksPerBeat ());
  auto const swung = beat * 2 / 3;
  ASSERT_EQ (p->getNumTicks (), 4 * beat);
  for (index_t b = 0; b < 4; ++b)
    {
      auto const before = p->getTick (b * beat + swung - 2);
      auto const after = p->getTick (b * beat + swung + 2);
      ASSERT_TRUE (before.isValid () && after.isValid ()) << b;
      EXPECT_FALSE (samePlace (before, after)) << "no landing on beat " << b;
      EXPECT_TRUE (samePlace (p->getTick (b * beat + beat / 2),
                              p->getTick (b * beat + beat / 4)))
          << "a straight eighth landed on beat " << b;
    }
}

TEST (PatternShapes, TheNewRhythmCellsStayInsideTheirRadius)
{
  for (auto const &p : { PatternGenerator::createFourFloor (4, radius, heightMap),
                         PatternGenerator::createOffbeat (4, radius, heightMap),
                         PatternGenerator::createClave23 (8, radius, heightMap),
                         PatternGenerator::createShuffle (4, radius, heightMap),
                         PatternGenerator::createGallop (4, radius, heightMap) })
    {
      auto const r = radii (*p);
      ASSERT_FALSE (r.empty ()) << p->getName ();
      for (auto const v : r)
        ASSERT_LE (v, radius + 1e-4f) << p->getName ();
    }
}
