/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

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

#include <a3-motion-ui/components/ElasticLine.hh>

#include <gtest/gtest.h>

namespace a3
{

TEST (ElasticLine, AClosedFigureWrapsAtItsSeam)
{
  // The last point and the first are neighbours on a closed figure, so a
  // bulge sitting on the seam has to reach across it rather than stopping
  // dead.
  EXPECT_EQ (arcDistance (0, 990, 1000, true), 10u);
  EXPECT_EQ (arcDistance (995, 5, 1000, true), 10u);
  EXPECT_EQ (arcDistance (500, 510, 1000, true), 10u);
}

TEST (ElasticLine, AnOpenFigureDoesNotWrap)
{
  // Wrapped, a pull at the start would raise a second bulge at the far end,
  // where nothing is pulling.
  EXPECT_EQ (arcDistance (0, 990, 1000, false), 990u);
  EXPECT_EQ (arcDistance (995, 5, 1000, false), 990u);
}

TEST (ElasticLine, TheLinePassesExactlyThroughTheBlob)
{
  // The point under the blob carries the whole deflection. Anything less and
  // the line stops short of the thing it is supposed to be attached to, which
  // is the one thing this effect exists to show.
  EXPECT_FLOAT_EQ (bulgeWeight (0, 120.f), 1.f);
}

TEST (ElasticLine, ItLandsOnTheFigureWithoutACorner)
{
  // Nothing beyond the reach, and it arrives there smoothly: the last stretch
  // has to flatten out, or the bulge ends in a visible kink.
  EXPECT_FLOAT_EQ (bulgeWeight (120, 120.f), 0.f);
  EXPECT_FLOAT_EQ (bulgeWeight (300, 120.f), 0.f);

  auto const nearEnd = bulgeWeight (118, 120.f);
  auto const justIn = bulgeWeight (110, 120.f);
  EXPECT_LT (nearEnd, 0.01f);
  EXPECT_LT (justIn, 0.1f);
}

TEST (ElasticLine, ItFallsAwayFromTheBlobAndNeverRises)
{
  auto previous = bulgeWeight (0, 200.f);
  for (std::size_t d = 1; d <= 260; ++d)
    {
      auto const now = bulgeWeight (d, 200.f);
      EXPECT_LE (now, previous) << "at " << d;
      EXPECT_GE (now, 0.f);
      EXPECT_LE (now, 1.f);
      previous = now;
    }
}

TEST (ElasticLine, AReachOfZeroIsARigidLine)
{
  // Not even the point under the blob moves: a deflection carried by one
  // point alone is a spike, not a band.
  EXPECT_FLOAT_EQ (bulgeWeight (0, 0.f), 0.f);
  EXPECT_FLOAT_EQ (bulgeWeight (5, 0.f), 0.f);
  EXPECT_FLOAT_EQ (bulgeWeight (0, -1.f), 0.f);
}

TEST (ElasticLine, AWiderReachCarriesMoreOfTheLine)
{
  EXPECT_GT (bulgeWeight (80, 300.f), bulgeWeight (80, 120.f));
  EXPECT_GT (bulgeWeight (200, 300.f), 0.f);
  EXPECT_FLOAT_EQ (bulgeWeight (200, 120.f), 0.f);
}

}
