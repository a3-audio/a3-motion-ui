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

#pragma once

#include <JuceHeader.h>

#include <vector>

namespace a3
{

/** A folder split into what the instrument ships with and what the performer
 *  made, the way the shapes' folder already is.
 *
 *  The split does two jobs, and the second is the one that matters: it lets
 *  the browser's filter narrow a list, and it lets a shipped file be
 *  protected from being written over. Without it every action and every set
 *  the device ships with could be overwritten silently.
 *
 *  Which of the two a file is, is decided by which directory it sits in, and
 *  that is decided where the files come from -- the repository puts its own
 *  into `system/` -- rather than by a rule the device has to guess at
 *  runtime.
 */
struct SplitFolderEntry
{
  juce::String name;
  juce::File file;
  bool isSystem;
};

/** The file a name stands for, `user/` first and then `system/`.
 *
 *  Yours wins, so one of the instrument's own can be covered by a version of
 *  your own without the original being deleted -- it stays there to go back
 *  to. A name with nothing behind it comes back as a file that does not
 *  exist, which is what every caller here already checks for.
 */
juce::File namedFileIn (juce::File const &root, juce::String const &name,
                        juce::String const &extension);

/** Where something new is written: always `user/`. Nothing the device writes
 *  may land among the instrument's own. */
juce::File newFileIn (juce::File const &root, juce::String const &name,
                      juce::String const &extension);

/** The first of `base`, `base 2`, `base 3`, ... that neither half holds.
 *
 *  Counted rather than stamped, because "Action 2" is a name somebody can say
 *  out loud and find again -- the same reasoning freeClipName() follows, which
 *  this replaces wherever a folder is split. */
juce::String freeNameIn (juce::File const &root, juce::String const &base,
                         juce::String const &extension);

/** Move whatever is lying flat in `root` into `user/`, and say how many.
 *
 *  What is already there is the performer's, and that is not a guess: the
 *  instrument's own arrive in `system/` because the repository puts them
 *  there. git knows what shipped and the device cannot, so the split is made
 *  where the files come from and this only tidies up what is left.
 *
 *  Idempotent, so it can run on every start -- a migration that cannot is one
 *  somebody has to remember to run. A loose name that a shipped file already
 *  has is counted rather than lost, and only the extension asked for is
 *  touched: a folder may hold a readme or a ledger, and a migration that swept
 *  those up would be one nobody could leave a note in.
 */
int splitLooseFilesIn (juce::File const &root, juce::String const &extension);

/** Everything in both, sorted by the name that is shown, one row per name --
 *  a covered name is listed once, as the file that would be opened. */
std::vector<SplitFolderEntry> listFilesIn (juce::File const &root,
                                           juce::String const &extension);

}
