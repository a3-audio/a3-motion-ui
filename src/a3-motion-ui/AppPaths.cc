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
#include "AppPaths.hh"

namespace a3
{

juce::File
resourceDirectory (juce::File const &executable)
{
  auto const beside = executable.getParentDirectory ().getChildFile ("resources");
  if (beside.isDirectory ())
    return beside;
  return executable.getParentDirectory ().getParentDirectory ().getChildFile (
      "share/a3-motion-ui/resources");
}

juce::File
logFile (juce::String const &home, juce::String const &xdgStateHome)
{
  auto const base = juce::File::isAbsolutePath (xdgStateHome)
                        ? juce::File (xdgStateHome)
                        : juce::File (home).getChildFile (".local/state");
  return base.getChildFile ("a3-motion").getChildFile ("a3-motion-ui.log");
}

} // namespace a3
