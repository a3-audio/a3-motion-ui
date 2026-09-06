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
 * the HeightMap at playback time by the MotionEngine.
 */
class PatternLibrary
{
public:
  enum class Category
  {
    System,
    User,
    /** A clip: a figure to play and every value it is played with. Listed
     *  from clips/ rather than from an SVG, so such an entry has no file, no
     *  path data and no ticks of its own -- it names its shape, and the shape
     *  entry beside it carries those.
     *
     *  System and User are the shapes themselves, which are their own kind of
     *  thing and have their own tab in the browser. */
    Clip
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
    /** The clip that reaches this shape, if one is beside it. A shape with no
     *  clip still loads -- somebody dropping an SVG into the folder by hand is
     *  a reasonable thing to do -- and gets the defaults.
     *
     *  Last on purpose: Entry is filled by aggregate initialisation in at
     *  least one place, so a field inserted in the middle silently renumbers
     *  every value after it. */
    juce::File clipFile;
    /** For a clip, the shape it is played on, by name. Empty on a shape
     *  entry, and on a clip written before a clip had to name one. */
    std::string svg;
  };

  /** Initialise with root directory containing system/ and user/ subdirs. */
  PatternLibrary (juce::File const &rootDir);

  /** Rescan both directories for .svg files.
   *  After this, getEntries() returns the updated list. */
  void refresh ();

  /** Total number of entries INCLUDING the implicit Empty at index 0. */
  int getNumEntries () const;

  /** Get entry at index (1-based, 0 = empty). */
  Entry const &getEntry (int index) const;

  /** Find the index for a pattern name.  Returns 0 if not found. */
  int indexForName (std::string const &name) const;

  /** Find the index of the entry a clip file belongs to. Returns 0 for an
   *  empty file or one no entry names.
   *
   *  This, not the name, is what says which row a slot came from: a settings
   *  preset leaves the shape alone, so the shape's name would point back at
   *  the shape's row however many presets were applied on top of it. */
  int indexForClipFile (juce::File const &file) const;

  /** Whether this entry is the instrument's rather than the performer's.
   *  Factory clips are never written to -- saving one makes a copy instead. */
  bool isFactory (int index) const;

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

  /** Number of clips listed without a shape. */
  int getNumSettingsPresets () const;

  juce::File const &getRootDir () const { return _rootDir; }
  juce::File getSystemDir () const { return _rootDir.getChildFile ("system"); }
  juce::File getUserDir () const { return _rootDir.getChildFile ("user"); }
  /** Where the clips live: beside the shapes, not inside them. A shape says
   *  where the sound goes and a clip says how it is played. */
  juce::File getClipDir () const { return _rootDir.getChildFile ("clips"); }

  /** Return a fingerprint (hash) of all SVG files in system/ and user/.
   *  Changes whenever files are added, removed, or modified. */
  juce::int64 getDirectoryFingerprint () const;

private:
  void scanDirectory (juce::File const &dir, Category category);
  /** Clips that name no shape, listed as settings presets. Runs after
   *  the shape passes so a clip that *does* name one is already in. */
  void scanSettingsPresets ();
  /** Sort the entries from @p firstOfCategory to the end by the name that
   *  is shown. Called once per scan, so the categories stay grouped. */
  void sortCategoryByName (size_t firstOfCategory);

  juce::File _rootDir;
  std::vector<Entry> _entries;  ///< index 0 unused (Empty), 1..N = patterns
  int _numSystemPatterns = 0;
  int _numUserPatterns = 0;
  int _numSettingsPresets = 0;
};

}
