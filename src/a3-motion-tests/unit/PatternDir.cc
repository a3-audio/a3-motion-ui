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

#include <a3-motion-ui/PatternDir.hh>

using a3::patternDirectory;

namespace
{
juce::File const base ("/home/aaa/a3-system/a3-motion/ui");

juce::var
config (char const *json)
{
  return juce::JSON::parse (json);
}
} // namespace

TEST (PatternDir, ARelativePathIsTakenFromTheWorkingDirectory)
{
  EXPECT_EQ (patternDirectory (config (R"({"patternDir": "pattern"})"), base),
             juce::File ("/home/aaa/a3-system/a3-motion/ui/pattern"));
}

TEST (PatternDir, AnAbsolutePathIsTakenAsItIs)
{
  EXPECT_EQ (patternDirectory (config (R"({"patternDir": "/media/stick/pattern"})"), base),
             juce::File ("/media/stick/pattern"));
}

TEST (PatternDir, NoKeyIsPatternBesideTheConfig)
{
  EXPECT_EQ (patternDirectory (config ("{}"), base), base.getChildFile ("pattern"));
  EXPECT_EQ (patternDirectory (config (R"({"patternDir": "  "})"), base),
             base.getChildFile ("pattern"));
}

TEST (PatternDir, TheShippedConfigIsRelative)
{
  // An absolute path in the shipped config is right on one machine only.
  auto const shipped = juce::JSON::parse (
      juce::File (A3_CONFIG_JSON_PATH).loadFileAsString ());
  auto const named = shipped.getProperty ("patternDir", juce::var ()).toString ();
  EXPECT_FALSE (juce::File::isAbsolutePath (named)) << named;
}
