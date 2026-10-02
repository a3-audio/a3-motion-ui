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

#include <a3-motion-engine/PositionPacer.hh>

// A channel's position goes to Core at most 60 times a second (2026-10-02):
// on every clock tick it was ~260, Core's port queue filled under load and
// the desk lagged behind stale positions. The newest position still wins:
// a skipped one is sent on the next tick once the interval is up.

using a3::PositionPacer;

namespace
{
double constexpr kInterval = 1000. / PositionPacer::kSendsPerSecond;
}

TEST (PositionPacer, TheFirstPositionGoesAtOnce)
{
  PositionPacer pacer (4);
  EXPECT_TRUE (pacer.due (0, 1000.));
}

TEST (PositionPacer, TheNextWaitsForTheInterval)
{
  PositionPacer pacer (4);
  pacer.sent (0, 1000.);
  EXPECT_FALSE (pacer.due (0, 1000. + kInterval / 2));
  EXPECT_TRUE (pacer.due (0, 1000. + kInterval));
}

TEST (PositionPacer, ChannelsArePacedApart)
{
  PositionPacer pacer (4);
  pacer.sent (0, 1000.);
  EXPECT_TRUE (pacer.due (1, 1000.));
}

TEST (PositionPacer, TicksAt260HzSendSixtyASecond)
{
  // A tick every 3.85 ms (96 PPQN at ~162 BPM); the pacer must not round
  // every send up to the next tick and fall to ~52 a second.
  PositionPacer pacer (1);
  auto sends = 0;
  for (auto tick = 0; tick < 2600; ++tick) // ten seconds
    {
      auto const now = tick * (1000. / 260.);
      if (pacer.due (0, now))
        {
          pacer.sent (0, now);
          ++sends;
        }
    }
  EXPECT_NEAR (sends, 600, 6);
}

TEST (PositionPacer, AfterAPauseItDoesNotBurst)
{
  // Nothing moved for a second: the next two ticks must not both send.
  PositionPacer pacer (1);
  pacer.sent (0, 0.);
  EXPECT_TRUE (pacer.due (0, 1000.));
  pacer.sent (0, 1000.);
  EXPECT_FALSE (pacer.due (0, 1003.85));
}
