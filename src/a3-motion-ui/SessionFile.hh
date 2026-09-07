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

#include <a3-motion-engine/ClipSettings.hh>

#include <optional>

#include <JuceHeader.h>

#include <string>
#include <vector>

namespace a3
{

/** The arrangement: what is where, rather than what anything *is*.
 *
 *  A take's own file carries what makes it that take — the movement, the
 *  elevation it is mapped through, its speed, its accent. None of that changes
 *  when you use it in another set. What *does* change is where it sits, how
 *  long the next take into that slot should be, and where the channels'
 *  filters and 3d are parked. That is a set, and a set is a thing you bring.
 *
 *  Which is the whole point of it being its own file next to the takes rather
 *  than something in the app's settings: a folder with a set and the takes it
 *  names is a gig on a stick.
 *
 *  Deliberately **not** here: the clock mode and the rec mode. The clock
 *  depends on what is plugged into the switch at the venue and the rec mode is
 *  a working habit; a set that changed either out from under you on load would
 *  be a surprise at the one moment nobody wants one. They stay device
 *  settings.
 */
struct Session
{
  struct Slot
  {
    /** The take's name, which is what the pattern library resolves by
     *  (`indexForName`) and what the take's own file carries as `data-name`.
     *  A set names its takes and they are looked for in the library beside
     *  it, so a stick carries its own and a name only has to be unique
     *  within one. An empty name is an empty slot.
     *
     *  Two takes with the same name on one stick would collide; the library
     *  has always had that limit, and a set inherits it rather than
     *  inventing a second way of naming the same thing. */
    std::string patternName;
    /** log2 of the length the next take into this slot will have, in bars. */
    int recordLengthLog2 = 0;

    /** The clip file this slot came from, by name and without its extension.
     *
     *  Beside the shape rather than instead of it: a settings preset carries
     *  no shape, and a shape can be in a slot with no clip beside it, so the
     *  two questions -- what is it playing, and where did its values come
     *  from -- have two answers. By name so a set can travel: a path would
     *  name a folder that is not there on the next stick.
     *
     *  Empty is not an error. A slot filled straight from a shape has no clip
     *  file, and says so. */
    std::string clipFile;

    /** The action this slot fires, by name and without its extension. Same
     *  reasoning as `clipFile`, and empty means ACT does the plain accent. */
    std::string action;

    /** Whether this slot was running when the set was written.
     *
     *  Whether, and nothing more: not how far through. Coming back mid-figure
     *  would put a set down somewhere other than the beginning of its own
     *  movement, and where it happened to be when somebody pressed Save is not
     *  a thing anybody chose. A slot that was running starts again from the
     *  top, on the next downbeat -- see applySet(). */
    bool playing = false;

    /** Everything this slot's clip is set to.
     *
     *  Written whenever there is a clip in the slot, not only when it differs
     *  from the clip's own file. A set is the whole state of an arrangement:
     *  it refers to clips rather than copying them, so two slots can hold the
     *  same clip and turning a control on one must not change the other --
     *  and a slot filled straight from a shape has no clip file to differ
     *  from at all, which is how its settings used to be dropped on the floor.
     *
     *  Still optional, and this is the one thing it says: a set written before
     *  this has none, and that must go on meaning "leave the clip's own
     *  settings alone" rather than "reset it to the defaults". Only the fields
     *  that differ from the *defaults* are written, so a file stays short and
     *  readable without anything being lost. */
    std::optional<ClipSettings> overrides;
  };

  struct Channel
  {
    float threeD = 0.f;
    float freq = 0.f;
    float q = 0.f;
    std::vector<Slot> slots;
  };

  /** What this session is called. Empty for the automatic one, which has no
   *  name because nobody chose it. */
  std::string name;

  std::vector<Channel> channels;
};

/** Reads a set. A missing or unreadable file is an empty set, not an error:
 *  a device with no set is a device somebody has not brought one to. */
Session loadSession (juce::File const &file, int numChannels, int numSlots);

bool saveSession (juce::File const &file, Session const &set);

/** Move a `set.json` written before sessions had names to `current.json`.
 *
 *  The old file is left where it is: a migration that deletes nothing can be
 *  run again when it goes wrong, and this one runs on every start. It does
 *  nothing once `current.json` exists, so a later `set.json` -- somebody's
 *  backup, dropped back in -- does not silently overwrite the session in use.
 *
 *  @returns true if it moved something. */
bool migrateSetToCurrent (juce::File const &root);

}
