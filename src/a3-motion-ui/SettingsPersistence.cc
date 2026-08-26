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
  if (parsed.hasProperty ("potSizeIndex"))
    settings.potSizeIndex = static_cast<int> (parsed["potSizeIndex"]);
  // A file written before the two sizes were split carries one index for
  // both. Reading it as the header's alone would silently shrink every
  // caption on the device.
  if (parsed.hasProperty ("fontSizeIndex"))
    {
      auto const legacy = static_cast<int> (parsed["fontSizeIndex"]);
      settings.headerSizeIndex = legacy;
      settings.bodySizeIndex = legacy;
    }
  if (parsed.hasProperty ("headerSizeIndex"))
    settings.headerSizeIndex = static_cast<int> (parsed["headerSizeIndex"]);
  if (parsed.hasProperty ("bodySizeIndex"))
    settings.bodySizeIndex = static_cast<int> (parsed["bodySizeIndex"]);

  return settings;
}

void
saveSettings (juce::File const &file, AppSettings const &settings)
{
  auto *obj = new juce::DynamicObject ();
  obj->setProperty ("clockMode", settings.clockMode);
  obj->setProperty ("potSizeIndex", settings.potSizeIndex);
  obj->setProperty ("headerSizeIndex", settings.headerSizeIndex);
  obj->setProperty ("bodySizeIndex", settings.bodySizeIndex);
  juce::var const state (obj);

  file.getParentDirectory ().createDirectory ();
  file.replaceWithText (juce::JSON::toString (state));
}

}
