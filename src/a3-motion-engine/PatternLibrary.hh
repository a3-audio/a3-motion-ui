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

#include <memory>
#include <string>
#include <vector>

#include <JuceHeader.h>

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/elevation/HeightMap.hh>

namespace a3
{

/**
 * PatternLibrary manages two directories of pattern files:
 *   - system/  — read-only factory patterns (shipped with the app)
 *   - user/    — user-recorded patterns (created at runtime)
 *
 * The library provides an ordered list of all available patterns
 * (system first, then user) that the UI uses for encoder cycling
 * through trajectories.
 *
 * Index 0 is always "Empty" (no pattern).
 * Indices 1..numSystem are system patterns.
 * Indices numSystem+1.. are user patterns.
 *
 * Z (height) is not stored in files — it is computed live via
 * HeightMap when patterns are loaded for playback.
 */
class PatternLibrary
{
public:
  enum class Category
  {
    System,
    User
  };

  struct Entry
  {
    std::string name;
    juce::File file;
    Category category;
    std::string svgPathData;   ///< SVG path 'd' attribute (normalised [-1,1])
    bool hasJumpDots{ false }; ///< true if pattern is a jump/dot pattern
    std::vector<std::pair<float,float>> jumpDots; ///< normalised dot positions
    std::vector<Pos> ticks;    ///< cached tick data for playback / fallback
    int lengthBeats{ 0 };      ///< pattern length in beats (from SVG metadata)
  };

  /** Initialise with root directory containing system/ and user/ subdirs.
   *  The HeightMap is used to compute Z when loading patterns for playback. */
  PatternLibrary (juce::File const &rootDir, HeightMap const &heightMap);

  /** Rescan both directories for .svg files.
   *  After this, getEntries() returns the updated list. */
  void refresh ();

  /** Total number of entries INCLUDING the implicit Empty at index 0. */
  int getNumEntries () const;

  /** Get entry at index (1-based, 0 = empty). */
  Entry const &getEntry (int index) const;

  /** Find the index for a pattern name.  Returns 0 if not found. */
  int indexForName (std::string const &name) const;

  /** Load a full Pattern object for the given index. */
  std::shared_ptr<Pattern> loadPattern (int index) const;

  /** Save a pattern to the user directory.
   *  Generates a unique filename.  Returns the assigned index
   *  after refreshing the library, or 0 on failure. */
  int saveUserPattern (std::shared_ptr<Pattern> const &pattern);

  /** Number of system patterns. */
  int getNumSystemPatterns () const;

  /** Number of user patterns. */
  int getNumUserPatterns () const;

  juce::File const &getRootDir () const { return _rootDir; }
  juce::File getSystemDir () const { return _rootDir.getChildFile ("system"); }
  juce::File getUserDir () const { return _rootDir.getChildFile ("user"); }

  /** Return a fingerprint (hash) of all SVG files in system/ and user/.
   *  Changes whenever files are added, removed, or modified. */
  juce::int64 getDirectoryFingerprint () const;

private:
  void scanDirectory (juce::File const &dir, Category category);

  juce::File _rootDir;
  HeightMap const &_heightMap;
  std::vector<Entry> _entries;  ///< index 0 unused (Empty), 1..N = patterns
  int _numSystemPatterns = 0;
  int _numUserPatterns = 0;
};

}
