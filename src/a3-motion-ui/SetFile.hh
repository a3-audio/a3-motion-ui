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
struct SetFile
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
  };

  struct Channel
  {
    float threeD = 0.f;
    float freq = 0.f;
    float q = 0.f;
    std::vector<Slot> slots;
  };

  std::vector<Channel> channels;
};

/** Reads a set. A missing or unreadable file is an empty set, not an error:
 *  a device with no set is a device somebody has not brought one to. */
SetFile loadSet (juce::File const &file, int numChannels, int numSlots);

bool saveSet (juce::File const &file, SetFile const &set);

}
