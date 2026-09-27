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

#include <a3-motion-engine/tempo/ExternalTempo.hh>

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

namespace a3
{

// a3-motion-ui#36: the engine took every tempo the external clock reported,
// and the source flipped between 70 and 140 on 17 % of the beats of a 140
// track. The follower is what stands between the two.

TEST (ExternalTempo, TheFirstReportIsTaken)
{
  ExternalTempoFollower follower;
  EXPECT_FLOAT_EQ (follower.onBeat (140.f), 140.f);
}

TEST (ExternalTempo, NonsenseIsIgnored)
{
  ExternalTempoFollower follower;
  follower.onBeat (140.f);
  EXPECT_FLOAT_EQ (follower.onBeat (0.f), 140.f);
  EXPECT_FLOAT_EQ (follower.onBeat (std::numeric_limits<float>::quiet_NaN ()),
                   140.f);
}

// Half or double the tempo it follows is the same tempo counted differently:
// no clip may run at double speed for a beat because the source miscounted.
TEST (ExternalTempo, AnOctaveReportIsTheSameTempo)
{
  ExternalTempoFollower follower;
  for (int i = 0; i < 8; ++i)
    follower.onBeat (140.f);

  for (int i = 0; i < 8; ++i)
    {
      auto const tempo = follower.onBeat (i % 2 == 0 ? 70.1f : 140.f);
      EXPECT_NEAR (tempo, 140.f, 1.f) << "beat " << i;
    }
}

// A report far off that does not hold is a stray, and changes nothing.
TEST (ExternalTempo, AStrayReportChangesNothing)
{
  ExternalTempoFollower follower;
  for (int i = 0; i < 8; ++i)
    follower.onBeat (140.f);

  EXPECT_NEAR (follower.onBeat (77.1f), 140.f, 0.01f);
  EXPECT_NEAR (follower.onBeat (140.f), 140.f, 0.01f);
}

// A small drift is followed, by glides rather than steps.
TEST (ExternalTempo, ADriftIsFollowedSmoothly)
{
  ExternalTempoFollower follower;
  follower.onBeat (140.f);

  auto previous = 140.f;
  for (int i = 0; i < 40; ++i)
    {
      auto const tempo = follower.onBeat (141.f);
      EXPECT_LE (std::abs (tempo - previous), 0.5f) << "beat " << i;
      previous = tempo;
    }
  EXPECT_NEAR (previous, 141.f, 0.05f);
}

// A real change is taken once it has held for a few beats, and then glided
// to -- not the next beat, and not in one step.
TEST (ExternalTempo, ARealChangeIsTakenOnceItHolds)
{
  ExternalTempoFollower follower;
  for (int i = 0; i < 8; ++i)
    follower.onBeat (140.f);

  for (int i = 0; i + 1 < externalTempoChangeBeats; ++i)
    EXPECT_NEAR (follower.onBeat (120.f), 140.f, 0.01f) << "beat " << i;

  auto previous = 140.f;
  for (int i = 0; i < 40; ++i)
    {
      auto const tempo = follower.onBeat (120.f);
      EXPECT_LE (std::abs (tempo - previous), 6.f) << "beat " << i;
      previous = tempo;
    }
  EXPECT_NEAR (previous, 120.f, 0.1f);
}

// And an octave that holds is a track that changed: the lock holds the
// tempo, it does not freeze it.
TEST (ExternalTempo, AnOctaveThatHoldsIsTaken)
{
  ExternalTempoFollower follower;
  for (int i = 0; i < 8; ++i)
    follower.onBeat (140.f);

  float tempo = 140.f;
  for (int i = 0; i < externalTempoOctaveBeats + 40; ++i)
    tempo = follower.onBeat (70.f);
  EXPECT_NEAR (tempo, 70.f, 0.1f);
}

}

namespace a3
{

// Back to the internal clock and out again: the next report is taken as it
// is, not measured against a tempo from before.
TEST (ExternalTempo, AfterAResetTheNextReportIsTaken)
{
  ExternalTempoFollower follower;
  for (int i = 0; i < 8; ++i)
    follower.onBeat (140.f);
  follower.reset ();
  EXPECT_FLOAT_EQ (follower.onBeat (96.f), 96.f);
}

}
