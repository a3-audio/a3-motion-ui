/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include <a3-motion-engine/util/Types.hh>

#include <optional>
#include <vector>

namespace a3
{

/** A clip's place: which channel, which of its slots. */
struct SlotRef
{
  index_t channel;
  index_t slot;

  bool
  operator== (SlotRef const &other) const
  {
    return channel == other.channel && slot == other.slot;
  }
};

/** Every slot whose action is `file`, in channel then slot order. Saving a
 *  script re-runs it on all of them, not only the shown one. */
std::vector<SlotRef>
slotsFiring (juce::File const &file,
             std::vector<std::vector<juce::File>> const &slotFiles);

/** Whether the FILES list waits -- a row tap, Rename, Delete -- because the
 *  script beside it has changes that are neither saved nor cancelled. */
bool listWaitsFor (bool scriptHasUnsavedChanges);

/** Which clip a Save as points at the copy: the one EDIT on ACTION came from,
 *  and nobody when FILES was opened from its own key. Taken, not read: the
 *  first Save as of a visit uses it up, so a copy of another script made
 *  afterwards re-points nobody. */
std::optional<SlotRef> takeEditOrigin (std::optional<SlotRef> &editOrigin);

/** What the script panel does when the list's chosen row is not the file it
 *  holds. */
enum class PanelSync
{
  /** The same file: nothing to do, unsaved text included. */
  Keep,
  /** Another file and nothing unsaved: show that one. */
  Reload,
  /** Another file but unsaved text: the row goes back to the panel's file,
   *  so the text can never be saved into a file it did not come from. */
  HoldRow,
};

PanelSync panelSyncFor (juce::File const &chosen, juce::File const &inPanel,
                        bool unsaved);

/** What a script gets wrong, line by line, for the panel's error strip. */
juce::StringArray scriptErrorsOf (juce::String const &script);

/** What any file in FILES gets wrong, read as the kind its extension says:
 *  a script is run (scriptErrorsOf), a set parsed as JSON, a clip or a shape
 *  parsed as XML. */
juce::StringArray fileErrorsOf (juce::String const &text,
                                juce::File const &file);

/** Whether those errors keep Save from writing. A set or an SVG that does not
 *  parse would make the file unloadable; a script with an error still runs
 *  the lines that read, as it always has. */
bool errorsBlockSaving (juce::StringArray const &errors,
                        juce::File const &file);

/** What a copy is named after: the file it came from ("Bloom" gives
 *  "Bloom 2" through freeFileIn), or "Action" for one with no origin. */
juce::String copyBaseFor (juce::File const &from);

/** Where a Cue button's clip lives. `file` is empty for a script without a
 *  `~clip` line, and for one naming a clip that does not exist -- then `error`
 *  says which, for the editor's strip, and the button does nothing. */
struct CueTarget
{
  juce::File file;
  juce::String error;
};

/** The clip a script's `~clip` names, looked up in the clips folder the way
 *  FILES looks it up (shipped half, then the user's). */
CueTarget cueClipFor (std::optional<juce::String> const &clip,
                      juce::File const &clipsDir);

}
