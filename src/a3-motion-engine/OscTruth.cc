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


#include "OscTruth.hh"
#include "TruthKeeper.hh"

namespace a3
{

juce::String
OscTruth::host (juce::String const &name) const
{
  return _data["hosts"][juce::Identifier (name)].toString ();
}

juce::var
OscTruth::listenerFor (juce::String const &program,
                       juce::String const &role) const
{
  if (auto const *listeners = _data["listeners"].getArray ())
    for (auto const &listener : *listeners)
      if (listener["program"].toString () == program
          && listener["role"].toString () == role)
        return listener;
  return {};
}

int
OscTruth::port (juce::String const &program, juce::String const &role) const
{
  auto const listener = listenerFor (program, role);
  return listener.isObject () ? static_cast<int> (listener["port"]) : -1;
}

std::optional<OscTruth::Endpoint>
OscTruth::endpoint (juce::String const &program,
                    juce::String const &role) const
{
  auto const listener = listenerFor (program, role);
  if (!listener.isObject ())
    return std::nullopt;

  auto hostName = listener["host"].toString ();
  if (hostName == "any")
    hostName = "local";

  auto const ip = host (hostName);
  if (ip.isEmpty ())
    return std::nullopt;

  return Endpoint{ ip, static_cast<int> (listener["port"]) };
}

juce::String
OscTruth::pattern (juce::String const &key) const
{
  return _data["addresses"][juce::Identifier (key)]["pattern"].toString ();
}

int
OscTruth::vuNumber (juce::String const &meterName) const
{
  if (auto const *meters = _data["vu_meters"].getArray ())
    for (int i = 0; i < meters->size (); ++i)
      if (meters->getReference (i).toString () == meterName)
        return i + 1;
  return 0;
}

OscTruth
parseOscTruth (juce::String const &json)
{
  OscTruth truth;
  auto const result = juce::JSON::parse (json, truth._data);
  if (result.failed ())
    truth._error = "a3-osc.json does not parse: " + result.getErrorMessage ();
  else if (!truth._data.isObject ())
    truth._error = "a3-osc.json is not an object";
  return truth;
}

OscTruth
loadOscTruth (juce::File const &file)
{
  if (!file.existsAsFile ())
    {
      OscTruth truth;
      truth._error = "no OSC truth at " + file.getFullPathName ();
      return truth;
    }
  return parseOscTruth (file.loadFileAsString ());
}

juce::File
oscTruthFile ()
{
  auto const overridden
      = juce::SystemStats::getEnvironmentVariable ("A3_OSC_TRUTH", {});
  return oscTruthFileFrom (
      overridden.isNotEmpty () ? overridden.toRawUTF8 () : nullptr,
      juce::File::getSpecialLocation (juce::File::userHomeDirectory));
}

juce::File
oscTruthFileFrom (char const *override, juce::File const &home)
{
  auto const cache
      = truthkeeper::cachePath (home.getFullPathName ().toStdString ());
  juce::var parsed;
  auto const cached = juce::File (cache);
  auto const usable
      = cached.existsAsFile ()
        && juce::JSON::parse (cached.loadFileAsString (), parsed).wasOk ()
        && parsed["addresses"].isObject ();
  return juce::File (truthkeeper::startPath (
      override, cache, usable, "/usr/share/a3/a3-osc.json"));
}

OscTruth const &
installedOscTruth ()
{
  static OscTruth const truth = loadOscTruth (oscTruthFile ());
  return truth;
}

}
