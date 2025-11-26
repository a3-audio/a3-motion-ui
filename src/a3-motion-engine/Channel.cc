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

#include "Channel.hh"

#include <JuceHeader.h>

namespace a3
{

Channel::Channel () : _position (Pos::fromSpherical (0, 0, 1)) {}

void
Channel::setPosition (Pos position)
{
  _mutex.lock ();
  _position = position;
  _mutex.unlock ();
}

Pos
Channel::getPosition () const
{
  _mutex.lock_shared ();
  auto position = _position;
  _mutex.unlock_shared ();

  return position;
}

float
Channel::getPot1 () const
{
  return _pot1;
}

void
Channel::setPot1 (float pot1)
{
  _pot1 = pot1;
}

float
Channel::getPot2 () const
{
  return _pot2;
}

void
Channel::setPot2 (float pot2)
{
  _pot2 = pot2;
}

}
