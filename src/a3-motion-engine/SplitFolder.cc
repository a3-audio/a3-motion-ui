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

#include "SplitFolder.hh"

#include <algorithm>

namespace a3
{

namespace
{
juce::File
systemDir (juce::File const &root)
{
  return root.getChildFile ("system");
}

juce::File
userDir (juce::File const &root)
{
  return root.getChildFile ("user");
}
}

juce::File
namedFileIn (juce::File const &root, juce::String const &name,
             juce::String const &extension)
{
  auto const mine = userDir (root).getChildFile (name + extension);
  if (mine.existsAsFile ())
    return mine;

  return systemDir (root).getChildFile (name + extension);
}

juce::File
newFileIn (juce::File const &root, juce::String const &name,
           juce::String const &extension)
{
  return userDir (root).getChildFile (name + extension);
}

juce::String
freeNameIn (juce::File const &root, juce::String const &base,
            juce::String const &extension)
{
  if (!namedFileIn (root, base, extension).existsAsFile ())
    return base;

  for (auto n = 2; n < 1000; ++n)
    {
      auto const candidate = base + " " + juce::String (n);
      if (!namedFileIn (root, candidate, extension).existsAsFile ())
        return candidate;
    }

  return base + " " + juce::String (juce::Time::currentTimeMillis ());
}

int
splitLooseFilesIn (juce::File const &root, juce::String const &extension)
{
  auto const loose
      = root.findChildFiles (juce::File::findFiles, false, "*" + extension);
  if (loose.isEmpty ())
    return 0;

  userDir (root).createDirectory ();

  auto moved = 0;
  for (auto const &file : loose)
    {
      auto const name
          = freeNameIn (root, file.getFileNameWithoutExtension (), extension);
      if (file.moveFileTo (userDir (root).getChildFile (name + extension)))
        ++moved;
    }

  return moved;
}

std::vector<SplitFolderEntry>
listFilesIn (juce::File const &root, juce::String const &extension)
{
  std::vector<SplitFolderEntry> entries;

  // The performer's first, so a name of theirs is the one that stays when a
  // shipped one of the same name is skipped below. Same order as the lookup,
  // and it has to be: a list that showed one file and opened another would be
  // a list you cannot trust.
  auto const scan = [&entries, &extension] (juce::File const &dir,
                                            bool isSystem) {
    for (auto const &file : dir.findChildFiles (juce::File::findFiles, false,
                                                "*" + extension))
      {
        auto const name = file.getFileNameWithoutExtension ();

        auto const already
            = std::any_of (entries.begin (), entries.end (),
                           [&name] (SplitFolderEntry const &e) {
                             return e.name == name;
                           });
        if (!already)
          entries.push_back ({ name, file, isSystem });
      }
  };

  scan (userDir (root), false);
  scan (systemDir (root), true);

  std::sort (entries.begin (), entries.end (),
             [] (SplitFolderEntry const &a, SplitFolderEntry const &b) {
               return a.name.compareIgnoreCase (b.name) < 0;
             });

  return entries;
}

}
