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

#include "TouchGrabs.hh"

namespace a3
{

void
TouchGrabs::down (int source, std::optional<index_t> channel)
{
  _bySource[source] = channel;
}

std::optional<index_t>
TouchGrabs::channelFor (int source) const
{
  auto const entry = _bySource.find (source);
  if (entry == _bySource.end ())
    return {};

  return entry->second;
}

bool
TouchGrabs::isHeld (index_t channel) const
{
  for (auto const &entry : _bySource)
    if (entry.second.has_value () && entry.second.value () == channel)
      return true;

  return false;
}

std::optional<index_t>
TouchGrabs::up (int source)
{
  auto const entry = _bySource.find (source);
  if (entry == _bySource.end ())
    return {};

  auto const channel = entry->second;
  _bySource.erase (entry);

  return channel;
}

bool
TouchGrabs::empty () const
{
  return _bySource.empty ();
}

std::vector<index_t>
TouchGrabs::heldChannels () const
{
  std::vector<index_t> held;
  for (auto const &entry : _bySource)
    if (entry.second.has_value ())
      held.push_back (entry.second.value ());

  return held;
}

}
