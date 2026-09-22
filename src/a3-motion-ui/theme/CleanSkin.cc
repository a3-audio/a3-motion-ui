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

#include "CleanSkin.hh"

namespace a3
{

namespace
{
/** The skin to go back to, or empty when there is none but clean. */
juce::String
wayBack (juce::String const &remembered, juce::StringArray const &available)
{
  if (remembered != cleanSkinName && available.contains (remembered))
    return remembered;
  if (available.contains ("default"))
    return "default";
  for (auto const &name : available)
    if (name != cleanSkinName)
      return name;
  return {};
}
}

std::optional<CleanSkinToggle>
toggleCleanSkin (juce::String const &active, juce::String const &remembered,
                 juce::StringArray const &available)
{
  if (!available.contains (cleanSkinName))
    return std::nullopt;

  if (active != cleanSkinName)
    return CleanSkinToggle{ cleanSkinName, active };

  auto const back = wayBack (remembered, available);
  if (back.isEmpty ())
    return std::nullopt;

  return CleanSkinToggle{ back, remembered };
}

}
