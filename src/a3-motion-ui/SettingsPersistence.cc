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
#include <a3-motion-engine/TextFile.hh>

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

  if (parsed.hasProperty ("developerMode"))
    settings.developerMode = static_cast<bool> (parsed["developerMode"]);

  if (parsed.hasProperty ("skinBeforeClean"))
    settings.skinBeforeClean = parsed["skinBeforeClean"].toString ();

  // Held to the limits a gesture has, whatever wrote the file: never seen
  // from below or upside down, never zoomed past the ends.
  if (parsed.hasProperty ("cameraPitch"))
    settings.cameraPitch
        = std::clamp (static_cast<float> (parsed["cameraPitch"]), 0.f,
                      juce::MathConstants<float>::halfPi);
  if (parsed.hasProperty ("cameraTurn"))
    settings.cameraTurn = static_cast<float> (parsed["cameraTurn"]);
  if (parsed.hasProperty ("cameraZoom"))
    settings.cameraZoom
        = std::clamp (static_cast<float> (parsed["cameraZoom"]), minCameraZoom,
                      maxCameraZoom);
  // Eight encoders, a bit each: anything past them is not a click.
  if (parsed.hasProperty ("encoderClicksMotion"))
    settings.encoderClicksMotion
        = static_cast<int> (parsed["encoderClicksMotion"]) & 0xff;
  if (parsed.hasProperty ("encoderClicksRecord"))
    settings.encoderClicksRecord
        = static_cast<int> (parsed["encoderClicksRecord"]) & 0xff;

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
  obj->setProperty ("developerMode", settings.developerMode);
  obj->setProperty ("skinBeforeClean", settings.skinBeforeClean);
  obj->setProperty ("cameraPitch", settings.cameraPitch);
  obj->setProperty ("cameraTurn", settings.cameraTurn);
  obj->setProperty ("cameraZoom", settings.cameraZoom);
  obj->setProperty ("encoderClicksMotion", settings.encoderClicksMotion);
  obj->setProperty ("encoderClicksRecord", settings.encoderClicksRecord);

  juce::var const state (obj);

  file.getParentDirectory ().createDirectory ();
  writeTextFile (file, juce::JSON::toString (state));
}

}
