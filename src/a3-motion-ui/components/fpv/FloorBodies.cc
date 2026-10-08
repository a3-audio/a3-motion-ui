/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include "FloorBodies.hh"

#include <a3-motion-engine/flight/FlightTuning.hh>

#include <algorithm>
#include <utility>

namespace a3
{

namespace
{
Vec2
insideTheRoom (Vec2 at)
{
  auto const distance = at.getDistanceFromOrigin ();
  return distance > 1.f ? at / distance : at;
}
}

float
massOf (BodyWeight weight)
{
  FlightTuning const tuning;
  switch (weight)
    {
    case BodyWeight::Group:
      return tuning.groupMass;
    case BodyWeight::Crowd:
      return tuning.crowdMass;
    case BodyWeight::Hotspot:
      return tuning.hotspotMass;
    case BodyWeight::DeadZone:
      return tuning.deadZoneMass;
    }
  return tuning.groupMass;
}

BodyWeight
nextWeight (BodyWeight weight)
{
  switch (weight)
    {
    case BodyWeight::Group:
      return BodyWeight::Crowd;
    case BodyWeight::Crowd:
      return BodyWeight::Hotspot;
    case BodyWeight::Hotspot:
      return BodyWeight::DeadZone;
    case BodyWeight::DeadZone:
      return BodyWeight::Group;
    }
  return BodyWeight::Group;
}

std::optional<int>
FloorBodies::add (Vec2 at)
{
  if (_count >= maxFlightBodies)
    return std::nullopt;

  auto const id = lowestFreeId ();
  _entries[static_cast<size_t> (_count)] = { insideTheRoom (at),
                                             BodyWeight::Group, id };
  ++_count;
  return id;
}

void
FloorBodies::move (int id, Vec2 at)
{
  if (auto *entry = find (id))
    entry->at = insideTheRoom (at);
}

void
FloorBodies::cycleWeight (int id)
{
  if (auto *entry = find (id))
    entry->weight = nextWeight (entry->weight);
}

void
FloorBodies::remove (int id)
{
  auto *entry = find (id);
  if (entry == nullptr)
    return;

  auto const end = _entries.begin () + _count;
  std::move (entry + 1, end, entry);
  --_count;
}

bool
FloorBodies::contains (int id) const
{
  return find (id) != nullptr;
}

BodyWeight
FloorBodies::weight (int id) const
{
  auto const *entry = find (id);
  return entry != nullptr ? entry->weight : BodyWeight::Group;
}

Vec2
FloorBodies::at (int id) const
{
  auto const *entry = find (id);
  return entry != nullptr ? entry->at : Vec2{};
}

int
FloorBodies::count () const
{
  return _count;
}

FlightBodies
FloorBodies::snapshot () const
{
  FlightBodies bodies;
  for (int i = 0; i < _count; ++i)
    {
      auto const &entry = _entries[static_cast<size_t> (i)];
      bodies.body[static_cast<size_t> (i)]
          = { entry.at, massOf (entry.weight), entry.id };
    }
  bodies.count = _count;
  return bodies;
}

FloorBodies::Entry const *
FloorBodies::find (int id) const
{
  auto const end = _entries.begin () + _count;
  auto const it = std::find_if (_entries.begin (), end,
                                [id] (Entry const &e) { return e.id == id; });
  return it != end ? &*it : nullptr;
}

FloorBodies::Entry *
FloorBodies::find (int id)
{
  return const_cast<Entry *> (std::as_const (*this).find (id));
}

int
FloorBodies::lowestFreeId () const
{
  for (int id = 0; id < maxFlightBodies; ++id)
    if (!contains (id))
      return id;
  return noBodyId;
}

}
