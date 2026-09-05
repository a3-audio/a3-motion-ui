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

class Pattern;

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
  /** The shape's file name, without extension — or empty.
   *
   *  Empty means a settings preset: applying it leaves the slot's shape where
   *  it is and changes only how it is played. The shape is chosen in the Shape
   *  section's own list, so a browser full of clips that differ only in their
   *  shape says nothing about the settings, which is what the browser is for. */
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

/** Whether a pattern has drifted from the clip it was filled from.
 *
 *  Worked out by comparing, not by watching: turning a control and turning it
 *  back leaves nothing behind, which a flag set on every touch could not
 *  manage. That is the whole reason this is a question rather than a state.
 *
 *  A pattern with no clip behind it -- an empty slot, or one filled from a
 *  shape that has none -- has nothing to have drifted from, so it has not.
 */
bool clipHasDrifted (Pattern const &pattern, juce::File const &clipFile);

/** Write a pattern's settings back into the clip it came from.
 *
 *  Keeps everything about the clip that is not a setting: its name, the shape
 *  it points at, and the names it used to have. Rewriting the file from
 *  scratch would drop the former names, and with them every session elsewhere
 *  that still refers to the clip by one of them -- a loss that would show up
 *  weeks later on somebody else's stick.
 *
 *  @returns false if the clip cannot be read or cannot be written.
 */
bool saveClipSettings (Pattern const &pattern, juce::File const &clipFile);

/** A name nobody is using yet: `base`, else `base 2`, `base 3`, ...
 *
 *  Names are the identity here, so two clips may not share one -- and the
 *  browser shows a single list, where two rows called the same thing could not
 *  be told apart. The extension is a parameter because actions are counted
 *  the same way and are not clips: one rule for naming, two kinds of file. */
juce::String freeClipName (juce::File const &clipDir,
                           juce::String const &base,
                           juce::String const &extension = ".json");

}
