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

/** Where Motion's own pictures and grids are: resources/ beside the
 *  executable when it is there (build.sh links it beside the binary of a
 *  checkout or worktree), else <exe>/../share/a3-motion-ui/resources (the
 *  package: /usr/bin -> /usr/share/a3-motion-ui). Never the working
 *  directory: the package runs Motion in the user's data folder
 *  (~/.local/share/a3-motion), which holds no resources (2026-10-08).
 */
juce::File resourceDirectory (juce::File const &executable);

/** The debug log: $XDG_STATE_HOME/a3-motion/a3-motion-ui.log, or
 *  ~/.local/state/a3-motion/a3-motion-ui.log when that is unset or not
 *  absolute. Not beside the executable: /usr/bin is not writable. Pure: home
 *  and $XDG_STATE_HOME are passed in.
 */
juce::File logFile (juce::String const &home, juce::String const &xdgStateHome);

} // namespace a3
