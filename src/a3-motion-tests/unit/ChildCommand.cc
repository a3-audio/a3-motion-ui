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

#include <a3-motion-ui/io/ChildCommand.hh>
#include <a3-motion-ui/io/Workspaces.hh>

#include <unistd.h>

#include <fstream>
#include <sstream>
#include <string>

using namespace a3;

namespace
{
/** Children of this process that have ended and not been waited for. */
int
zombieChildren ()
{
  auto const self = std::to_string (getpid ());
  int count = 0;
  for (auto const &entry : juce::RangedDirectoryIterator (
           juce::File ("/proc"), false, "*", juce::File::findDirectories))
    {
      std::ifstream stat (entry.getFile ().getChildFile ("stat")
                              .getFullPathName ().toStdString ());
      std::string line;
      std::getline (stat, line);
      // "pid (comm) S ppid ...": comm may hold spaces, so start after ')'.
      auto const close = line.rfind (')');
      if (close == std::string::npos)
        continue;
      std::string state, ppid;
      std::istringstream fields (line.substr (close + 1));
      fields >> state >> ppid;
      if (state == "Z" && ppid == self)
        ++count;
    }
  return count;
}
}

TEST (ChildCommand, ReturnsWhatTheCommandPrinted)
{
  EXPECT_EQ (runCommand ({ "echo", "hello" }).trim (), "hello");
}

TEST (ChildCommand, ACommandThatCannotStartIsEmpty)
{
  EXPECT_TRUE (runCommand ({ "a3-no-such-command-anywhere" }).isEmpty ());
}

TEST (ChildCommand, LeavesNoZombieBehind)
{
  auto const before = zombieChildren ();
  for (int i = 0; i < 50; ++i)
    runCommand ({ "true" });
  EXPECT_EQ (zombieChildren (), before);
}

TEST (ChildCommand, AHungChildIsKilledAndReaped)
{
  auto const before = zombieChildren ();
  auto const started = juce::Time::getMillisecondCounter ();
  // Closes stdout and stderr (JUCE pipes both) at once, then outlives the timeout.
  runCommand ({ "sh", "-c", "exec >&- 2>&-; sleep 30" }, 200);
  EXPECT_LT (juce::Time::getMillisecondCounter () - started, 5000u);
  EXPECT_EQ (zombieChildren (), before);
}

TEST (ChildCommand, TheWorkspaceCallsLeaveNoZombieBehind)
{
  auto const before = zombieChildren ();
  for (int i = 0; i < 20; ++i)
    {
      workspaces::list ();
      workspaces::goTo (1);
    }
  EXPECT_EQ (zombieChildren (), before);
}
