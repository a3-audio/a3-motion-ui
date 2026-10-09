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

#include "LiveMood.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
bool
heard (float level)
{
  return std::isfinite (level) && level >= 0.f;
}
}

void
BarMeter::hear (int channel, float rms)
{
  if (channel < 0 || channel >= channels || !heard (rms))
    return;
  _sum[static_cast<size_t> (channel)] += rms;
  ++_count[static_cast<size_t> (channel)];
}

float
BarMeter::closeBar ()
{
  auto loudest = 0.;
  for (size_t ch = 0; ch < _sum.size (); ++ch)
    if (_count[ch] > 0)
      loudest = std::max (loudest, _sum[ch] / _count[ch]);
  _sum = {};
  _count = {};
  return static_cast<float> (loudest);
}

LiveMood::LiveMood (MoodTuning const &tuning) : _tuning (tuning) {}

void
LiveMood::push (float energy)
{
  std::rotate (_levels.begin (), _levels.begin () + 1, _levels.end ());
  _levels.back () = energy;
  _count = std::min (_count + 1, kept);
}

float
LiveMood::newest () const
{
  return _count > 0 ? _levels.back () : 0.f;
}

float
LiveMood::before () const
{
  auto const n = std::min (_tuning.historyBars, _count - 1);
  if (n <= 0)
    return 0.f;
  auto sum = 0.f;
  for (auto i = 0; i < n; ++i)
    sum += _levels[static_cast<size_t> (kept - 2 - i)];
  return sum / static_cast<float> (n);
}

bool
LiveMood::rising () const
{
  if (_count < _tuning.buildBars + 1 || _tuning.buildBars + 1 > kept)
    return false;
  for (auto i = 0; i < _tuning.buildBars; ++i)
    {
      auto const later = _levels[static_cast<size_t> (kept - 1 - i)];
      auto const earlier = _levels[static_cast<size_t> (kept - 2 - i)];
      if (later < _tuning.buildStep * earlier || later < _tuning.quiet)
        return false;
    }
  return true;
}

void
LiveMood::enter (MusicSection section, long long since)
{
  _section = section;
  _since = since;
}

void
LiveMood::addBar (long long bar, float energy)
{
  if (!heard (energy))
    return;
  if (_count > 0 && bar != _lastBar + 1)
    {
      _count = 0;
      enter (MusicSection::Groove, bar);
    }
  push (energy);
  _lastBar = bar;

  if (_section == MusicSection::Drop && bar - _since + 1 >= _tuning.dropBars)
    enter (MusicSection::Groove, bar + 1);

  if (_count <= _tuning.historyBars)
    return;
  auto const earlier = before ();
  if (earlier < _tuning.quiet && energy < _tuning.quiet)
    return;

  if (_section != MusicSection::Drop && energy >= _tuning.quiet
      && energy >= _tuning.dropRise * earlier)
    {
      enter (MusicSection::Drop, bar);
      return;
    }
  if (_section != MusicSection::Breakdown && energy <= _tuning.breakdownFall * earlier)
    {
      enter (MusicSection::Breakdown, bar);
      return;
    }
  if ((_section == MusicSection::Breakdown || _section == MusicSection::Groove) && rising ())
    enter (MusicSection::Build, bar);
}

MusicCue
LiveMood::cue () const
{
  MusicCue cue;
  cue.section = _section;
  cue.energy = newest ();
  cue.previewed = false;

  auto const running = std::max (0LL, _lastBar + 1);
  auto const phrase = static_cast<long long> (std::max (1, _tuning.phraseBars));
  auto const nextLine = (running / phrase + 1) * phrase;

  switch (_section)
    {
    case MusicSection::Build:
      cue.next = MusicSection::Drop;
      cue.changeBar = nextLine;
      break;
    case MusicSection::Breakdown:
      cue.next = MusicSection::Build;
      cue.changeBar = nextLine;
      break;
    case MusicSection::Drop:
      cue.next = MusicSection::Groove;
      cue.changeBar = _since + _tuning.dropBars;
      break;
    case MusicSection::Groove:
      break;
    }
  return cue;
}

}
