/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <a3-motion-engine/flight/BaseOrbit.hh>
#include <a3-motion-engine/flight/BeatPulse.hh>
#include <a3-motion-engine/flight/FlightWorld.hh>
#include <a3-motion-engine/flight/PilotGames.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/Geometry.hh>

#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using namespace a3;

// The games flown by the flight model and heard from the middle of the room:
// a bend counts only when it is at least the heard bend (30 deg), and a ship
// is where the figure says on the 1, not somewhere near it.

namespace
{
constexpr int fourFour = 4;
GameTuning const tuning;

float
degreesOf (Vec2 p)
{
  return std::atan2 (p.y, p.x) * 180.f / pi<float> ();
}

float
degreesBetween (Vec2 a, Vec2 b)
{
  return std::abs (std::remainder (degreesOf (a) - degreesOf (b), 360.f));
}

FlightBodies
oneGroupAt (Vec2 at)
{
  FlightBodies bodies;
  bodies.count = 1;
  bodies.body[0] = { at, FlightTuning{}.groupMass, 0 };
  return bodies;
}

FlightBodies
oneGroup ()
{
  return oneGroupAt ({ -0.5f, 0.3f });
}

/** The clock's place at `beats`, as the engine hands it to the pulse. */
Measure
measureAt (double beats)
{
  auto const perBeat = TempoClock::getTicksPerBeat ();
  auto const ticks = static_cast<int> (std::llround (beats * perBeat));
  auto const wholeBeats = ticks / perBeat;
  return { wholeBeats / fourFour, wholeBeats % fourFour, ticks % perBeat };
}

MusicCue
heading (MusicSection now, std::optional<MusicSection> next, long long changeBar)
{
  MusicCue cue;
  cue.section = now;
  cue.next = next;
  cue.changeBar = changeBar;
  cue.energy = 0.5f;
  return cue;
}

/** The four ships on their rabbits, the games beside them, stepped the way
 *  the engine steps them: the games first, then the world with their goals. */
struct Floor
{
  explicit Floor (FlightBodies floorBodies = oneGroup (), juce::int64 seed = 3,
                  GameTuning const &gameTuning = {})
      : bodies (floorBodies), games (seed, FlightTuning{}, gameTuning)
  {
    FlightTuning const flight;
    for (auto ch = 0; ch < flightShips; ++ch)
      world.launch (ch, rabbitAt (0., ch, fourFour, flight).at, 0., fourFour);
  }

  GameShips
  ships () const
  {
    GameShips out{};
    for (auto ch = 0; ch < flightShips; ++ch)
      out[static_cast<size_t> (ch)]
          = { canFly[static_cast<size_t> (ch)], ShipResume{ true, noBodyId }, world.ship (ch) };
    return out;
  }

  bool
  ask (PilotGame game, int leader, PilotRecruit with, MusicCue const &cue)
  {
    GameRequest request;
    request.order.game = game;
    request.order.with = with;
    request.leader = leader;
    return games.request (request, ships (), bodies, cue, beats, fourFour);
  }

  /** Steps to beat `until`, calling `look` with the beat after every tick. */
  void
  runTo (double until, MusicCue const &cue, std::function<void (double)> const &look = {})
  {
    auto const dt = 1. / TempoClock::getTicksPerBeat ();
    while (beats < until - 1e-9)
      {
        games.step (ships (), bodies, cue, PilotLevel::Off, beats, fourFour);
        std::array<ShipOrders, flightShips> orders{};
        for (auto ch = 0; ch < flightShips; ++ch)
          {
            auto &order = orders[static_cast<size_t> (ch)];
            order.flying = true;
            if (auto const goal = games.steerOf (ch, beats, fourFour))
              {
                order.goal = FlightGoal::Steer;
                order.steer = *goal;
                order.bodyId = games.gameOf (ch)->target;
              }
          }
        world.step (orders, bodies, beats, fourFour,
                    gravityPulse (measureAt (beats), fourFour, FlightTuning{}),
                    static_cast<float> (dt));
        beats += dt;
        if (look)
          look (beats);
      }
  }

  Vec2
  at (int ch) const
  {
    return world.ship (ch).p;
  }

  FlightBodies bodies;
  FlightWorld world{ 7 };
  PilotGames games;
  std::array<bool, flightShips> canFly{ true, true, true, true };
  double beats = 0.;
};

/** A fake-out as the middle of the room hears it: how far the ship stands
 *  from its target when the approach ends, the widest it turns away before
 *  the strike, and the nearest it comes on the 1. */
struct HeardFakeOut
{
  float standOff = 0.f;
  float approached = 0.f;
  float widest = 0.f;
  /** The widest the ship turns from where the approach ended. */
  float turned = 0.f;
  float closest = 180.f;
};

/** Ship 0's fake-out at `target`, its 1 on beat 16; `each` sees every tick. */
HeardFakeOut
flyFakeOut (Floor &floor, MusicCue const &cue, Vec2 target,
            std::function<void (double)> const &each = [] (double) {})
{
  HeardFakeOut heard;
  floor.runTo (12., cue, each); // the approach
  heard.standOff = floor.at (0).getDistanceFrom (target);
  heard.approached = degreesBetween (floor.at (0), target);
  auto const approachEnd = floor.at (0);
  floor.runTo (16. - tuning.strikeBeats, cue, [&] (double beat) {
    heard.widest = std::max (heard.widest, degreesBetween (floor.at (0), target));
    heard.turned = std::max (heard.turned, degreesBetween (floor.at (0), approachEnd));
    each (beat);
  });
  floor.runTo (17., cue, [&] (double beat) {
    if (beat >= 15.)
      heard.closest = std::min (heard.closest, degreesBetween (floor.at (0), target));
    each (beat);
  });
  return heard;
}

Vec2
onCircle (float degrees, float radius)
{
  auto const angle = degrees * pi<float> () / 180.f;
  return { radius * std::cos (angle), radius * std::sin (angle) };
}

/** How fast the heard direction, seen from the middle, swings: degrees per
 *  beat between two ticks. */
float
swingDegreesPerBeat (Vec2 before, Vec2 after)
{
  return degreesBetween (before, after) * static_cast<float> (TempoClock::getTicksPerBeat ());
}
/** The nearest two ships of a game come to each other from now to beat
 *  `until`, and when. */
struct NearestPair
{
  float distance = 2.f;
  double beat = 0.;
};

NearestPair
nearestPairUntil (Floor &floor, double until, MusicCue const &cue)
{
  NearestPair nearest;
  floor.runTo (until, cue, [&] (double beat) {
    for (auto a = 0; a < flightShips; ++a)
      for (auto b = a + 1; b < flightShips; ++b)
        {
          if (!floor.games.plays (a) || !floor.games.plays (b))
            continue;
          if (auto const d = floor.at (a).getDistanceFrom (floor.at (b)); d < nearest.distance)
            nearest = { d, beat };
        }
  });
  return nearest;
}
}

TEST (PilotGamesFlight, TheFakeOutIsHeardTurningAwayAndThenAtItsTargetOnTheOne)
{
  for (auto const breathing : { false, true })
    {
      Floor floor;
      floor.world.setBreathing (breathing);
      auto const cue = heading (MusicSection::Build, MusicSection::Drop, 4); // the 1 on beat 16
      ASSERT_TRUE (floor.ask (PilotGame::FakeOut, 0, PilotRecruit::Self, cue));
      auto const heard = flyFakeOut (floor, cue, floor.bodies.body[0].at);
      EXPECT_LT (heard.standOff, 0.35f) << "breathing " << breathing;
      EXPECT_GE (heard.widest - heard.approached, tuning.heardBendDegrees)
          << "breathing " << breathing;
      EXPECT_LE (heard.closest, 15.f) << "breathing " << breathing;
    }
}

TEST (PilotGamesFlight, TheFormationIsHeardInALineAndThenBurstingApartOnTheOne)
{
  Floor floor;
  auto const cue = heading (MusicSection::Build, MusicSection::Drop, 3); // the 1 on beat 12
  ASSERT_TRUE (floor.ask (PilotGame::Formation, 0, PilotRecruit::All, cue));
  floor.runTo (11.9, cue);

  std::array<Vec2, flightShips> lined{};
  for (auto ch = 0; ch < flightShips; ++ch)
    {
      auto const slot = floor.games.steerOf (ch, floor.beats, fourFour);
      ASSERT_TRUE (slot.has_value ()) << ch;
      EXPECT_LT (floor.at (ch).getDistanceFrom (slot->at), 0.15f) << ch;
      lined[static_cast<size_t> (ch)] = floor.at (ch);
    }

  floor.runTo (12. + fourFour - 0.1, cue); // the bar after the 1
  for (auto ch = 0; ch < flightShips; ++ch)
    EXPECT_GE (degreesBetween (floor.at (ch), lined[static_cast<size_t> (ch)]),
               tuning.heardBendDegrees)
        << ch;
}

TEST (PilotGamesFlight, TheHiderIsHeardOnOneSideAndThenOppositeOnTheOne)
{
  Floor floor (FlightBodies{});
  auto const cue = heading (MusicSection::Breakdown, MusicSection::Build, 4); // the 1 on beat 16
  ASSERT_TRUE (floor.ask (PilotGame::HideAndSeek, 1, PilotRecruit::Self, cue));
  auto const crossFrom = 16. - tuning.crossBeats;
  floor.runTo (crossFrom - 0.01, cue);
  auto const hideGoal = floor.games.steerOf (1, floor.beats, fourFour);
  ASSERT_TRUE (hideGoal.has_value ());
  auto const hidden = floor.at (1);
  EXPECT_LT (degreesBetween (hidden, hideGoal->at), 15.f);

  floor.runTo (16.5, cue);
  EXPECT_GE (degreesBetween (floor.at (1), hidden), 150.f);
}

TEST (PilotGamesFlight, TwoShipsAreHeardAnsweringEachOtherABarEach)
{
  Floor floor (FlightBodies{});
  MusicCue groove;
  groove.energy = 0.5f;
  ASSERT_TRUE (floor.ask (PilotGame::CallAndResponse, 0, PilotRecruit::Self, groove));
  auto partner = -1;
  for (auto ch = 1; ch < flightShips; ++ch)
    if (floor.games.plays (ch))
      partner = ch;
  ASSERT_GE (partner, 0);

  floor.runTo (4., groove); // gathered; the first call
  auto const leaderBefore = floor.at (0);
  auto const partnerBefore = floor.at (partner);
  floor.runTo (8., groove);
  EXPECT_GE (degreesBetween (floor.at (0), leaderBefore), tuning.heardBendDegrees);
  EXPECT_LT (degreesBetween (floor.at (partner), partnerBefore), 10.f);

  auto const leaderMid = floor.at (0);
  auto const partnerMid = floor.at (partner);
  floor.runTo (12., groove); // the answer
  EXPECT_LT (degreesBetween (floor.at (0), leaderMid), 10.f);
  EXPECT_GE (degreesBetween (floor.at (partner), partnerMid), tuning.heardBendDegrees);
}

/** Every ship's state at every tick from now to beat `until`. */
std::vector<ShipState>
pathUntil (Floor &floor, double until, MusicCue const &cue)
{
  std::vector<ShipState> path;
  floor.runTo (until, cue, [&] (double) {
    for (auto ch = 0; ch < flightShips; ++ch)
      path.push_back (floor.world.ship (ch));
  });
  return path;
}

bool
samePath (std::vector<ShipState> const &a, std::vector<ShipState> const &b)
{
  return a.size () == b.size ()
         && std::memcmp (a.data (), b.data (), a.size () * sizeof (ShipState)) == 0;
}

TEST (PilotGamesFlight, SameSeedSameSkyWithGames)
{
  auto const fly = [] {
    Floor floor;
    auto const cue = heading (MusicSection::Build, MusicSection::Drop, 3);
    floor.ask (PilotGame::Formation, 0, PilotRecruit::All, cue);
    return pathUntil (floor, 20., cue);
  };
  EXPECT_TRUE (samePath (fly (), fly ()));
}

TEST (PilotGamesFlight, SameSeedSameSkyWhenTheDiceChooseTheVeer)
{
  // The leader comes in on the group's own line through the middle, so
  // neither side is the one it comes in on and the dice choose the veer.
  Vec2 const onTheLeadersLine{ -0.5f, 0.f };
  auto const fly = [&] (juce::int64 seed, float &veerSide) {
    Floor floor (oneGroupAt (onTheLeadersLine), seed);
    auto const cue = heading (MusicSection::Build, MusicSection::Drop, 4);
    floor.ask (PilotGame::FakeOut, 0, PilotRecruit::Self, cue);
    veerSide = floor.games.steerOf (0, 13., fourFour)->at.y;
    return pathUntil (floor, 20., cue);
  };
  auto clockwise = 0;
  auto counterClockwise = 0;
  for (juce::int64 seed = 1; seed <= 8; ++seed)
    {
      auto sideA = 0.f;
      auto sideB = 0.f;
      auto const a = fly (seed, sideA);
      auto const b = fly (seed, sideB);
      EXPECT_TRUE (samePath (a, b)) << "seed " << seed;
      EXPECT_EQ (sideA, sideB) << "seed " << seed;
      (sideA < 0.f ? clockwise : counterClockwise) += 1;
    }
  EXPECT_GT (clockwise, 0) << "the dice never chose clockwise";
  EXPECT_GT (counterClockwise, 0) << "the dice never chose counter-clockwise";
}

TEST (PilotGamesFlight, AFakeOutAtAGroupNearTheMiddleStillTurnsAHeardBendAway)
{
  // Near the middle the approach ends well off the target's direction, seen
  // from the middle. Whichever side it comes in on, the ship must be heard
  // turning a bend from where the approach ended, end that bend a heard
  // bend away from its target, and still land on it on the 1.
  auto narrowest = 180.f;
  auto widestMiss = 0.f;
  for (auto const radius : { 0.2f, 0.3f })
    for (auto degrees = 0; degrees < 360; degrees += 30)
      for (juce::int64 seed = 1; seed <= 4; ++seed)
        {
          Floor floor (oneGroupAt (onCircle (static_cast<float> (degrees), radius)), seed);
          auto const cue = heading (MusicSection::Build, MusicSection::Drop, 4);
          ASSERT_TRUE (floor.ask (PilotGame::FakeOut, 0, PilotRecruit::Self, cue));
          auto const heard = flyFakeOut (floor, cue, floor.bodies.body[0].at);
          narrowest = std::min (narrowest, heard.turned);
          widestMiss = std::max (widestMiss, heard.closest);
          EXPECT_GE (heard.turned, tuning.heardBendDegrees)
              << "group at " << degrees << " deg, radius " << radius << ", seed " << seed;
          EXPECT_GE (heard.widest, tuning.heardBendDegrees)
              << "group at " << degrees << " deg, radius " << radius << ", seed " << seed;
          EXPECT_LE (heard.closest, 15.f)
              << "group at " << degrees << " deg, radius " << radius << ", seed " << seed;
        }
  RecordProperty ("narrowestBendDegrees", juce::String (narrowest, 1).toStdString ());
  RecordProperty ("widestMissDegrees", juce::String (widestMiss, 1).toStdString ());
}

TEST (PilotGamesFlight, AThreeShipFormationBurstsApartWithItsMiddleShipAcrossTheRoom)
{
  // The middle place of three bursts straight across the room. On a flat
  // clip that is a flip of the heard direction, which the engine's turn
  // limit spreads over a beat; recorded here: how near the middle the floor
  // path passes and how fast its direction swings before that limit.
  Floor floor;
  floor.canFly[3] = false;
  auto const cue = heading (MusicSection::Build, MusicSection::Drop, 3); // the 1 on beat 12
  ASSERT_TRUE (floor.ask (PilotGame::Formation, 0, PilotRecruit::All, cue));
  ASSERT_FALSE (floor.games.plays (3));
  floor.runTo (11.9, cue);

  std::array<Vec2, flightShips> lined{};
  for (auto ch = 0; ch < 3; ++ch)
    lined[static_cast<size_t> (ch)] = floor.at (ch);
  auto nearestMiddle = 2.f;
  auto fastestSwing = 0.f;
  auto before = lined;
  floor.runTo (12. + fourFour - 0.1, cue, [&] (double) {
    for (auto ch = 0; ch < 3; ++ch)
      {
        auto const now = floor.at (ch);
        nearestMiddle = std::min (nearestMiddle, now.getDistanceFromOrigin ());
        fastestSwing = std::max (fastestSwing,
                                 swingDegreesPerBeat (before[static_cast<size_t> (ch)], now));
        before[static_cast<size_t> (ch)] = now;
      }
  });
  RecordProperty ("nearestMiddle", juce::String (nearestMiddle, 3).toStdString ());
  RecordProperty ("fastestSwingDegreesPerBeat", juce::String (fastestSwing, 0).toStdString ());
  for (auto ch = 0; ch < 3; ++ch)
    EXPECT_GE (degreesBetween (floor.at (ch), lined[static_cast<size_t> (ch)]),
               tuning.heardBendDegrees)
        << ch;
}

TEST (PilotGamesFlight, AFakeOutWithNoGroupCrossesTheRoomAndIsHeardOnTheOne)
{
  // With no group the fake-out aims across the room, so the approach runs
  // through the middle: on a flat clip a heard flip, which the engine's turn
  // limit spreads over a beat; recorded as for the three-ship burst.
  Floor floor (FlightBodies{});
  auto const cue = heading (MusicSection::Build, MusicSection::Drop, 4); // the 1 on beat 16
  ASSERT_TRUE (floor.ask (PilotGame::FakeOut, 0, PilotRecruit::Self, cue));
  auto const aim = floor.games.steerOf (0, 16., fourFour);
  ASSERT_TRUE (aim.has_value ());

  auto nearestMiddle = 2.f;
  auto fastestSwing = 0.f;
  auto before = floor.at (0);
  auto const heard = flyFakeOut (floor, cue, aim->at, [&] (double) {
    nearestMiddle = std::min (nearestMiddle, floor.at (0).getDistanceFromOrigin ());
    fastestSwing = std::max (fastestSwing, swingDegreesPerBeat (before, floor.at (0)));
    before = floor.at (0);
  });
  RecordProperty ("nearestMiddle", juce::String (nearestMiddle, 3).toStdString ());
  RecordProperty ("fastestSwingDegreesPerBeat", juce::String (fastestSwing, 0).toStdString ());
  RecordProperty ("bendDegrees",
                  juce::String (heard.widest - heard.approached, 1).toStdString ());
  RecordProperty ("closestDegrees", juce::String (heard.closest, 1).toStdString ());
  EXPECT_GE (heard.widest - heard.approached, tuning.heardBendDegrees);
  EXPECT_LE (heard.closest, 15.f);
}

// Two ships that meet on one point are heard as one. The softening core of
// the push between ships is the nearest two of a crew may come.
float const apart = FlightTuning{}.separationSoftening;

TEST (PilotGamesFlight, ACrewMemberFromTheOtherSideIsHeardTurningAway)
{
  // A crew flies one rigid figure in lanes, dealt by shortest glides. A
  // member that comes in on the target's other side from the leader still
  // veers at the bar before the 1, and must be heard turning too.
  Floor floor;
  auto const cue = heading (MusicSection::Build, MusicSection::Drop, 4); // the 1 on beat 16
  ASSERT_TRUE (floor.ask (PilotGame::FakeOut, 0, PilotRecruit::All, cue));
  auto const target = floor.bodies.body[0].at;
  floor.runTo (12., cue);
  auto const sideOf = [&] (Vec2 p) { return target.x * p.y - target.y * p.x < 0.f; };
  std::array<Vec2, flightShips> approachEnd{};
  auto member = -1;
  for (auto ch = 0; ch < flightShips; ++ch)
    {
      approachEnd[static_cast<size_t> (ch)] = floor.at (ch);
      if (ch != 0 && member < 0 && sideOf (floor.at (ch)) != sideOf (floor.at (0)))
        member = ch;
    }
  ASSERT_GE (member, 0) << "no member comes in on the other side";
  auto turned = 0.f;
  floor.runTo (16. - tuning.strikeBeats, cue, [&] (double) {
    turned = std::max (turned, degreesBetween (floor.at (member),
                                               approachEnd[static_cast<size_t> (member)]));
  });
  RecordProperty ("member", member);
  RecordProperty ("turnedDegrees", juce::String (turned, 1).toStdString ());
  EXPECT_GE (turned, tuning.heardBendDegrees) << "member " << member;
}

TEST (PilotGamesFlight, ACrewsFakeOutKeepsItsShipsApart)
{
  Floor floor;
  auto const cue = heading (MusicSection::Build, MusicSection::Drop, 4); // the 1 on beat 16
  ASSERT_TRUE (floor.ask (PilotGame::FakeOut, 0, PilotRecruit::All, cue));
  auto const nearest = nearestPairUntil (floor, 20., cue);
  RecordProperty ("nearestDistance", juce::String (nearest.distance, 3).toStdString ());
  RecordProperty ("nearestBeat", juce::String (nearest.beat, 2).toStdString ());
  EXPECT_GE (nearest.distance, apart) << "at beat " << nearest.beat;
}

TEST (PilotGamesFlight, ACrewsFakeOutAtAGroupNearTheMiddleKeepsItsShipsApart)
{
  auto nearestOfAll = 2.f;
  for (auto const radius : { 0.2f, 0.3f })
    for (auto degrees = 0; degrees < 360; degrees += 45)
      {
        Floor floor (oneGroupAt (onCircle (static_cast<float> (degrees), radius)));
        auto const cue = heading (MusicSection::Build, MusicSection::Drop, 4);
        ASSERT_TRUE (floor.ask (PilotGame::FakeOut, 0, PilotRecruit::All, cue));
        auto const nearest = nearestPairUntil (floor, 20., cue);
        nearestOfAll = std::min (nearestOfAll, nearest.distance);
        EXPECT_GE (nearest.distance, apart) << "group at " << degrees << " deg, radius "
                                            << radius << ", beat " << nearest.beat;
      }
  RecordProperty ("nearestDistance", juce::String (nearestOfAll, 3).toStdString ());
}

TEST (PilotGamesFlight, AFormationKeepsItsShipsApart)
{
  // A group near the rim, which the line faces, and one on the other side
  // of the room, across the ships' paths to their places.
  auto nearestOfAll = 2.f;
  for (auto const group : { Vec2{ -0.5f, 0.3f }, Vec2{ 0.3f, -0.6f } })
    {
      Floor floor (oneGroupAt (group));
      auto const cue = heading (MusicSection::Build, MusicSection::Drop, 3); // the 1 on beat 12
      ASSERT_TRUE (floor.ask (PilotGame::Formation, 0, PilotRecruit::All, cue));
      auto const nearest = nearestPairUntil (floor, 16., cue);
      nearestOfAll = std::min (nearestOfAll, nearest.distance);
      EXPECT_GE (nearest.distance, apart)
          << "group at " << group.x << ", " << group.y << ", beat " << nearest.beat;
      RecordProperty ("nearest " + juce::String (group.x).toStdString () + ","
                          + juce::String (group.y).toStdString (),
                      juce::String (nearest.distance, 3).toStdString ());
    }
  RecordProperty ("nearestDistance", juce::String (nearestOfAll, 3).toStdString ());
}

TEST (PilotGamesFlight, AFormationAnywhereRoundTheRoomKeepsItsShipsApartAndBurstsAHeardBend)
{
  // The line faces a group anywhere round the room: the four ships stay the
  // core apart through the line-up, the burst and the bar after it, and
  // every ship's burst point lies at least 45 deg from its place.
  auto nearestOfAll = 2.f;
  auto narrowestBurst = 180.f;
  for (auto const radius : { 0.4f, 0.6f, 0.8f })
    for (auto degrees = 0; degrees < 360; degrees += 30)
      {
        Floor floor (oneGroupAt (onCircle (static_cast<float> (degrees), radius)));
        auto const cue = heading (MusicSection::Build, MusicSection::Drop, 3); // the 1 on beat 12
        ASSERT_TRUE (floor.ask (PilotGame::Formation, 0, PilotRecruit::All, cue));
        for (auto ch = 0; ch < flightShips; ++ch)
          {
            auto const place = floor.games.steerOf (ch, 11.9, fourFour)->at;
            auto const burst = floor.games.steerOf (ch, 12., fourFour)->at;
            narrowestBurst = std::min (narrowestBurst, degreesBetween (place, burst));
            EXPECT_GE (degreesBetween (place, burst), 45.f)
                << "group at " << degrees << " deg, radius " << radius << ", ship " << ch;
          }
        auto const nearest = nearestPairUntil (floor, 16., cue);
        nearestOfAll = std::min (nearestOfAll, nearest.distance);
        EXPECT_GE (nearest.distance, apart) << "group at " << degrees << " deg, radius "
                                            << radius << ", beat " << nearest.beat;
      }
  RecordProperty ("nearestDistance", juce::String (nearestOfAll, 3).toStdString ());
  RecordProperty ("narrowestBurstDegrees", juce::String (narrowestBurst, 1).toStdString ());
}

TEST (PilotGamesFlight, ACallAndItsAnswerKeepTheirShipsApart)
{
  Floor floor (FlightBodies{});
  MusicCue groove;
  groove.energy = 0.5f;
  ASSERT_TRUE (floor.ask (PilotGame::CallAndResponse, 0, PilotRecruit::Self, groove));
  auto const nearest = nearestPairUntil (floor, 24., groove);
  RecordProperty ("nearestDistance", juce::String (nearest.distance, 3).toStdString ());
  RecordProperty ("nearestBeat", juce::String (nearest.beat, 2).toStdString ());
  EXPECT_GE (nearest.distance, apart) << "at beat " << nearest.beat;
}

// A group the DJ placed between the rim and the speakers, past the disc the
// ships fly on: a game played against it still keeps every ship inside the
// disc, finite, and ends on time.
TEST (PilotGamesFlight, AGameAgainstAGroupPastTheRimStaysInsideTheDisc)
{
  for (auto const game : { PilotGame::FakeOut, PilotGame::Formation })
    for (auto const with : { PilotRecruit::Self, PilotRecruit::All })
      for (auto const degrees : { 30.f, 160.f, -100.f })
        {
          Floor floor (oneGroupAt (onCircle (degrees, 1.3f)));
          auto const cue = heading (MusicSection::Build, MusicSection::Drop, 4); // the 1 on beat 16
          ASSERT_TRUE (floor.ask (game, 0, with, cue));
          auto worst = 0.f;
          auto finite = true;
          floor.runTo (24., cue, [&] (double) {
            for (auto ch = 0; ch < flightShips; ++ch)
              {
                auto const &ship = floor.world.ship (ch);
                finite = finite && std::isfinite (ship.p.x) && std::isfinite (ship.p.y);
                worst = std::max (worst, ship.p.getDistanceFromOrigin ());
              }
          });
          auto const what = std::string (game == PilotGame::FakeOut ? "fake-out" : "formation")
                            + (with == PilotRecruit::All ? ", all" : ", alone") + ", group at "
                            + std::to_string (degrees) + " deg";
          EXPECT_TRUE (finite) << what;
          EXPECT_LE (worst, 1.f + 1e-5f) << what;
          EXPECT_FALSE (floor.games.plays (0)) << what << ": over by beat 24";
        }
}
