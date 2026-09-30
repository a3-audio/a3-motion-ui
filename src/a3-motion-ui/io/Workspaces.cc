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

#include "Workspaces.hh"

#include <optional>

namespace a3
{
namespace workspaces
{

namespace
{
/** Where the leading number of "3:REAPER" ends. */
int
digitsEnd (juce::String const &name)
{
  int end = 0;
  while (end < name.length () && juce::CharacterFunctions::isDigit (name[end]))
    ++end;
  return end;
}

std::optional<Workspace>
fromName (juce::String const &name, bool current)
{
  auto const end = digitsEnd (name);
  if (end == 0)
    return std::nullopt;

  auto const hasLabel = end < name.length () && name[end] == ':';
  return Workspace{ name.substring (0, end).getIntValue (),
                    hasLabel ? name.substring (end + 1) : name, current };
}
}

std::vector<Workspace>
parse (juce::String const &i3Reply)
{
  // Held in a named var: getArray() points into it. StemDeck read the list
  // through a temporary's and crashed on the tap (2026-09-30).
  auto const reply = juce::JSON::parse (i3Reply);
  auto const *entries = reply.getArray ();
  if (entries == nullptr)
    return {};

  std::vector<Workspace> out;
  for (auto const &entry : *entries)
    {
      if (!entry.isObject ())
        continue;
      if (auto workspace = fromName (entry["name"].toString (),
                                     static_cast<bool> (entry["focused"])))
        out.push_back (*workspace);
    }
  return out;
}

SwitcherGeometry
switcherGeometry (int windowWidth)
{
  auto const share = [windowWidth] (int atRigWidth) {
    return juce::roundToInt (atRigWidth * windowWidth / 768.0);
  };
  return { share (6),   share (30),  share (2),  share (80),
           share (40),  share (170), share (40), share (4) };
}

std::vector<Workspace>
list ()
{
  juce::ChildProcess i3;
  if (!i3.start (juce::StringArray{ "i3-msg", "-t", "get_workspaces" }))
    return {};

  return parse (i3.readAllProcessOutput ());
}

void
goTo (int number)
{
  juce::ChildProcess i3;
  if (!i3.start (juce::StringArray{ "i3-msg", "-q",
                                    "workspace number " + juce::String (number) }))
    juce::Logger::writeToLog ("workspaces: could not ask i3 for "
                              + juce::String (number));
}

}
}
