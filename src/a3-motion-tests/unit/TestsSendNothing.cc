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

#include <JuceHeader.h>

#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <stdexcept>

using namespace a3;

namespace
{

// An engine built without a backend aims at Core as the truth names it --
// on the rig, the live one. Every test run used to send positions and
// 3D/FREQ/Q there (#67). In the test runner that engine must not come up.
TEST (TestsSendNothing, AnEngineWithoutABackendIsRefused)
{
  HeightMapSphere heightMap;
  EXPECT_THROW (MotionEngine (4, heightMap), std::logic_error);
}

}
