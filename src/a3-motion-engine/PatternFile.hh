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
#include <utility>

#include <JuceHeader.h>

#include <a3-motion-engine/Pattern.hh>

namespace a3
{

/**
 * PatternFile handles serialisation/deserialisation of Pattern data
 * to/from SVG files on disk.
 *
 * File format (SVG):
 *   <svg xmlns="..." viewBox="-1 -1 2 2"
 *        data-name="Circle" data-beats="16" data-ppqn="128">
 *     <path d="M ... C ..." fill="none" stroke="black"/>
 *     <circle cx="0.5" cy="0.3" r="0.05" data-at="0"/>  <!-- jump dots -->
 *   </svg>
 *
 * A jump dot is one hit (#61): data-at is the tick (of data-ppqn) it is
 * landed on, it holds until the next one, and the tick before each landing
 * stays empty. A dot is written per hit, so a place landed on twice has two.
 * Dots without data-at -- every file written before #61, and any file where
 * only some carry it -- are spread evenly over the clip, as they always were.
 *
 * Continuous patterns are stored with Catmull-Rom→Bézier curves
 * and a palindrome (forward+backward) approach for seamless loops.
 *
 * A take (Pattern::isTake()) is stored differently (#68): the svg carries
 * data-kind="take" and its path is a polyline of every tick in the
 * coordinates it plays in -- no scaling, no thinning, no palindrome:
 *     <path d="M x y L x y ..." data-ticks="0 340 760"/>
 * one subpath per run, data-ticks naming the tick each one starts at. A
 * tapped take adds its taps as circles, for the picture. A file without
 * data-kind is read as a shape, which is how a take written before #68
 * still loads.
 *
 * Note: Z (height) is NOT stored — it is computed live by the
 * HeightMap in PatternLibrary when loading patterns for playback.
 */
class PatternFile
{
public:
  /** Save a Pattern's tick data to an SVG file.
   *  Returns true on success. */
  static bool save (std::shared_ptr<Pattern> const &pattern,
                    juce::File const &file);

  /** Load a Pattern from an SVG file.
   *  Returns nullptr on failure. The returned pattern has status Idle. */
  static std::shared_ptr<Pattern> load (juce::File const &file);

  /** Peek result — lightweight data extracted from an SVG file. */
  struct PeekResult
  {
    std::string name;
    std::string pathData;     ///< SVG path 'd' attribute string
    std::vector<std::pair<float,float>> jumpDots;
    int lengthBeats = 0;
  };

  /** Read metadata from an SVG file without creating a full Pattern.
   *  Returns empty name on failure. */
  static PeekResult peek (juce::File const &file);

  /** Write a new name into an existing pattern file, leaving everything else
   *  in it exactly as it was.
   *
   *  One attribute, not a re-save. Renaming by loading and saving again would
   *  re-derive the path from the ticks and hand back a file that is nearly,
   *  but not quite, the one that was read -- and a rename is the one operation
   *  that must not change the shape. */
  static bool setName (juce::File const &file, juce::String const &name);
};

}
