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

#include "ClipMigration.hh"

#include <a3-motion-engine/ClipFile.hh>
#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/PatternFile.hh>

#include <iostream>

namespace a3
{

int
migrateCombinedPatterns (juce::File const &root)
{
  auto const takes = root.getChildFile ("user");
  if (!takes.isDirectory ())
    return 0;

  auto const clips = root.getChildFile ("clips");

  int migrated = 0;

  for (auto const &file :
       takes.findChildFiles (juce::File::findFiles, false, "*.svg"))
    {
      // The clip is named after the shape *without* its beat-count prefix --
      // 04_Rec_120613.svg is reached by Rec_120613.json -- because that is the
      // name PatternLibrary looks a clip up by, and the one the factory clips
      // and saveUserPattern() already use. Named with the prefix, every
      // migrated take would be invisible.
      auto const shape = file.getFileNameWithoutExtension ();
      auto const name = shape.fromFirstOccurrenceOf ("_", false, false);
      auto const clipFile = clips.getChildFile (name + ".json");

      // What somebody set beats what a file once held: an existing clip is
      // never overwritten by a migration that happens to run afterwards.
      if (clipFile.existsAsFile ())
        continue;

      auto const pattern = PatternFile::load (file);
      if (pattern == nullptr)
        {
          std::cerr << "ClipMigration: cannot read "
                    << file.getFullPathName () << std::endl;
          continue;
        }

      Clip clip;
      clip.name = name.toStdString ();
      clip.svg = shape.toStdString ();
      clip.settings = clipSettingsFrom (*pattern);

      // The fade is already in the geometry -- see the header.
      clip.settings.fadeSixteenths = 0;

      if (ClipFile::save (clip, clipFile))
        ++migrated;
      else
        std::cerr << "ClipMigration: cannot write "
                  << clipFile.getFullPathName () << std::endl;
    }

  if (migrated > 0)
    std::cout << "ClipMigration: " << migrated
              << " takes given a clip of their own" << std::endl;

  return migrated;
}

}
