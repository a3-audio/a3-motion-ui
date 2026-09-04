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

#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/util/Types.hh>

#include <optional>
#include <string>
#include <vector>

namespace a3
{

/** A clip: how a shape is played, and which shape that is.
 *
 *  The shape itself lives in its own file and is named here rather than
 *  embedded, so two clips can play the same figure at different speeds without
 *  a second copy of the geometry — and so a session can be carried to another
 *  device as a handful of small readable files.
 */
struct Clip
{
  std::string name;
  /** Names this clip used to have. Renaming rewrites the sessions it can see;
   *  a session it could not see — on another stick, in a backup — finds the
   *  clip again through these. */
  std::vector<std::string> aka;
  /** The shape's file name, without extension. */
  std::string svg;

  ClipSettings settings;
};

namespace ClipFile
{

bool save (Clip const &clip, juce::File const &file);

/** Nothing rather than a clip full of noise: a file that cannot be read is a
 *  slot that stays empty and says so, not one that comes up with invented
 *  values in the middle of a set. */
std::optional<Clip> load (juce::File const &file);

}

}
