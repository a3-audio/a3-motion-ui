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

#include "MusicCue.hh"

#include <cmath>

namespace a3
{

MusicCue
cueFrom (MusicAhead const &ahead, long long downbeatBar, bool previewed)
{
  MusicCue cue;
  cue.section = ahead.section;
  cue.next = ahead.next;
  cue.energy = ahead.energy;
  cue.previewed = previewed;
  if (ahead.next && ahead.barsUntilNext >= 0)
    cue.changeBar = downbeatBar + ahead.barsUntilNext;
  return cue;
}

long long
nearestDownbeatBar (double beats, int beatsPerBar)
{
  if (beatsPerBar <= 0 || !std::isfinite (beats))
    return 0;
  return std::llround (beats / beatsPerBar);
}

MusicCue
chooseCue (std::optional<MusicAhead> const &freshPreview, long long previewBar,
           MusicCue const &liveMood)
{
  if (freshPreview)
    return cueFrom (*freshPreview, previewBar, true);
  return liveMood;
}

MusicCue
withSectionStart (MusicCue next, MusicCue const &previous, long long bar)
{
  if (next.sectionStartBar >= 0)
    return next;
  if (next.section == previous.section && previous.sectionStartBar >= 0)
    {
      next.sectionStartBar = previous.sectionStartBar;
      return next;
    }
  auto const announced = next.section != previous.section && previous.next
                         && *previous.next == next.section && previous.changeBar >= 0;
  next.sectionStartBar = announced ? previous.changeBar : bar;
  return next;
}

}
