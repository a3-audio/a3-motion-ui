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

#include <a3-motion-engine/preview/MusicCue.hh>

#include <array>

namespace a3
{

/** What the live mood listens for. Ratios of a bar's level to the bars
 *  before it, so the master's level does not matter [guess, all of them:
 *  listen on the rig]. */
struct MoodTuning
{
  int historyBars = 4;        // a bar is held against the mean of this many before it
  float dropRise = 1.5f;      // this much louder: the drop
  float breakdownFall = 0.5f; // this much quieter: a breakdown
  float buildStep = 1.05f;    // each bar at least this much louder than the last...
  int buildBars = 3;          // ...this many times over: a build
  int phraseBars = 8;         // a change is expected on the next 8-bar line; a build or
                              // breakdown that sees none by then was just how it plays
  int dropBars = 8;           // a drop is a groove again after this long
  float quiet = 0.02f;        // quieter bars say nothing (a stop, a gap)
};

/** A bar's level from the channel meters: the loudest of the four channels'
 *  mean RMS over the bar. Message thread only. */
class BarMeter
{
public:
  static constexpr int channels = 4;

  /** One meter reading of `channel` (0-3). Other channels, and a level that
   *  is not finite or below 0, are not heard. */
  void hear (int channel, float rms);
  /** The bar's level, 0 for a bar with nothing heard; the next bar starts
   *  empty. */
  float closeBar ();

private:
  std::array<double, channels> _sum{};
  std::array<int, channels> _count{};
};

/** The music's section from the rise and fall of bar levels: the pilots'
 *  timing when StemDeck says nothing. Pure and deterministic; message thread
 *  only. */
class LiveMood
{
public:
  explicit LiveMood (MoodTuning const &tuning = {});

  /** Bar `bar` is over and its level was `energy`. Bars come in order; one
   *  out of order (the clock jumped) starts afresh. A level that is not
   *  finite or below 0 is not heard. */
  void addBar (long long bar, float energy);
  /** What the bars so far say, for the bar now running. */
  MusicCue cue () const;

private:
  static constexpr int kept = 8;

  /** historyBars, as many as are kept before the newest. */
  int history () const;
  void push (float energy);
  float newest () const;
  /** The mean of the historyBars bars before the newest. */
  float before () const;
  bool rising () const;
  /** How long the current section lasts without a change of its own. */
  int dwell () const;
  void enter (MusicSection section, long long since);

  MoodTuning _tuning;
  std::array<float, kept> _levels{}; // the newest last
  int _count = 0;
  long long _lastBar = -1;
  MusicSection _section = MusicSection::Groove;
  long long _since = 0;
};

}
