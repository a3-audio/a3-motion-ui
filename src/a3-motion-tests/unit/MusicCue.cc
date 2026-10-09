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

#include <a3-motion-engine/preview/MusicCue.hh>

using namespace a3;

// What the pilots go by: StemDeck's preview counted in the engine's bars, or
// the live mood when there is no fresh preview.

namespace
{
MusicAhead
ahead (MusicSection now, std::optional<MusicSection> next, int bars, float energy = 0.4f)
{
  MusicAhead out;
  out.section = now;
  out.next = next;
  out.barsUntilNext = bars;
  out.energy = energy;
  return out;
}
}

TEST (MusicCue, APreviewsBarsCountFromItsDownbeat)
{
  auto const cue = cueFrom (ahead (MusicSection::Build, MusicSection::Drop, 3), 20, true);
  EXPECT_EQ (cue.section, MusicSection::Build);
  ASSERT_TRUE (cue.next.has_value ());
  EXPECT_EQ (*cue.next, MusicSection::Drop);
  EXPECT_EQ (cue.changeBar, 23);
  EXPECT_FLOAT_EQ (cue.energy, 0.4f);
  EXPECT_TRUE (cue.previewed);
}

TEST (MusicCue, ALoopHoldingTheChangeOffIsNoKnownChange)
{
  auto const cue = cueFrom (ahead (MusicSection::Build, MusicSection::Drop, -1), 20, true);
  EXPECT_EQ (cue.changeBar, -1);
}

TEST (MusicCue, TheSetsEndIsNoKnownChange)
{
  auto const cue = cueFrom (ahead (MusicSection::Groove, std::nullopt, 6), 20, true);
  EXPECT_FALSE (cue.next.has_value ());
  EXPECT_EQ (cue.changeBar, -1);
}

TEST (MusicCue, AMessageSentOnTheDownbeatBelongsToItEarlyOrLate)
{
  EXPECT_EQ (nearestDownbeatBar (15.9, 4), 4);
  EXPECT_EQ (nearestDownbeatBar (16.2, 4), 4);
  EXPECT_EQ (nearestDownbeatBar (14.1, 4), 4);
  EXPECT_EQ (nearestDownbeatBar (13.9, 4), 3);
  EXPECT_EQ (nearestDownbeatBar (8.9, 3), 3);
}

TEST (MusicCue, AMeterOfNoBeatsHasNoBars)
{
  EXPECT_EQ (nearestDownbeatBar (10., 0), 0);
}

TEST (MusicCue, AFreshPreviewWinsOverTheLiveMood)
{
  MusicCue live;
  live.section = MusicSection::Breakdown;
  auto const cue = chooseCue (ahead (MusicSection::Build, MusicSection::Drop, 2), 10, live);
  EXPECT_EQ (cue.section, MusicSection::Build);
  EXPECT_EQ (cue.changeBar, 12);
  EXPECT_TRUE (cue.previewed);
}

TEST (MusicCue, WithoutAFreshPreviewTheLiveMoodSpeaks)
{
  MusicCue live;
  live.section = MusicSection::Breakdown;
  live.next = MusicSection::Build;
  live.changeBar = 16;
  auto const cue = chooseCue (std::nullopt, 10, live);
  EXPECT_EQ (cue.section, MusicSection::Breakdown);
  EXPECT_EQ (cue.changeBar, 16);
  EXPECT_FALSE (cue.previewed);
}
