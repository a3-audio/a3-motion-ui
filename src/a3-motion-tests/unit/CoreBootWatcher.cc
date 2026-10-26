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

#include <JuceHeader.h>

#include <a3-motion-engine/ChannelSource.hh>
#include <a3-motion-engine/CoreBootWatcher.hh>

using namespace a3;

namespace
{
juce::OSCMessage
announcement (std::initializer_list<juce::String> strings,
              juce::String const &address = "/core/here")
{
  juce::OSCMessage message (address);
  for (auto const &s : strings)
    message.addString (s);
  return message;
}
}

// Core's /core/here carries url, fingerprint and, since the boot id, a third
// string. Only that third one says which Core this is.
TEST (CoreBootWatcher, TheBootIdIsTheThirdString)
{
  EXPECT_EQ (bootIdOf (announcement ({ "http://core/api/truth", "fp", "abc" })),
             std::optional<std::string> ("abc"));
}

TEST (CoreBootWatcher, AnAnnouncementWithoutOneSaysNothing)
{
  EXPECT_FALSE (bootIdOf (announcement ({ "url", "fp" })));
  EXPECT_FALSE (bootIdOf (announcement ({ "url", "fp", "" })));
  EXPECT_FALSE (bootIdOf (announcement ({ "url", "fp", "abc" }, "/other")));

  juce::OSCMessage notAString ("/core/here");
  notAString.addString ("url");
  notAString.addString ("fp");
  notAString.addInt32 (7);
  EXPECT_FALSE (bootIdOf (notAString));
}

// The watcher hands every boot id on; what a new Core means is the
// ChannelSources' decision. Together, in the order of a start: Core's recall
// answer sets the decks, then the first announcement arrives and must keep
// them; a repeat does nothing; a changed id clears them.
TEST (CoreBootWatcher, AChangedBootIdClearsTheDecksAndTheFirstDoesNot)
{
  ChannelSources sources;
  int calls = 0;
  CoreBootWatcher watcher ([&] (std::string const &id) {
    ++calls;
    sources.coreAnnounced (id);
  });

  sources.setSource (0, 1);
  sources.setPresent (1, true);
  watcher.handle (announcement ({ "url", "fp", "boot-1" }));
  EXPECT_EQ (sources.source (0), 1) << "the first announcement wiped the recall";
  EXPECT_TRUE (sources.isPresent (1));
  watcher.handle (announcement ({ "url", "fp", "boot-1" }));
  EXPECT_EQ (sources.source (0), 1);

  watcher.handle (announcement ({ "url", "fp" }));
  EXPECT_EQ (calls, 2) << "an announcement without an id was handed on";

  watcher.handle (announcement ({ "url", "fp", "boot-2" }));
  EXPECT_EQ (sources.source (0), analogSource);
}
