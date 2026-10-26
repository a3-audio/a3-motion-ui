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

// The pro-DJ's offline pass of 2026-10-17 as a regression test: a scripted
// set at 128 BPM -- music cues pushed per bar as StemDeck previews them or
// the live mood hears them, presses as ActionReach sends them -- run on the
// game floor tick by tick (GameFloor.hh), so every run is the same and takes
// milliseconds. The real engine on the real clock, with CSV output for the
// plots, is PilotsOfflinePassTool (DISABLED_, a tool).
//
// Slaps land on beats 1 and 3, never on the breath's 4; every drop is
// slapped -- on its 1 when it was previewed, whatever pilot figure was
// running; a DJ's figure is never pre-empted; a ship handed back cruises on
// from where it stands instead of swinging through the middle; and over 64
// bars the pilots keep to their budget.

#include <gtest/gtest.h>

#include "GameFloor.hh"

#include <a3-motion-engine/elevation/HeightMapSphere.hh>
#include <a3-motion-engine/flight/PilotGames.hh>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <vector>

using namespace a3;
using a3::gamefloor::Floor;
using a3::gamefloor::fourFour;
using a3::gamefloor::oneGroupAt;

namespace
{
GameTuning const tuning;
double const tick = 1. / TempoClock::getTicksPerBeat ();
std::vector<juce::int64> const seeds{ 1, 2, 3, 4 };

using CueScript = std::function<MusicCue (long long)>;

MusicCue
cue (MusicSection now, std::optional<MusicSection> next, long long changeBar, long long since,
     float energy, bool previewed = true)
{
  MusicCue c;
  c.section = now;
  c.next = next;
  c.changeBar = changeBar;
  c.sectionStartBar = since;
  c.energy = energy;
  c.previewed = previewed;
  return c;
}

/** 64 bars as StemDeck previews them: groove, a 16-bar build, the drop on
 *  bar 32, a breakdown. */
MusicCue
previewedSet (long long bar)
{
  if (bar < 16)
    return cue (MusicSection::Groove, MusicSection::Build, 16, 0, 0.5f);
  if (bar < 32)
    return cue (MusicSection::Build, MusicSection::Drop, 32, 16, 0.6f);
  if (bar < 48)
    return cue (MusicSection::Drop, MusicSection::Breakdown, 48, 32, 0.9f);
  return cue (MusicSection::Breakdown, MusicSection::Groove, 64, 48, 0.4f);
}

/** The same set as the live mood hears it: the build three bars after it
 *  starts rising, the drop of bar 32 a bar late, the breakdown a bar late. */
MusicCue
liveSet (long long bar)
{
  auto const nextLine = [] (long long b) { return (b / 8 + 1) * 8; };
  if (bar < 19)
    return cue (MusicSection::Groove, std::nullopt, -1, 0, 0.5f, false);
  if (bar < 33)
    return cue (MusicSection::Build, MusicSection::Drop, nextLine (bar), 19, 0.6f, false);
  if (bar < 41)
    return cue (MusicSection::Drop, MusicSection::Groove, 40, 32, 0.9f, false);
  if (bar < 49)
    return cue (MusicSection::Groove, std::nullopt, -1, 41, 0.6f, false);
  return cue (MusicSection::Breakdown, MusicSection::Build, nextLine (bar), 48, 0.3f, false);
}

/** A breakdown StemDeck says ends in the drop of bar 16. */
MusicCue
breakdownIntoDrop (long long bar)
{
  if (bar < 16)
    return cue (MusicSection::Breakdown, MusicSection::Drop, 16, 0, 0.4f);
  return cue (MusicSection::Drop, MusicSection::Groove, 32, 16, 0.9f);
}

/** The rig on 2026-10-17: "groove -> drop" previewed, the drop on bar 21,
 *  so the pilots' call of bar 16 runs to the drop's 1. */
MusicCue
grooveIntoDrop (long long bar)
{
  if (bar < 21)
    return cue (MusicSection::Groove, MusicSection::Drop, 21, 0, 0.5f);
  return cue (MusicSection::Drop, MusicSection::Breakdown, 37, 21, 0.9f);
}

PilotOrder
autoPress ()
{
  PilotOrder order;
  order.style = PilotStyle::Auto;
  return order;
}

PilotOrder
named (PilotGame game, PilotRecruit with = PilotRecruit::Self)
{
  PilotOrder order;
  order.game = game;
  order.with = with;
  return order;
}

struct Press
{
  double beat;
  int ship;
  PilotOrder order;
};

/** What happened on one ship at one tick, at the beat the games were
 *  stepped on. */
struct Seen
{
  double beat;
  int ship;
  PilotGame game;
  bool byPilot;
  std::optional<OrbitPoint> steer;
  Vec2 at;
};

/** A run: per tick and ship, what the board said and where the ship flew. */
struct Run
{
  std::vector<std::vector<Seen>> ticks;

  std::optional<Seen>
  on (int ship, double beat) const
  {
    auto const at = std::llround (beat / tick);
    if (at < 0 || at >= static_cast<long long> (ticks.size ()))
      return std::nullopt;
    return ticks[static_cast<size_t> (at)][static_cast<size_t> (ship)];
  }
};

/** Runs `bars` of `script` on a floor at `level`, pressing `presses` on
 *  their beats. */
Run
play (CueScript const &script, int bars, PilotLevel level, juce::int64 seed,
      FlightBodies bodies = oneGroupAt ({ -0.5f, 0.3f }), std::vector<Press> presses = {})
{
  Floor floor (bodies, seed);
  floor.world.setBreathing (true);
  floor.level = level;
  Run run;
  size_t next = 0;
  auto const until = static_cast<double> (bars) * fourFour;
  while (floor.beats < until - 1e-9)
    {
      auto const bar = static_cast<long long> (std::floor (floor.beats / fourFour + 1e-9));
      auto const now = script (bar);
      while (next < presses.size () && floor.beats + 1e-9 >= presses[next].beat)
        {
          floor.press (presses[next].order, presses[next].ship, now);
          ++next;
        }
      // One tick at a time, so a press lands on the tick it was made on.
      floor.runTo (floor.beats + tick, now, [&] (double after) {
        auto const stepped = after - tick;
        std::vector<Seen> row;
        for (auto s = 0; s < flightShips; ++s)
          {
            auto const game = floor.games.gameOf (s);
            row.push_back ({ stepped, s, game ? game->game : PilotGame::None,
                             game && game->byPilot, floor.games.steerOf (s, stepped, fourFour),
                             floor.at (s) });
          }
        run.ticks.push_back (std::move (row));
      });
    }
  return run;
}

/** A slap's event: the beat its point moves (its opening included). */
struct SlapStep
{
  double beat;
  int ship;
  bool byPilot;
  bool opening;
};

std::vector<SlapStep>
slapSteps (Run const &run)
{
  std::vector<SlapStep> steps;
  std::array<std::optional<Vec2>, flightShips> last{};
  for (auto const &row : run.ticks)
    for (auto const &seen : row)
      {
        auto &before = last[static_cast<size_t> (seen.ship)];
        auto const slapping = seen.game == PilotGame::Slap && seen.steer.has_value ();
        if (slapping && (!before || *before != seen.steer->at))
          steps.push_back ({ seen.beat, seen.ship, seen.byPilot, !before.has_value () });
        before = slapping ? std::optional<Vec2> (seen.steer->at) : std::nullopt;
      }
  return steps;
}

double
beatInBar (double beat)
{
  auto const in = std::fmod (beat, static_cast<double> (fourFour));
  return in < 0. ? in + fourFour : in;
}

bool
onBeat (double beat, double which)
{
  return std::abs (beatInBar (beat) - which) < tick / 2.;
}

std::string
where (juce::int64 seed, double beat)
{
  return "seed " + std::to_string (seed) + ", bar " + std::to_string (beat / fourFour);
}

/** The pilots' slap on `beat`, if one plays then. */
std::optional<Seen>
pilotSlapOn (Run const &run, double beat)
{
  for (auto s = 0; s < flightShips; ++s)
    if (auto const seen = run.on (s, beat);
        seen && seen->byPilot && seen->game == PilotGame::Slap && seen->steer)
      return seen;
  return std::nullopt;
}

Vec2 const theFront{ tuning.slapSideRadius, 0.f };

// A swing through the middle passes r 0 and turns ~180 deg in a beat. On
// these runs (2026-10-17) a hand-back came no nearer than r 0.34 and turned
// at most 30 deg in a beat on the floor (the pass on the real engine,
// smoothed by the heard-turn limit: 0.40 and 26 deg); the bounds leave room.
constexpr float handBackDegreesPerBeat = 36.f;
constexpr float handBackNearest = 0.3f;
}

TEST (PilotsOfflinePass, ThePilotsSlapOnlyOnBeatsOneAndThree)
{
  for (auto const &script : { CueScript (previewedSet), CueScript (liveSet) })
    for (auto const seed : seeds)
      {
        auto const steps = slapSteps (play (script, 64, PilotLevel::Fly, seed));
        ASSERT_FALSE (steps.empty ()) << "the drop was slapped; seed " << seed;
        for (auto const &step : steps)
          EXPECT_TRUE (onBeat (step.beat, 0.) || onBeat (step.beat, 2.))
              << where (seed, step.beat) << ", ship " << step.ship;
      }
}

TEST (PilotsOfflinePass, APressedSlapNeverStepsOnTheBreathsBeat)
{
  // Pressed just before a 1, at beat 2.5, in beat 4, just before beat 3, a
  // pair just before a 1, at beat 1.6, and in beat 3 -- in a drop, the
  // pilots off.
  auto const drop = [] (long long) {
    return cue (MusicSection::Drop, MusicSection::Breakdown, 64, 0, 0.9f);
  };
  std::vector<Press> const presses{ { 2 * 4 - 0.15, 0, autoPress () },
                                    { 6 * 4 + 1.5, 0, autoPress () },
                                    { 10 * 4 + 3.3, 0, autoPress () },
                                    { 14 * 4 + 1.9, 0, autoPress () },
                                    { 18 * 4 - 0.15, 1, named (PilotGame::Slap, PilotRecruit::Nearest) },
                                    { 22 * 4 + 0.6, 2, autoPress () },
                                    { 24 * 4 + 2.6, 3, autoPress () } };
  auto const steps = slapSteps (play (drop, 28, PilotLevel::Off, 1, oneGroupAt ({ -0.5f, 0.3f }),
                                      presses));
  ASSERT_GE (steps.size (), presses.size ());
  for (auto const &step : steps)
    {
      EXPECT_FALSE (onBeat (step.beat, 3.)) << where (1, step.beat) << ", ship " << step.ship;
      if (!step.opening)
        EXPECT_TRUE (onBeat (step.beat, 0.) || onBeat (step.beat, 2.))
            << "after its opening a slap keeps to 1 and 3; " << where (1, step.beat);
    }
  auto const opensOn = [&] (double beat) {
    return std::any_of (steps.begin (), steps.end (), [beat] (SlapStep const &s) {
      return s.opening && std::abs (s.beat - beat) < tick / 2.;
    });
  };
  EXPECT_TRUE (opensOn (2 * 4)) << "pressed just before a 1: opens on the 1";
  EXPECT_TRUE (opensOn (6 * 4 + 2)) << "pressed at beat 2.5: opens on beat 3";
  EXPECT_TRUE (opensOn (11 * 4)) << "pressed in beat 4: waits for the next 1";
  EXPECT_TRUE (opensOn (25 * 4)) << "pressed in beat 3: its next beat is the breath's, so the 1";
}

TEST (PilotsOfflinePass, EveryPreviewedDropIsSlappedOnItsOneWithTheSnap)
{
  struct Case
  {
    std::string what;
    CueScript script;
    int dropBar;
    FlightBodies bodies;
  };
  std::vector<Case> const cases{
    { "out of a climb", previewedSet, 32, oneGroupAt ({ -0.5f, 0.3f }) },
    { "through a pilot's hide", breakdownIntoDrop, 16, FlightBodies{} },
    { "through a pilot's call", grooveIntoDrop, 21, oneGroupAt ({ -0.5f, 0.3f }) },
  };
  for (auto const &c : cases)
    for (auto const seed : seeds)
      {
        auto const run = play (c.script, c.dropBar + 3, PilotLevel::Fly, seed, c.bodies);
        auto const one = static_cast<double> (c.dropBar) * fourFour;
        auto const slap = pilotSlapOn (run, one);
        ASSERT_TRUE (slap.has_value ()) << c.what << ": " << where (seed, one);
        EXPECT_EQ (slap->steer->at, theFront) << c.what << ": the snap; seed " << seed;
      }
}

TEST (PilotsOfflinePass, ThePilotsCallThroughTheDropReallyRuns)
{
  // Guards the case above: the call it is named for must be playing then.
  for (auto const seed : seeds)
    {
      auto const run = play (grooveIntoDrop, 21, PilotLevel::Fly, seed);
      auto calling = false;
      for (auto s = 0; s < flightShips; ++s)
        if (auto const seen = run.on (s, 21. * fourFour - tick); seen && seen->byPilot)
          calling = calling || seen->game == PilotGame::CallAndResponse;
      EXPECT_TRUE (calling) << "seed " << seed << ": a pilot calls into the drop's 1";
    }
}

TEST (PilotsOfflinePass, ADropTheRoomHearsIsSlappedInItsFirstTwoBars)
{
  for (auto const seed : seeds)
    {
      auto const run = play (liveSet, 36, PilotLevel::Fly, seed);
      auto slapped = false;
      for (auto beat = 32. * fourFour; beat < 34. * fourFour; beat += tick)
        slapped = slapped || pilotSlapOn (run, beat).has_value ();
      EXPECT_TRUE (slapped) << "seed " << seed;
    }
}

TEST (PilotsOfflinePass, ADjsFigureIsNotPreemptedByTheDrop)
{
  for (auto const seed : seeds)
    {
      auto const run = play (breakdownIntoDrop, 18, PilotLevel::Fly, seed, FlightBodies{},
                             { { 12 * 4 + 0.5, 0, named (PilotGame::HideAndSeek) } });
      auto const one = 16. * fourFour;
      auto const dj = run.on (0, one);
      ASSERT_TRUE (dj.has_value ());
      EXPECT_EQ (dj->game, PilotGame::HideAndSeek) << "seed " << seed << ": the DJ's hide plays on";
      EXPECT_FALSE (dj->byPilot);
      EXPECT_TRUE (pilotSlapOn (run, one).has_value ())
          << "seed " << seed << ": a pilot slaps the drop with another ship";
    }
}

namespace
{
/** How a ship sounds once a figure hands it back: the nearest it comes to
 *  the middle and the widest it is heard to turn in a beat, over the two
 *  bars after (or until its next figure). */
struct HandBack
{
  double beat;
  int ship;
  PilotGame game;
  bool fromThePole;
  float nearest = 2.f;
  float widestPerBeat = 0.f;
};

float
degreesApart (Pos const &a, Pos const &b)
{
  auto const dot = a.x () * b.x () + a.y () * b.y () + a.z () * b.z ();
  auto const norms = std::sqrt ((a.x () * a.x () + a.y () * a.y () + a.z () * a.z ())
                                * (b.x () * b.x () + b.y () * b.y () + b.z () * b.z ()));
  return std::acos (std::clamp (dot / norms, -1.f, 1.f)) * 180.f / pi<float> ();
}

/** Every hand-back in `run`, heard through the default clip band. */
std::vector<HandBack>
handBacks (Run const &run)
{
  HeightMapSphere sphere;
  ElevationParams const band;
  auto const heard = [&] (Vec2 p) {
    return sphere.mapTo3D (Pos::fromCartesian (p.x, p.y, 0.f), band);
  };
  auto const perBeat = static_cast<size_t> (TempoClock::getTicksPerBeat ());
  auto const window = static_cast<size_t> (2 * fourFour) * perBeat;
  std::vector<HandBack> out;
  for (size_t t = 1; t < run.ticks.size (); ++t)
    for (auto s = 0; s < flightShips; ++s)
      {
        auto const &before = run.ticks[t - 1][static_cast<size_t> (s)];
        auto const &now = run.ticks[t][static_cast<size_t> (s)];
        if (before.game == PilotGame::None || now.game != PilotGame::None)
          continue;
        HandBack back{ now.beat, s, before.game,
                       before.steer && before.steer->at.getDistanceFromOrigin () < 0.1f };
        for (auto u = t; u < std::min (run.ticks.size (), t + window); ++u)
          {
            auto const &seen = run.ticks[u][static_cast<size_t> (s)];
            if (seen.game != PilotGame::None)
              break;
            back.nearest = std::min (back.nearest, seen.at.getDistanceFromOrigin ());
            if (u >= perBeat)
              back.widestPerBeat = std::max (
                  back.widestPerBeat,
                  degreesApart (heard (run.ticks[u - perBeat][static_cast<size_t> (s)].at),
                                heard (seen.at)));
          }
        out.push_back (back);
      }
  return out;
}

/** The runs a hand-back is checked on: the pilots through both sets, and
 *  every figure pressed, the old four included. */
std::vector<Run>
handBackRuns ()
{
  std::vector<Run> runs;
  for (auto const seed : seeds)
    {
      runs.push_back (play (previewedSet, 64, PilotLevel::Fly, seed));
      runs.push_back (play (liveSet, 56, PilotLevel::Fly, seed));
    }
  auto const groove = [] (long long bar) {
    return cue (MusicSection::Groove, MusicSection::Build, (bar / 8 + 1) * 8, 0, 0.5f);
  };
  runs.push_back (play (groove, 40, PilotLevel::Off, 1, oneGroupAt ({ -0.5f, 0.3f }),
                        { { 2 * 4 + 0.5, 0, autoPress () },
                          { 12 * 4 + 0.5, 2, named (PilotGame::FakeOut) },
                          { 20 * 4 + 0.5, 3, named (PilotGame::HideAndSeek) },
                          { 28 * 4 + 0.5, 0, named (PilotGame::Formation, PilotRecruit::Nearest) } }));
  auto const breakdown = [] (long long) {
    return cue (MusicSection::Breakdown, MusicSection::Build, 64, 0, 0.4f);
  };
  runs.push_back (play (breakdown, 20, PilotLevel::Off, 1, oneGroupAt ({ -0.5f, 0.3f }),
                        { { 2 * 4 + 0.5, 0, autoPress () },
                          { 8 * 4 + 0.5, 1, named (PilotGame::Encircle) } }));
  return runs;
}
}

TEST (PilotsOfflinePass, AShipHandedBackCruisesOnWithoutASwingThroughTheMiddle)
{
  auto count = 0;
  for (auto const &run : handBackRuns ())
    for (auto const &back : handBacks (run))
      {
        ++count;
        auto const what = "ship " + std::to_string (back.ship) + " out of game "
                          + std::to_string (static_cast<int> (back.game)) + " at bar "
                          + std::to_string (back.beat / fourFour);
        EXPECT_LE (back.widestPerBeat, handBackDegreesPerBeat) << what;
        if (back.fromThePole)
          continue; // an up-slap ends overhead by design
        EXPECT_GE (back.nearest, handBackNearest) << what;
      }
  EXPECT_GE (count, 20) << "the runs hand back every kind of figure";
}

TEST (PilotsOfflinePass, OverASetThePilotsKeepToTheirBudget)
{
  for (auto const &script : { CueScript (previewedSet), CueScript (liveSet) })
    for (auto const seed : seeds)
      {
        auto const run = play (script, 64, PilotLevel::Fly, seed);
        auto mostInFigures = 0;
        std::optional<long long> endedOnBar;
        auto pilotsBefore = false;
        auto pilotFigures = 0;
        for (auto const &row : run.ticks)
          {
            auto inFigures = 0;
            auto pilots = false;
            for (auto const &seen : row)
              {
                inFigures += seen.steer ? 1 : 0;
                pilots = pilots || seen.byPilot;
              }
            mostInFigures = std::max (mostInFigures, inFigures);
            auto const bar = static_cast<long long> (std::floor (row.front ().beat / fourFour));
            if (pilots && !pilotsBefore)
              {
                ++pilotFigures;
                if (endedOnBar)
                  EXPECT_GE (bar, *endedOnBar + tuning.restBars)
                      << "the pilots rest after a figure; " << where (seed, row.front ().beat);
              }
            if (pilotsBefore && !pilots)
              endedOnBar = bar;
            pilotsBefore = pilots;
          }
        EXPECT_LE (mostInFigures, 2) << "two ships move, the rest wait; seed " << seed;
        EXPECT_GE (pilotFigures, 3) << "the pilots play through the set; seed " << seed;

        std::array<std::optional<double>, flightShips> lastStep{};
        for (auto const &step : slapSteps (run))
          {
            auto &last = lastStep[static_cast<size_t> (step.ship)];
            if (last && !step.opening)
              EXPECT_GE (step.beat - *last, tuning.eventSpacingBeats - tick / 2.)
                  << where (seed, step.beat) << ", ship " << step.ship;
            last = step.beat;
          }
      }
}
