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
#include "MusicPreview.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

std::optional<MusicSection>
musicSectionFromWord (std::string const &word)
{
  if (word == "groove")
    return MusicSection::Groove;
  if (word == "build")
    return MusicSection::Build;
  if (word == "drop")
    return MusicSection::Drop;
  if (word == "breakdown")
    return MusicSection::Breakdown;
  return std::nullopt;
}

char const *
wordOf (MusicSection section)
{
  switch (section)
    {
    case MusicSection::Groove:
      return "groove";
    case MusicSection::Build:
      return "build";
    case MusicSection::Drop:
      return "drop";
    case MusicSection::Breakdown:
      return "breakdown";
    }
  return "groove";
}

bool
isNoMusic (std::string const &section)
{
  return section == noMusicWord;
}

std::optional<MusicAhead>
musicAheadFrom (std::string const &section, std::string const &next,
                int barsUntilNext, float energy)
{
  // A non-finite energy is a malformed word, not a loud one; -1 is the
  // lowest bar count that means anything (a loop holds the change off).
  if (!std::isfinite (energy) || barsUntilNext < -1)
    return std::nullopt;

  auto const now = musicSectionFromWord (section);
  if (!now)
    return std::nullopt;

  MusicAhead ahead;
  ahead.section = *now;
  ahead.barsUntilNext = barsUntilNext;
  ahead.energy = std::clamp (energy, 0.f, 1.f);

  if (next == setEndsWord)
    return ahead;

  ahead.next = musicSectionFromWord (next);
  if (!ahead.next)
    return std::nullopt;
  return ahead;
}

void
MusicPreview::receive (MusicAhead const &ahead, double nowSeconds)
{
  _ahead = ahead;
  _receivedAt = nowSeconds;
}

void
MusicPreview::clear ()
{
  _ahead.reset ();
}

std::optional<MusicPreview::Current>
MusicPreview::current (double nowSeconds, double bpm) const
{
  if (!_ahead)
    return std::nullopt;

  auto const tempo = bpm > 0.0 ? bpm : tempoWhenUnknown;
  auto const seconds = std::max (0.0, nowSeconds - _receivedAt);
  auto const ageBars = seconds * tempo / 60.0 / beatsPerBar;

  if (ageBars > staleAfterBars)
    return std::nullopt;
  return Current{ *_ahead, ageBars };
}

}
