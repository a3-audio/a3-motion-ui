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

#include <a3-motion-ui/AppPaths.hh>

using a3::logFile;
using a3::resourceDirectory;

namespace
{
struct Scratch
{
  juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getNonexistentChildFile ("a3-app-paths", "", false);
  Scratch () { dir.createDirectory (); }
  ~Scratch () { dir.deleteRecursively (); }
};

juce::String
sourcesOf (juce::File const &folder)
{
  juce::String all;
  for (auto const &file : folder.findChildFiles (juce::File::findFiles, true, "*.cc;*.hh"))
    all << file.getFullPathName () << "\n" << file.loadFileAsString () << "\n";
  return all;
}
} // namespace

TEST (AppPaths, ResourcesBesideTheExecutableWin)
{
  // build.sh links resources/ beside the binary of a checkout or worktree.
  Scratch s;
  s.dir.getChildFile ("bin/resources").createDirectory ();
  s.dir.getChildFile ("share/a3-motion-ui/resources").createDirectory ();
  EXPECT_EQ (resourceDirectory (s.dir.getChildFile ("bin/a3-motion-ui")),
             s.dir.getChildFile ("bin/resources"));
}

TEST (AppPaths, ThePackagesResourcesAreInShare)
{
  // /usr/bin/a3-motion-ui -> /usr/share/a3-motion-ui/resources.
  Scratch s;
  s.dir.getChildFile ("bin").createDirectory ();
  EXPECT_EQ (resourceDirectory (s.dir.getChildFile ("bin/a3-motion-ui")),
             s.dir.getChildFile ("share/a3-motion-ui/resources"));
  EXPECT_EQ (resourceDirectory (juce::File ("/usr/bin/a3-motion-ui")),
             juce::File ("/usr/share/a3-motion-ui/resources"));
}

TEST (AppPaths, NoResourceIsReadFromTheWorkingDirectory)
{
  // The package runs Motion in the user's data folder, which holds no
  // resources: a picture read from there is a picture silently missing.
  auto const sources = sourcesOf (juce::File (A3_UI_SOURCE_DIR));
  for (auto at = sources.indexOf ("\"resources/"); at >= 0;
       at = sources.indexOf (at + 1, "\"resources/"))
    EXPECT_FALSE (sources.substring (juce::jmax (0, at - 160), at)
                      .contains ("getCurrentWorkingDirectory"))
        << sources.substring (juce::jmax (0, at - 160), at + 40);
}

TEST (AppPaths, TheLogIsInTheUsersStateFolder)
{
  EXPECT_EQ (logFile ("/home/aaa", ""),
             juce::File ("/home/aaa/.local/state/a3-motion/a3-motion-ui.log"));
}

TEST (AppPaths, AnAbsoluteXdgStateHomeWins)
{
  EXPECT_EQ (logFile ("/home/aaa", "/data/state"),
             juce::File ("/data/state/a3-motion/a3-motion-ui.log"));
}

TEST (AppPaths, ARelativeXdgStateHomeIsIgnored)
{
  // The XDG spec: a relative $XDG_STATE_HOME is invalid and must be ignored.
  EXPECT_EQ (logFile ("/home/aaa", "state"),
             juce::File ("/home/aaa/.local/state/a3-motion/a3-motion-ui.log"));
}

TEST (AppPaths, TheLogIsNotWrittenBesideTheExecutable)
{
  // /usr/bin is not writable; the package's Motion would log nowhere.
  auto const app = juce::File (A3_UI_SOURCE_DIR).getChildFile ("StandaloneApp.cc").loadFileAsString ();
  EXPECT_FALSE (app.contains ("currentExecutableFile"));
}
