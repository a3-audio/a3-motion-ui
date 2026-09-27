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

#include <gtest/gtest.h>

#include <a3-motion-ui/components/SphereMarks.hh>

using namespace a3;

// A shape of dots (Cross, Corner, Bounce) is its dots on the sphere. They were
// three times the line wide, and the line is 0.0018 of the sphere's radius
// since it went to the GPU: a dot of one pixel on the device, which is no
// dot. They keep a size of their own now, and still grow with a thick line.
TEST (SphereMarks, ADotIsSeenWhateverTheLine)
{
  // On the device the sphere's radius is about 180 px: 0.03 is five of them.
  EXPECT_GE (jumpDotDiameter (0.0018f), 0.03f);
}

TEST (SphereMarks, ADotGrowsWithAThickLine)
{
  EXPECT_FLOAT_EQ (jumpDotDiameter (0.03f), 0.09f);
}
