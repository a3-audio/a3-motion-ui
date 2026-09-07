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

namespace a3
{

/** Lift the settings out of takes that were saved with them inside.
 *
 *  Every take written before clips existed carries its settings in the SVG.
 *  This reads each one, writes a clip file beside it, and **leaves the take
 *  exactly where it was** — a migration that deletes nothing can be run again
 *  when it goes wrong, and it runs on every start.
 *
 *  The shape is not moved either. `PatternLibrary` looks in `user/`, so a take
 *  moved to a new folder now would simply vanish; moving the folders is a
 *  later step of its own.
 *
 *  One thing it cannot do: the fade is baked into the geometry of an old take
 *  and cannot be recovered, so the clip claims none. The take keeps sounding
 *  exactly as it did, and nothing pretends the fade is still adjustable when
 *  the material it ate is gone.
 *
 *  @returns how many takes were migrated; a take that already has a clip is
 *           left alone, so a second run returns zero.
 */
int migrateCombinedPatterns (juce::File const &root);

}
