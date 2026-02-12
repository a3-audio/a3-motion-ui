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

#include <JuceHeader.h>

#include <a3-motion-engine/Pattern.hh>

namespace a3
{

/**
 * PatternFile handles serialisation/deserialisation of Pattern data
 * to/from JSON files on disk.
 *
 * File format (JSON):
 * {
 *   "name": "Circle",
 *   "lengthBeats": 16,
 *   "ticksPerBeat": 128,
 *   "ticks": [
 *     { "x": 0.8, "y": 0.0, "z": 0.5 },
 *     ...
 *     null,    // invalid tick (NaN jump position)
 *     ...
 *   ]
 * }
 */
class PatternFile
{
public:
  /** Save a Pattern's tick data to a JSON file.
   *  Returns true on success. */
  static bool save (std::shared_ptr<Pattern> const &pattern,
                    juce::File const &file);

  /** Load a Pattern from a JSON file.
   *  Returns nullptr on failure. The returned pattern has status Idle. */
  static std::shared_ptr<Pattern> load (juce::File const &file);

  /** Read only the name and tick data from a file without creating
   *  a full Pattern object.  Used for generating thumbnail icons.
   *  Returns the name, and fills outTicks.  Empty string on failure. */
  static std::string peekNameAndTicks (juce::File const &file,
                                       std::vector<Pos> &outTicks);
};

}
