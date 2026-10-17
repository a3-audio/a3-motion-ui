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

#pragma once

#include <a3-motion-engine/PilotOrder.hh>
#include <a3-motion-engine/flight/BaseOrbit.hh>
#include <a3-motion-engine/flight/BeatPulse.hh>
#include <a3-motion-engine/flight/FlightWorld.hh>
#include <a3-motion-engine/flight/PilotGames.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>

#include <JuceHeader.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

// The flight floor with the games on it, stepped tick by tick the way the
// engine's clock thread steps them, without the clock: the same script runs
// the same way every time, in milliseconds.

namespace a3::gamefloor
{
constexpr int fourFour = 4;

inline FlightBodies
oneGroupAt (Vec2 at)
{
  FlightBodies bodies;
  bodies.count = 1;
  bodies.body[0] = { at, FlightTuning{}.groupMass, 0 };
  return bodies;
}

inline FlightBodies
oneGroup ()
{
  return oneGroupAt ({ -0.5f, 0.3f });
}

/** The clock's place at `beats`, as the engine hands it to the pulse. */
inline Measure
measureAt (double beats)
{
  auto const perBeat = TempoClock::getTicksPerBeat ();
  auto const ticks = static_cast<int> (std::llround (beats * perBeat));
  auto const wholeBeats = ticks / perBeat;
  return { wholeBeats / fourFour, wholeBeats % fourFour, ticks % perBeat };
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

  /** A press as `ActionReach` sends it: the whole order, on `leader`'s ship. */
  bool
  press (PilotOrder const &order, int leader, MusicCue const &cue)
  {
    GameRequest request;
    request.order = order;
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
        games.step (ships (), bodies, cue, level, beats, fourFour);
        std::array<ShipOrders, flightShips> orders{};
        for (auto ch = 0; ch < flightShips; ++ch)
          {
            auto &order = orders[static_cast<size_t> (ch)];
            order.flying = true;
            if (auto const goal = games.steerOf (ch, beats, fourFour))
              {
                auto const stance = games.stanceOf (ch, beats, fourFour);
                order.goal = FlightGoal::Steer;
                order.steer = *goal;
                order.bodyId = games.gameOf (ch)->target;
                order.hold = stance.hold;
                order.breathes = stance.breathes;
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

  /** Steps to beat `until`, each bar under the cue `cueOf` gives it. */
  void
  runScript (double until, std::function<MusicCue (long long)> const &cueOf,
             std::function<void (double)> const &look = {})
  {
    while (beats < until - 1e-9)
      {
        auto const bar = static_cast<long long> (std::floor (beats / fourFour + 1e-9));
        runTo (std::min (until, static_cast<double> (bar + 1) * fourFour), cueOf (bar), look);
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
  PilotLevel level = PilotLevel::Off;
  double beats = 0.;
};

}
