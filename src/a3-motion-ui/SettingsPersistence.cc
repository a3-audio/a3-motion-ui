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

#include "SettingsPersistence.hh"

#include <algorithm>

namespace a3
{

AppSettings
loadSettings (juce::File const &file)
{
  AppSettings settings;

  if (!file.existsAsFile ())
    return settings;

  juce::var parsed;
  if (juce::JSON::parse (file.loadFileAsString (), parsed).failed ())
    return settings;

  if (parsed.hasProperty ("clockMode"))
    settings.clockMode = static_cast<int> (parsed["clockMode"]);

  if (parsed.hasProperty ("recMode"))
    settings.recMode = recModeFromName (
        parsed["recMode"].toString ());

  // Entry by entry, and only as far as the file goes: a file naming fewer
  // keys than the device has says nothing about the rest, and a hand-edited
  // speed outside the range would sit on a key the drag cannot bring back.
  if (auto const *speeds = parsed["speedButtons"].getArray ())
    for (int i = 0; i < std::min (speeds->size (), numSpeedButtons); ++i)
      settings.speedButtonLog2[static_cast<size_t> (i)]
          = std::clamp (static_cast<int> ((*speeds)[i]), speedLog2Min,
                        speedLog2Max);

  return settings;
}

void
saveSettings (juce::File const &file, AppSettings const &settings)
{
  auto *obj = new juce::DynamicObject ();
  obj->setProperty ("clockMode", settings.clockMode);
  obj->setProperty ("recMode",
                    recModeName (settings.recMode));

  juce::Array<juce::var> speeds;
  for (auto const log2 : settings.speedButtonLog2)
    speeds.add (log2);
  obj->setProperty ("speedButtons", speeds);

  juce::var const state (obj);

  file.getParentDirectory ().createDirectory ();
  file.replaceWithText (juce::JSON::toString (state));
}

}
