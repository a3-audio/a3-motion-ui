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

#pragma once

#include <a3-motion-engine/flight/FlightField.hh>

#include <optional>

namespace a3
{

/** How heavy a group on the floor is. The tap on a body steps through them in
 *  this order, so the dead zone is one tap past the heaviest group. */
enum class BodyWeight
{
  Group,    // a few people
  Crowd,    // a dancing crowd
  Hotspot,  // "more of this here"
  DeadZone, // "not here"
};

float massOf (BodyWeight weight);     // 1, 2, 3, -2
BodyWeight nextWeight (BodyWeight weight); // G -> C -> H -> X -> G

/** The groups the DJ has placed on the floor. Message thread only; the engine
 *  gets a copy through snapshot ().
 *
 *  A body is named by its id, not its place in the list: an escort holds on
 *  to the id, and a removal moves the later bodies down a place but leaves
 *  their ids alone. A new body takes the lowest id that is free, so ids stay
 *  in 0..7 and the label (id + 1) in G1..G8. */
class FloorBodies
{
public:
  /** The new body's id, or nothing when the floor is full. */
  std::optional<int> add (Vec2 at);
  /** Clamped into the room (radius 1). */
  void move (int id, Vec2 at);
  void cycleWeight (int id);
  void remove (int id);

  bool contains (int id) const;
  BodyWeight weight (int id) const; // Group for an unknown id
  Vec2 at (int id) const;           // the centre for an unknown id
  int count () const;

  /** In the order they were placed, each with its id and mass. */
  FlightBodies snapshot () const;

private:
  struct Entry
  {
    Vec2 at;
    BodyWeight weight = BodyWeight::Group;
    int id = noBodyId;
  };

  Entry const *find (int id) const;
  Entry *find (int id);
  int lowestFreeId () const;

  std::array<Entry, maxFlightBodies> _entries{};
  int _count = 0;
};

}
