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
 *     <circle cx="0.5" cy="0.3" r="0.05"/>  <!-- jump-dot patterns -->
 *   </svg>
 *
 * Continuous patterns are stored with Catmull-Rom→Bézier curves
 * and a palindrome (forward+backward) approach for seamless loops.
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
};

}
