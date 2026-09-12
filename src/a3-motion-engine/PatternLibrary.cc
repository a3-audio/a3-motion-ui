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

#include "SplitFolder.hh"

#include <a3-motion-engine/ClipFile.hh>
#include <a3-motion-engine/ClipSettings.hh>

#include <a3-motion-engine/PatternFile.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>

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

  auto const beforePresets = static_cast<int> (_entries.size ());
  scanSettingsPresets ();
  _numSettingsPresets = static_cast<int> (_entries.size ()) - beforePresets;

  std::cout << "PatternLibrary: " << _numSystemPatterns << " system, "
            << _numUserPatterns << " user patterns, " << _numSettingsPresets
            << " settings presets loaded from " << _rootDir.getFullPathName ()
            << std::endl;
}

void
PatternLibrary::scanSettingsPresets ()
{
  auto const dir = getClipDir ();
  if (!dir.isDirectory ())
    return;

  // Both halves, the way every other list here is read. A clip's origin does
  // not fit in `category` -- that slot says it is a clip -- so it is carried
  // beside it, which is what lets the clips be filtered at all.
  auto const listed = listFilesIn (dir, ".json");

  // Same rule as the shapes: by the name that is read, within this category
  // only. A clip's name and its file name usually agree -- a renamed one is
  // exactly when they do not.
  auto const firstOfThisCategory = _entries.size ();

  for (auto const &found : listed)
    {
      auto const &file = found.file;
      auto const clip = ClipFile::load (file);
      if (!clip.has_value ())
        {
          std::cerr << "PatternLibrary: skipping unreadable clip: "
                    << file.getFullPathName () << std::endl;
          continue;
        }

      if (clip->name.empty ())
        continue;

      // Every clip, whether or not it names a shape. They were skipped when
      // they did, on the grounds that the shape listed them -- which put the
      // two kinds in one list and made a row's meaning depend on which row it
      // was.
      Entry entry;
      entry.name = clip->name;
      entry.category = Category::Clip;
      entry.clipFile = file;
      entry.svg = clip->svg;
      entry.isShipped = found.isSystem;

      _entries.push_back (std::move (entry));
    }

  sortCategoryByName (firstOfThisCategory);
}

void
PatternLibrary::scanDirectory (juce::File const &dir, Category category)
{
  if (!dir.isDirectory ())
    return;

  auto files = dir.findChildFiles (juce::File::findFiles, false, "*.svg");

  // Sorted after the names are read, not by file name: a shape's file name
  // carries its beat count, so 04_Zigzag sorted before 16_Arc -- which looks
  // like no order at all to anyone who cannot see the prefix. Within this
  // category only, so the grouping the browser's dot makes visible survives.
  auto const firstOfThisCategory = _entries.size ();

  for (auto const &file : files)
    {
      Entry entry;
      entry.file = file;
      entry.category = category;

      auto pr = PatternFile::peek (file);
      entry.name = pr.name;
      entry.svgPathData = pr.pathData;
      entry.jumpDots = pr.jumpDots;
      entry.hasJumpDots = !pr.jumpDots.empty ();
      entry.lengthBeats = pr.lengthBeats;

      if (entry.name.empty ())
        {
          std::cerr << "PatternLibrary: skipping invalid file: "
                    << file.getFullPathName () << std::endl;
          continue;
        }

      // No clip is looked for beside a shape any more. A clip names the shape
      // it is played on, outright, so the two are related in one direction
      // and by name -- the file-name convention that guessed it the other way
      // round is what let a preset and a shape wear the same name.

      // Said in the field as well as in the category, so one question has one
      // answer wherever it is asked.
      entry.isShipped = category == Category::System;

      _entries.push_back (std::move (entry));
    }

  sortCategoryByName (firstOfThisCategory);
}

void
PatternLibrary::sortCategoryByName (size_t firstOfCategory)
{
  std::sort (_entries.begin () + static_cast<long> (firstOfCategory),
             _entries.end (), [] (Entry const &a, Entry const &b) {
               return juce::String (a.name).compareNatural (
                          juce::String (b.name))
                      < 0;
             });
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
  static Entry emptyEntry{ "Empty", {}, Category::System, {}, false, {}, {}, 0 };
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

int
PatternLibrary::indexForClipFile (juce::File const &file) const
{
  if (file == juce::File{})
    return 0;

  for (size_t i = 0; i < _entries.size (); ++i)
    if (_entries[i].clipFile == file)
      return static_cast<int> (i) + 1;

  return 0;
}

bool
PatternLibrary::isFactory (int index) const
{
  if (index <= 0 || static_cast<size_t> (index - 1) >= _entries.size ())
    return false;

  return _entries[static_cast<size_t> (index - 1)].category
         == Category::System;
}

std::shared_ptr<Pattern>
PatternLibrary::loadPattern (int index) const
{
  if (index <= 0 || static_cast<size_t> (index - 1) >= _entries.size ())
    return nullptr;

  auto const &entry = _entries[static_cast<size_t> (index - 1)];
  auto pattern = PatternFile::load (entry.file);

  // NOTE: patterns store 2D positions (x,y only, z=0).
  // The HeightMap (elevation coverage) is applied at playback time
  // by MotionEngine::performPlayback(), not at load time.
  // This allows dynamic coverage changes without reloading patterns.

  // The shape says where the sound goes; the clip beside it says how it is
  // played. Applied after the shape is read, so a clip's settings win over
  // whatever the shape file still happens to carry -- which matters while both
  // formats are in use, and stops mattering once the shape holds none.
  if (pattern != nullptr && entry.clipFile.existsAsFile ())
    if (auto const clip = ClipFile::load (entry.clipFile))
      applyClipSettings (*pattern, clip->settings);

  return pattern;
}

int
PatternLibrary::saveUserPattern (std::shared_ptr<Pattern> const &pattern)
{
  if (!pattern)
    return 0;

  auto name = pattern->getName ();
  if (name.empty ())
    name = "Recording";

  // Generate filename: <beats>_<name>.svg  e.g. 04_Rec_123456.svg
  auto userDir = getUserDir ();
  userDir.createDirectory ();

  auto const numTicks = pattern->getNumTicks ();
  auto const lengthBeats
      = static_cast<int> (numTicks) / TempoClock::getTicksPerBeat ();

  auto safeNameStr = juce::String (name)
                         .replaceCharacters (" /\\:*?\"<>|", "__________");
  auto basename = juce::String::formatted ("%02d_", lengthBeats)
                  + safeNameStr;

  // If a file with this name already exists, append _2, _3, ...
  auto filename = basename + ".svg";
  auto existing = userDir.findChildFiles (juce::File::findFiles, false,
                                          "*.svg");
  int suffix = 1;
  while (userDir.getChildFile (filename).existsAsFile ())
    {
      ++suffix;
      filename = basename + "_" + juce::String (suffix) + ".svg";
    }

  auto file = userDir.getChildFile (filename);

  if (!PatternFile::save (pattern, file))
    {
      std::cerr << "PatternLibrary: failed to save " << file.getFullPathName ()
                << std::endl;
      return 0;
    }

  // The take's settings, in a clip of its own beside the shape. A new clip
  // every time, never the slot's existing one: the slot's clip may be in other
  // slots and in other sessions, and pointing it at a fresh recording would
  // overwrite every one of them without a word.
  //
  // Naming the shape it was recorded on, which is how every clip says what it
  // is played on.
  {
    Clip clip;
    clip.svg = pattern->getName ();
    clip.name = file.getFileNameWithoutExtension ()
                    .fromFirstOccurrenceOf ("_", false, false)
                    .toStdString ();
    clip.settings = clipSettingsFrom (*pattern);

    auto const clipFile
        = newFileIn (getClipDir (), juce::String (clip.name), ".json");
    if (!ClipFile::save (clip, clipFile))
      std::cerr << "PatternLibrary: failed to save the clip for "
                << file.getFullPathName () << std::endl;
  }

  std::cout << "PatternLibrary: saved user pattern '"
            << name << "' to " << file.getFullPathName () << std::endl;

  // Refresh and return the new index
  refresh ();
  return indexForName (name);
}

juce::int64
PatternLibrary::getDirectoryFingerprint () const
{
  juce::int64 hash = 0;
  auto hashDir = [&hash] (juce::File const &dir, juce::String const &pattern) {
    if (!dir.isDirectory ())
      return;
    auto files = dir.findChildFiles (juce::File::findFiles, false, pattern);
    for (auto const &f : files)
      {
        // Mix filename and modification time into hash
        hash ^= f.getFileName ().hashCode64 ();
        hash ^= f.getLastModificationTime ().toMilliseconds ();
        hash = (hash << 7) | (static_cast<juce::uint64> (hash) >> 57); // rotate
      }
    // Also mix file count so deletions are detected
    hash ^= static_cast<juce::int64> (files.size ()) * 2654435761LL;
  };
  hashDir (getSystemDir (), "*.svg");
  hashDir (getUserDir (), "*.svg");
  // Clips too: a settings preset has no shape, so a folder watched for SVGs
  // alone would never notice one being added or thrown away.
  hashDir (getClipDir ().getChildFile ("system"), "*.json");
  hashDir (getClipDir ().getChildFile ("user"), "*.json");
  return hash;
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

int
PatternLibrary::getNumSettingsPresets () const
{
  return _numSettingsPresets;
}

}
