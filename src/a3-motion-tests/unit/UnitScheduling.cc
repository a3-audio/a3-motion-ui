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

#include <gtest/gtest.h>

#include <JuceHeader.h>

namespace
{

// Real-time priority for the whole process put every thread of the app --
// GUI, OSC, the lot -- above JACK's process threads, and JACK reported late
// clients until it was gone (a3nuc1, 2026-10-06). Only JACK's process threads
// run real-time; JACK gives client threads their priority itself.
//
// Not even as a comment: a commented-out line is the one that gets switched
// back on.

juce::File
motionUnit ()
{
  return juce::File (A3_PLATFORM_CONFIG_DIR).getChildFile ("a3-motion.service");
}

} // namespace

TEST (UnitScheduling, TheUnitFileIsThere)
{
  ASSERT_TRUE (motionUnit ().existsAsFile ())
      << motionUnit ().getFullPathName ();
}

TEST (UnitScheduling, MotionSetsNoRealtimePolicyForTheWholeProcess)
{
  juce::StringArray lines;
  lines.addLines (motionUnit ().loadFileAsString ());

  for (auto const &line : lines)
    {
      EXPECT_FALSE (line.contains ("CPUSchedulingPolicy")) << line;
      EXPECT_FALSE (line.contains ("CPUSchedulingPriority")) << line;
      EXPECT_FALSE (line.contains ("LimitRTPRIO")) << line;
    }
}
