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

#pragma once

#include <a3-motion-engine/RecMode.hh>

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace a3
{

/** The Motion and Elevation knobs a take records as lanes (REC 3c,
 *  2026-09-26). The order is the file's and never changes: a lane is found by
 *  its name, and the names are stored. */
enum class Knob
{
  Rotate,
  Spin,
  Reach,
  Swell,
  SqueezeX,
  StretchX,
  SqueezeY,
  StretchY,
  ClipBottom,
  ClipTop,
  Sway,
  Elevation,
  Tilt,
  TiltSweep,
  Roll,
  RollSweep,
};
constexpr int numKnobs = 16;

/** The knobs a take's band is made of: where the figure sits and how far it
 *  reaches, the two clips and the two sweeps that move them. A take records
 *  over the whole sphere, so these are still from its start until it is saved
 *  or discarded, and none of them gets a lane (maintainer, 2026-10-08). */
constexpr bool
isTakeBandKnob (Knob knob)
{
  switch (knob)
    {
    case Knob::Elevation:
    case Knob::Reach:
    case Knob::ClipTop:
    case Knob::ClipBottom:
    case Knob::Sway:
    case Knob::Swell: return true;
    default: return false;
    }
}

/** The name a clip file stores a knob's lane under -- the knob's caption. */
constexpr char const *
knobName (Knob knob)
{
  switch (knob)
    {
    case Knob::Rotate: return "rot";
    case Knob::Spin: return "spin";
    case Knob::Reach: return "reach";
    case Knob::Swell: return "swell";
    case Knob::SqueezeX: return "sqzX";
    case Knob::StretchX: return "strX";
    case Knob::SqueezeY: return "sqzY";
    case Knob::StretchY: return "strY";
    case Knob::ClipBottom: return "clip-bot";
    case Knob::ClipTop: return "clip-top";
    case Knob::Sway: return "sway";
    case Knob::Elevation: return "elv";
    case Knob::Tilt: return "tilt";
    case Knob::TiltSweep: return "tswp";
    case Knob::Roll: return "roll";
    case Knob::RollSweep: return "rswp";
    }
  return "";
}

/** One knob's lane: a value per tick of the take, or nothing where no pass
 *  wrote one. Played back as a step: between written ticks the last value
 *  holds, round the take's end too -- a knob turned and let go stays where it
 *  was left, as it does under the hand. */
class KnobLane
{
public:
  explicit KnobLane (long long ticks = 0);

  bool empty () const;
  long long ticks () const { return static_cast<long long> (_values.size ()); }

  void write (long long tick, float value);

  /** The value at a play position, in ticks; nothing on an empty lane. */
  std::optional<float> at (double tick) const;

  /** Only the ticks where the value changes, for the file: a knob held still
   *  for a whole pass is one point, not a pass's worth. */
  std::vector<std::pair<int, float> > changePoints () const;
  static KnobLane fromChangePoints (std::vector<std::pair<int, float> > const &,
                                    long long ticks);

private:
  /** The last tick written at or before `tick`, without wrapping; -1 for
   *  none. */
  long long lastWrittenAtOrBefore (long long tick) const;

  std::vector<float> _values;
  // Which ticks are written: a bit each, and a bit per word of those for
  // whether it has any. at() finds the last value written in a few word
  // reads rather than by walking back -- a lane from a file holds only where
  // it changes, and a walk back over a 64-bar take at every tick took 47 s
  // on the clock thread (2026-10-08).
  std::vector<std::uint64_t> _written;
  std::vector<std::uint64_t> _wordsWritten;
};

/** Writes one knob into its lane during a take, by the rule the path is
 *  written by (shouldWriteTick): Touch while the hand holds it, Latch from the
 *  touch to the end of the lap it was let go in, Write the whole pass. The
 *  knob's value is written as it stands -- let go, it stays where it was
 *  left, which is the held value Latch and Write carry on with.
 *
 *  One per knob and take: what it remembers -- whether the hand has been on
 *  it, where it was let go -- is about this take only. */
class KnobRecorder
{
public:
  /** `ticksNow` counts from the start of the take, across laps; the lane is
   *  written at its place in the lap. */
  /** @returns whether this tick was written. */
  bool recordTick (KnobLane &lane, RecMode mode, bool held, float value,
                   long long ticksNow, long long lapTicks);

private:
  bool _wasHeld = false;
  bool _hasTouched = false;
  long long _ticksAtLift = 0;
};

/** A clip's lanes, one per knob in Knob's order. */
using KnobLanes = std::array<KnobLane, numKnobs>;
using KnobRecorders = std::array<KnobRecorder, numKnobs>;

}
