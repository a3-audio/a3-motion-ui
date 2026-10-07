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
#include <a3-motion-engine/OscSendGuard.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <regex>
#include <set>
#include <stdexcept>
#include <string>

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

TEST (TestsSendNothing, TheRunnerRefusesOscSenders)
{
  EXPECT_TRUE (oscSendersRefused ());
}

// Whatever the host: a documentation address that routes nowhere is refused
// as well, so a test that tries fails on every machine and with every truth.
TEST (TestsSendNothing, ASenderIsRefusedWhateverItsHost)
{
  for (auto const *host : { "127.0.0.1", "192.168.8.10", "192.0.2.10" })
    {
      juce::OSCSender sender;
      EXPECT_THROW (connectOscSender (sender, host, 9000), std::logic_error)
          << host;
    }
}

// The guard holds only if every OSC sender is aimed through it. Each
// juce::OSCSender in the app and the engine is named here, and none of
// those names may be connected directly.
TEST (TestsSendNothing, EverySenderIsAimedThroughTheGuard)
{
  auto const src = juce::File (__FILE__)
                       .getParentDirectory ()
                       .getParentDirectory ()
                       .getParentDirectory ();
  auto const sources = [&] {
    juce::Array<juce::File> files;
    for (auto const *dir : { "a3-motion-ui", "a3-motion-engine" })
      files.addArray (src.getChildFile (dir).findChildFiles (
          juce::File::findFiles, true, "*.cc;*.hh;*.h;*.cpp"));
    return files;
  }();
  ASSERT_FALSE (sources.isEmpty ()) << src.getFullPathName ();

  std::regex const declared{ R"(juce::OSCSender\s+(\w+))" };
  std::set<std::string> names;
  for (auto const &file : sources)
    {
      auto const text = file.loadFileAsString ().toStdString ();
      for (std::sregex_iterator it (text.begin (), text.end (), declared), end;
           it != end; ++it)
        names.insert ((*it)[1].str ());
    }
  ASSERT_FALSE (names.empty ()) << "no juce::OSCSender found -- moved?";

  for (auto const &file : sources)
    {
      if (file.getFileName () == "OscSendGuard.cc")
        continue;
      auto const text = file.loadFileAsString ().toStdString ();
      for (auto const &name : names)
        {
          std::regex const direct{ "\\b" + name + R"(\s*(\.|->)\s*connect\s*\()" };
          EXPECT_FALSE (std::regex_search (text, direct))
              << file.getFullPathName () << " connects " << name
              << " directly; aim it with connectOscSender ()";
        }
    }
}

}
