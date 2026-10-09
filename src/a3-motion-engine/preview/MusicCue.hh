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

#include <a3-motion-engine/preview/MusicPreview.hh>

#include <optional>

namespace a3
{

/** What the pilots go by: where the music is and when it changes next,
 *  counted in the engine's own bars. Made on the message thread -- from
 *  StemDeck's preview while it is fresh, else from the live mood -- and kept
 *  by the clock thread, newest wins. */
struct MusicCue
{
  MusicSection section = MusicSection::Groove;
  /** What comes next; nothing: not known, or the set ends first. */
  std::optional<MusicSection> next;
  /** The bar on whose downbeat `next` starts; -1: not known. */
  long long changeBar = -1;
  /** 0-1, how loud the music is now. */
  float energy = 0.f;
  /** From StemDeck's preview, not from the live mood. */
  bool previewed = false;
};

/** A preview as a cue, its bars counted from `downbeatBar` -- the bar whose
 *  downbeat it came on. A loop holding the change off (-1) and the set's end
 *  are no known change. */
MusicCue cueFrom (MusicAhead const &ahead, long long downbeatBar, bool previewed);

/** The bar whose downbeat is nearest `beats`: what a message sent on a
 *  downbeat belongs to, early or late. 0 in a meter of no beats. */
long long nearestDownbeatBar (double beats, int beatsPerBar);

/** The preview while it is fresh, the live mood otherwise. */
MusicCue chooseCue (std::optional<MusicAhead> const &freshPreview, long long previewBar,
                    MusicCue const &liveMood);

}
