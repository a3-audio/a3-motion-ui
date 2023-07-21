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

#pragma once

#include <memory>
#include <shared_mutex>

#include <a3-motion-engine/util/Geometry.hh>

#include <a3-motion-engine/elevation/HeightMap.hh>

namespace a3
{

class Channel
{
public:
  Channel ();

  void setPosition (Pos const &position);
  Pos getPosition () const;

  void recomputeHeight ();

private:
  Pos _position;
  std::unique_ptr<HeightMap> _heightMap;

  // for now we use a "fair" RW lock using std::shared_mutex to see
  // how it performs.  we might implement a write-preferring RW lock
  // based on the example at
  // https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock#Using_a_condition_variable_and_a_mutex
  mutable std::shared_mutex _mutex;
};

}
