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

// The offline pass as a tool, not a test: the real engine on the real clock
// at 128 BPM, a scripted set of music cues and presses, and every channel's
// heard position written per tick to CSV, plus what the backend was handed
// to send -- the plots of smoke-test/pilots-contrasts.md come from here.
// Nothing leaves the process: the backend records, it does not send. Every
// scenario runs in real time (up to 2 minutes), so all are DISABLED_; the
// same scenarios are asserted, fast and deterministic, in PilotsOfflinePass.
//
//   A3_PASS_OUT=<dir> a3-motion-tests --gtest_also_run_disabled_tests \
//       --gtest_filter='PilotsOfflinePassTool.*'

#include "WaitUntil.hh"

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/PilotOrder.hh>
#include <a3-motion-engine/backends/SpatBackend.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>
#include <a3-motion-engine/flight/PilotGames.hh>
#include <a3-motion-engine/preview/MusicCue.hh>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

using namespace a3;

namespace
{
constexpr float passBpm = 128.f;
constexpr int channels = 4;
using Clock = std::chrono::steady_clock;

struct Sent
{
  double seconds;
  int channel;
  float azimuth, elevation;
};

class RecordingBackend : public SpatBackend
{
public:
  void
  sendPosition (index_t channel, Pos const &pos) override
  {
    std::lock_guard<std::mutex> lock (mutex);
    sent.push_back ({ std::chrono::duration<double> (Clock::now () - start).count (),
                      static_cast<int> (channel), pos.azimuth (), pos.elevation () });
  }
  void sendPot1 (index_t, float) override {}
  void sendPot2 (index_t, float) override {}
  void sendPot3 (index_t, float) override {}

  std::mutex mutex;
  std::vector<Sent> sent;
  Clock::time_point start = Clock::now ();

protected:
  void addressesChanged (OscAddresses const &) override {}
};

struct Row
{
  long long tick;
  double seconds;
  std::array<Pos, channels> heard;
  std::array<int, channels> game;
  std::array<int, channels> byPilot;
  std::array<int, channels> leader;
  std::array<int, channels> mode;
};

std::shared_ptr<Pattern>
clipStandingAt (index_t channel, float x, float y)
{
  auto pattern = std::make_shared<Pattern> ();
  auto const ticks = static_cast<index_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (ticks);
  for (index_t tick = 0; tick < ticks; ++tick)
    pattern->setTick (tick, Pos::fromCartesian (x, y, 0.f));
  pattern->markComplete ();
  pattern->setChannel (channel);
  pattern->setEndAction (EndAction::Loop);
  pattern->setPlaybackLength (Measure{ 0, 1, 0 });
  return pattern;
}

FlightBodies
oneGroupAt (float x, float y)
{
  FlightBodies bodies;
  bodies.count = 1;
  bodies.body[0] = { { x, y }, 1.f, 0 };
  return bodies;
}

/** A press the script makes: at `beat` (absolute), on `channel`. */
struct Press
{
  double beat;
  int channel;
  PilotOrder order;
  std::string label;
  /** Optional: choose the channel at press time (e.g. the climbing one). */
  std::function<int (MotionEngine &)> pick;
};

/** Something the script does to the floor at `beat`. */
struct Change
{
  double beat;
  std::function<void (MotionEngine &)> apply;
  std::string label;
};

struct Scenario
{
  std::string name;
  int bars = 16;
  PilotLevel level = PilotLevel::Off;
  std::array<FlightMode, channels> modes{ FlightMode::Orbit, FlightMode::Orbit,
                                          FlightMode::Orbit, FlightMode::Orbit };
  FlightBodies bodies = oneGroupAt (-0.5f, 0.3f);
  std::function<MusicCue (long long)> cueOf;
  std::vector<Press> presses;
  std::vector<Change> changes;
};

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

std::string
outDir ()
{
  auto const *env = std::getenv ("A3_PASS_OUT");
  return env ? env : "/tmp";
}

void
run (Scenario const &scenario)
{
  auto owned = std::make_unique<RecordingBackend> ();
  auto *backend = owned.get ();
  HeightMapSphere heightMap;
  auto engine = std::make_unique<MotionEngine> (channels, heightMap, std::move (owned));
  std::array<std::shared_ptr<Pattern>, channels> clips{
    clipStandingAt (0, 0.5f, 0.f), clipStandingAt (1, 0.f, 0.5f),
    clipStandingAt (2, -0.5f, 0.f), clipStandingAt (3, 0.f, -0.5f)
  };
  for (index_t ch = 0; ch < channels; ++ch)
    engine->setPreviewMode (ch, false);
  engine->setTempoBPM (passBpm);
  engine->setFlightBreath (true);
  engine->setFlightBodies (scenario.bodies);
  for (auto &clip : clips)
    engine->playPattern (clip, Measure{});
  ASSERT_TRUE (waitUntil ([&] {
    for (auto &clip : clips)
      if (clip->getStatus () != Pattern::Status::Playing)
        return false;
    return true;
  }));

  std::mutex mutex;
  std::vector<Row> rows;
  rows.reserve (static_cast<size_t> (scenario.bars + 8) * 4 * 128);
  auto const start = Clock::now ();
  long long firstTick = -1;
  auto handle = engine->getTempoClock ().scheduleEventHandlerAddition (
      [&] (Measure measure) {
        Row row;
        row.tick = Measure::convertToTicks (measure, engine->getBeatsPerBar ());
        row.seconds = std::chrono::duration<double> (Clock::now () - start).count ();
        for (auto ch = 0; ch < channels; ++ch)
          {
            auto const s = static_cast<size_t> (ch);
            row.heard[s] = engine->getChannelPosition (static_cast<index_t> (ch));
            auto const game = engine->gameOf (static_cast<index_t> (ch));
            row.game[s] = game ? static_cast<int> (game->game) : 0;
            row.byPilot[s] = game && game->byPilot ? 1 : 0;
            row.leader[s] = game ? game->leader : -1;
            row.mode[s] = static_cast<int> (engine->getFlightMode (static_cast<index_t> (ch)));
          }
        std::lock_guard<std::mutex> lock (mutex);
        if (rows.size () < rows.capacity ())
          rows.push_back (row);
      },
      TempoClock::Event::Tick, TempoClock::Execution::TimerThread);

  auto const lastTick = [&] {
    std::lock_guard<std::mutex> lock (mutex);
    return rows.empty () ? -1LL : rows.back ().tick;
  };
  ASSERT_TRUE (waitUntil ([&] { return lastTick () >= 0; }));
  auto const perBeat = static_cast<double> (TempoClock::getTicksPerBeat ());
  auto const perBar = perBeat * engine->getBeatsPerBar ();
  // Start the script on the next downbeat so its bar 0 is a real 1.
  auto const origin = (static_cast<long long> (lastTick () / perBar) + 1)
                      * static_cast<long long> (perBar);
  firstTick = origin;
  // The script counts bars from its own start; the engine's cue counts the
  // clock's bars.
  auto const originBar = static_cast<long long> (origin / static_cast<long long> (perBar));
  auto const engineCue = [&] (long long bar) {
    auto c = scenario.cueOf (bar);
    if (c.changeBar >= 0)
      c.changeBar += originBar;
    if (c.sectionStartBar >= 0)
      c.sectionStartBar += originBar;
    return c;
  };
  for (auto ch = 0; ch < channels; ++ch)
    engine->setFlightMode (static_cast<index_t> (ch), scenario.modes[static_cast<size_t> (ch)]);
  engine->setMusicCue (engineCue (0));
  ASSERT_TRUE (waitUntil ([&] { return lastTick () >= origin - 64; }, 4000, 1));
  engine->setPilotLevel (scenario.level);

  std::ofstream events (outDir () + "/" + scenario.name + "-events.csv");
  events << "beat,kind,channel,label\n";
  long long cueBar = -1;
  size_t nextPress = 0, nextChange = 0;
  auto const endTick = origin + static_cast<long long> (scenario.bars * perBar);
  while (true)
    {
      auto const tick = lastTick ();
      if (tick >= endTick)
        break;
      auto const beat = (static_cast<double> (tick - origin)) / perBeat;
      auto const bar = static_cast<long long> (std::floor (beat / engine->getBeatsPerBar ()));
      if (bar != cueBar && bar >= 0)
        {
          auto const c = engineCue (bar);
          engine->setMusicCue (c);
          events << beat << ",cue,-1,section" << static_cast<int> (c.section) << "_next"
                 << (c.next ? static_cast<int> (*c.next) : -1) << "_change" << c.changeBar
                 << "_prev" << c.previewed << "\n";
          cueBar = bar;
        }
      while (nextChange < scenario.changes.size () && beat >= scenario.changes[nextChange].beat)
        {
          scenario.changes[nextChange].apply (*engine);
          events << beat << ",change,-1," << scenario.changes[nextChange].label << "\n";
          ++nextChange;
        }
      while (nextPress < scenario.presses.size () && beat >= scenario.presses[nextPress].beat)
        {
          auto const &press = scenario.presses[nextPress];
          auto const ch = press.pick ? press.pick (*engine) : press.channel;
          if (ch >= 0)
            engine->requestGame (static_cast<index_t> (ch), press.order);
          events << beat << ",press," << ch << "," << press.label << "\n";
          ++nextPress;
        }
      juce::Thread::sleep (1);
    }
  handle.reset ();

  std::vector<Row> copy;
  {
    std::lock_guard<std::mutex> lock (mutex);
    copy = rows;
  }
  std::ofstream out (outDir () + "/" + scenario.name + "-ticks.csv");
  out << "beat,seconds,ch,az,el,fx,fy,game,byPilot,leader,mode\n";
  for (auto const &row : copy)
    {
      if (row.tick < firstTick - static_cast<long long> (perBar))
        continue;
      auto const beat = static_cast<double> (row.tick - firstTick) / perBeat;
      for (auto ch = 0; ch < channels; ++ch)
        {
          auto const s = static_cast<size_t> (ch);
          auto const floor = heightMap.mapTo2D (row.heard[s], clips[s]->getElevationParams ());
          out << beat << "," << row.seconds << "," << ch << "," << row.heard[s].azimuth () << ","
              << row.heard[s].elevation () << "," << floor.x () << "," << floor.y () << ","
              << row.game[s] << "," << row.byPilot[s] << "," << row.leader[s] << ","
              << row.mode[s] << "\n";
        }
    }
  std::ofstream sent (outDir () + "/" + scenario.name + "-sent.csv");
  sent << "seconds,ch,az,el\n";
  std::lock_guard<std::mutex> lock (backend->mutex);
  for (auto const &s : backend->sent)
    sent << s.seconds << "," << s.channel << "," << s.azimuth << "," << s.elevation << "\n";
  EXPECT_GT (copy.size (), 100u);
}

/** 64 bars as StemDeck previews them (as PilotGamesFlight's scriptedSet). */
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

/** The same set as the live mood names it: a build three bars after it
 *  starts rising, a drop one bar after its 1, a breakdown one bar late. */
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

int
climbingPilot (MotionEngine &engine)
{
  for (auto ch = 0; ch < channels; ++ch)
    if (auto const g = engine.gameOf (static_cast<index_t> (ch));
        g && g->byPilot && g->game == PilotGame::Climb)
      return ch;
  return -1;
}
}

TEST (PilotsOfflinePassTool, DISABLED_FlyPreviewedSet)
{
  Scenario s;
  s.name = "fly-previewed";
  s.bars = 64;
  s.level = PilotLevel::Fly;
  s.cueOf = previewedSet;
  run (s);
}

TEST (PilotsOfflinePassTool, DISABLED_FlyLiveSet)
{
  Scenario s;
  s.name = "fly-live";
  s.bars = 56;
  s.level = PilotLevel::Fly;
  s.cueOf = liveSet;
  run (s);
}

TEST (PilotsOfflinePassTool, DISABLED_PressesInADrop)
{
  Scenario s;
  s.name = "press-drop";
  s.bars = 26;
  s.cueOf = [] (long long) { return cue (MusicSection::Drop, MusicSection::Breakdown, 64, 0, 0.9f); };
  // on the 1 (pressed just before it), mid-bar (beat 2.5), in beat 4, on beat 3
  s.presses = { { 2 * 4 - 0.15, 0, autoPress (), "auto-just-before-1" },
                { 6 * 4 + 1.5, 0, autoPress (), "auto-mid-bar-beat2.5" },
                { 10 * 4 + 3.3, 0, autoPress (), "auto-in-beat4" },
                { 14 * 4 + 1.9, 0, autoPress (), "auto-just-before-beat3" },
                { 18 * 4 - 0.15, 1, named (PilotGame::Slap, PilotRecruit::Nearest),
                  "slap-with-nearest-before-1" },
                { 22 * 4 + 0.6, 2, autoPress (), "auto-ch2-beat1.6" } };
  run (s);
}

TEST (PilotsOfflinePassTool, DISABLED_PressInABuild)
{
  Scenario s;
  s.name = "press-build";
  s.bars = 24;
  s.cueOf = [] (long long bar) {
    return bar < 16 ? cue (MusicSection::Build, MusicSection::Drop, 16, 0, 0.6f)
                    : cue (MusicSection::Drop, MusicSection::Breakdown, 32, 16, 0.9f);
  };
  s.presses = { { 4 * 4 + 1.4, 0, autoPress (), "auto-11.5-bars-before-drop" },
                { 11 * 4 + 1.4, 2, autoPress (), "auto-4.5-bars-before-drop" } };
  run (s);
}

TEST (PilotsOfflinePassTool, DISABLED_PressInALoopedBuild)
{
  Scenario s;
  s.name = "press-build-looped";
  s.bars = 32;
  // StemDeck saw the drop at 16; the DJ loops the build at bar 12, so from
  // then the drop is always 8 bars away.
  s.cueOf = [] (long long bar) {
    return bar < 12 ? cue (MusicSection::Build, MusicSection::Drop, 16, 0, 0.6f)
                    : cue (MusicSection::Build, MusicSection::Drop, bar + 8, 0, 0.6f);
  };
  s.presses = { { 2 * 4 + 1.4, 0, autoPress (), "auto-climb" } };
  run (s);
}

TEST (PilotsOfflinePassTool, DISABLED_PressesInABreakdown)
{
  Scenario s;
  s.name = "press-breakdown";
  s.bars = 26;
  s.cueOf = [] (long long) {
    return cue (MusicSection::Breakdown, MusicSection::Build, 64, 0, 0.4f);
  };
  s.presses = { { 2 * 4 + 0.5, 0, autoPress (), "auto-graze" },
                { 8 * 4 + 0.5, 1, named (PilotGame::Encircle), "encircle" },
                { 18 * 4 + 0.5, 2, autoPress (), "auto-no-group" } };
  s.changes = { { 16 * 4, [] (MotionEngine &e) { e.setFlightBodies (FlightBodies{}); },
                  "group-removed" } };
  run (s);
}

TEST (PilotsOfflinePassTool, DISABLED_PressesInAGrooveAndTheOldGames)
{
  Scenario s;
  s.name = "press-groove";
  s.bars = 40;
  s.modes = { FlightMode::Orbit, FlightMode::Clip, FlightMode::Orbit, FlightMode::Orbit };
  s.cueOf = [] (long long bar) {
    return cue (MusicSection::Groove, MusicSection::Build, (bar / 8 + 1) * 8, 0, 0.5f);
  };
  s.presses = { { 2 * 4 + 0.5, 0, autoPress (), "auto-groove-call-response" },
                { 12 * 4 + 0.5, 2, named (PilotGame::FakeOut), "fakeout" },
                { 20 * 4 + 0.5, 3, named (PilotGame::HideAndSeek), "hideseek" },
                { 28 * 4 + 0.5, 0, named (PilotGame::Formation, PilotRecruit::Nearest),
                  "formation-nearest" } };
  run (s);
}

TEST (PilotsOfflinePassTool, DISABLED_PressAtFlyTakesOnlyItsShip)
{
  Scenario s;
  s.name = "fly-takeover";
  s.bars = 24;
  s.level = PilotLevel::Fly;
  s.cueOf = [] (long long bar) {
    return bar < 16 ? cue (MusicSection::Build, MusicSection::Drop, 16, 0, 0.6f)
                    : cue (MusicSection::Drop, MusicSection::Breakdown, 32, 16, 0.9f);
  };
  Press take{ 9 * 4 + 1.4, -1, autoPress (), "auto-on-the-climbing-ship", climbingPilot };
  s.presses = { take };
  run (s);
}

namespace
{
/** A breakdown StemDeck says ends at bar 16; the drop the room hears comes
 *  at `heardDrop`, and only then does the preview say "drop" (its section
 *  start, as the UI's withSectionStart works it out, is the announced bar). */
MusicCue
breakdownIntoDrop (long long bar, long long heardDrop)
{
  if (bar < heardDrop)
    return cue (MusicSection::Breakdown, MusicSection::Drop, 16, 0, 0.4f);
  return cue (MusicSection::Drop, MusicSection::Groove, 32, 16, 0.9f);
}
}

TEST (PilotsOfflinePassTool, DISABLED_FlyDropOnTimeNoGroup)
{
  Scenario s;
  s.name = "fly-drop-on-time";
  s.bars = 24;
  s.level = PilotLevel::Fly;
  s.bodies = FlightBodies{};
  s.cueOf = [] (long long bar) { return breakdownIntoDrop (bar, 16); };
  run (s);
}

TEST (PilotsOfflinePassTool, DISABLED_FlyDropLateNoGroup)
{
  Scenario s;
  s.name = "fly-drop-late";
  s.bars = 24;
  s.level = PilotLevel::Fly;
  s.bodies = FlightBodies{};
  s.cueOf = [] (long long bar) { return breakdownIntoDrop (bar, 20); };
  run (s);
}

TEST (PilotsOfflinePassTool, DISABLED_FlyDropOnTimeWithGroup)
{
  Scenario s;
  s.name = "fly-drop-on-time-group";
  s.bars = 24;
  s.level = PilotLevel::Fly;
  s.cueOf = [] (long long bar) { return breakdownIntoDrop (bar, 16); };
  run (s);
}

/** The heard arcs of a slap's points through a few clip bands. */
TEST (PilotsOfflinePassTool, DISABLED_SlapArcsPerBand)
{
  HeightMapSphere heightMap;
  auto const side = GameTuning{}.slapSideRadius;
  std::ofstream out (outDir () + "/slap-arcs-per-band.csv");
  out << "reach,base,front_el,side_el,LR_arc,FB_arc,UD_arc,pole_el\n";
  for (auto const base : { 0.f, 0.25f, 0.5f })
    for (auto const reach : { 0.3f, 0.5f, 0.7f, 1.0f, 2.6f })
      {
        ElevationParams params;
        params.reach = reach;
        params.elevationBase = base;
        auto const at = [&] (float x, float y) {
          return heightMap.mapTo3D (Pos::fromCartesian (x, y, 0.f), params);
        };
        auto const arc = [] (Pos const &a, Pos const &b) {
          auto const d = a.x () * b.x () + a.y () * b.y () + a.z () * b.z ();
          auto const n = std::sqrt ((a.x () * a.x () + a.y () * a.y () + a.z () * a.z ())
                                    * (b.x () * b.x () + b.y () * b.y () + b.z () * b.z ()));
          return std::acos (std::clamp (d / n, -1.f, 1.f)) * 180.f / 3.14159265f;
        };
        auto const front = at (side, 0.f), back = at (-side, 0.f);
        auto const left = at (0.f, side), right = at (0.f, -side);
        auto const pole = at (0.f, 0.f);
        out << reach << "," << base << "," << front.elevation () << "," << left.elevation () << ","
            << arc (left, right) << "," << arc (front, back) << "," << arc (pole, front) << ","
            << pole.elevation () << "\n";
      }
}

/** A G group (the floor's lightest, mass 0.5) at 0.58 from the middle. */
TEST (PilotsOfflinePassTool, DISABLED_GrazeAGGroup)
{
  Scenario s;
  s.name = "graze-g-group";
  s.bars = 20;
  s.bodies = oneGroupAt (-0.5f, 0.3f);
  s.bodies.body[0].mass = FlightTuning{}.groupMass;
  s.cueOf = [] (long long) {
    return cue (MusicSection::Breakdown, MusicSection::Build, 64, 0, 0.4f);
  };
  s.presses = { { 2 * 4 + 0.5, 0, autoPress (), "auto-graze-1" },
                { 8 * 4 + 0.5, 2, autoPress (), "auto-graze-2" },
                { 14 * 4 + 0.5, 1, named (PilotGame::Graze), "graze-named" } };
  run (s);
}
