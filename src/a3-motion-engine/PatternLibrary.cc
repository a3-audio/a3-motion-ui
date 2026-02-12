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

#include "PatternLibrary.hh"

#include <a3-motion-engine/PatternFile.hh>

#include <algorithm>
#include <iostream>

namespace a3
{

PatternLibrary::PatternLibrary (juce::File const &rootDir)
    : _rootDir (rootDir)
{
  // Ensure directories exist
  getSystemDir ().createDirectory ();
  getUserDir ().createDirectory ();

  refresh ();
}

void
PatternLibrary::refresh ()
{
  _entries.clear ();
  _numSystemPatterns = 0;
  _numUserPatterns = 0;

  scanDirectory (getSystemDir (), Category::System);
  _numSystemPatterns = static_cast<int> (_entries.size ());

  scanDirectory (getUserDir (), Category::User);
  _numUserPatterns
      = static_cast<int> (_entries.size ()) - _numSystemPatterns;

  std::cout << "PatternLibrary: " << _numSystemPatterns << " system, "
            << _numUserPatterns << " user patterns loaded from "
            << _rootDir.getFullPathName () << std::endl;
}

void
PatternLibrary::scanDirectory (juce::File const &dir, Category category)
{
  if (!dir.isDirectory ())
    return;

  auto files = dir.findChildFiles (juce::File::findFiles, false, "*.json");

  // Sort alphabetically for deterministic order
  std::sort (files.begin (), files.end (),
             [] (juce::File const &a, juce::File const &b) {
               return a.getFileName ().compareNatural (b.getFileName ()) < 0;
             });

  for (auto const &file : files)
    {
      Entry entry;
      entry.file = file;
      entry.category = category;
      entry.name = PatternFile::peekNameAndTicks (file, entry.ticks);

      if (entry.name.empty ())
        {
          std::cerr << "PatternLibrary: skipping invalid file: "
                    << file.getFullPathName () << std::endl;
          continue;
        }

      _entries.push_back (std::move (entry));
    }
}

int
PatternLibrary::getNumEntries () const
{
  // +1 for the implicit Empty at index 0
  return static_cast<int> (_entries.size ()) + 1;
}

PatternLibrary::Entry const &
PatternLibrary::getEntry (int index) const
{
  // Index 0 = empty (should not be called, but return a static empty entry)
  static Entry emptyEntry{ "Empty", {}, Category::System, {} };
  if (index <= 0 || static_cast<size_t> (index - 1) >= _entries.size ())
    return emptyEntry;
  return _entries[static_cast<size_t> (index - 1)];
}

int
PatternLibrary::indexForName (std::string const &name) const
{
  for (size_t i = 0; i < _entries.size (); ++i)
    {
      if (_entries[i].name == name)
        return static_cast<int> (i) + 1;
    }
  return 0;
}

std::shared_ptr<Pattern>
PatternLibrary::loadPattern (int index) const
{
  if (index <= 0 || static_cast<size_t> (index - 1) >= _entries.size ())
    return nullptr;

  auto const &entry = _entries[static_cast<size_t> (index - 1)];
  return PatternFile::load (entry.file);
}

int
PatternLibrary::saveUserPattern (std::shared_ptr<Pattern> const &pattern)
{
  if (!pattern)
    return 0;

  auto name = pattern->getName ();
  if (name.empty ())
    name = "Recording";

  // Generate unique filename: user_NNNN_<name>.json
  auto userDir = getUserDir ();
  userDir.createDirectory ();

  // Find next available number
  int maxNum = 0;
  auto existing = userDir.findChildFiles (juce::File::findFiles, false,
                                          "*.json");
  for (auto const &f : existing)
    {
      auto fname = f.getFileNameWithoutExtension ();
      // Parse leading number from "user_NNNN_name" or just "NNNN_name"
      auto numStr = fname.upToFirstOccurrenceOf ("_", false, false);
      auto num = numStr.getIntValue ();
      if (num > maxNum)
        maxNum = num;
    }

  auto nextNum = maxNum + 1;
  auto safeNameStr = juce::String (name)
                         .replaceCharacters (" /\\:*?\"<>|", "__________");
  auto filename = juce::String::formatted ("%04d_", nextNum)
                  + safeNameStr + ".json";

  auto file = userDir.getChildFile (filename);

  if (!PatternFile::save (pattern, file))
    {
      std::cerr << "PatternLibrary: failed to save " << file.getFullPathName ()
                << std::endl;
      return 0;
    }

  std::cout << "PatternLibrary: saved user pattern '"
            << name << "' to " << file.getFullPathName () << std::endl;

  // Refresh and return the new index
  refresh ();
  return indexForName (name);
}

int
PatternLibrary::getNumSystemPatterns () const
{
  return _numSystemPatterns;
}

int
PatternLibrary::getNumUserPatterns () const
{
  return _numUserPatterns;
}

}
