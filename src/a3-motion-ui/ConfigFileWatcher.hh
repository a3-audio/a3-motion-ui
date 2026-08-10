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

/** Reports whether a file has been rewritten since the last check.
 *
 *  Used to pick up edits to config.json while the app runs, so the visual
 *  tuning values can be adjusted without a rebuild-and-restart cycle. Compares
 *  modification time and size — an editor that rewrites a file within the same
 *  timestamp tick almost always changes its length too. */
class ConfigFileWatcher
{
public:
  explicit ConfigFileWatcher (juce::File const &file);

  /** True exactly once per change. A file that does not exist never reports a
   *  change, so a truncated intermediate save cannot spin the caller. */
  bool hasChanged ();

private:
  juce::File _file;
  juce::int64 _modificationTime = 0;
  juce::int64 _size = 0;
};

}
