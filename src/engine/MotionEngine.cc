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

#include "MotionEngine.hh"

namespace a3
{

MotionEngine::MotionEngine (int const numChannels)
{
  createChannels (numChannels);

  tempoClock.start ();

  //  juce::Thread::sleep (1000);

  //   tempoClock.stop ();
}

void
MotionEngine::createChannels (int const numChannels)
{
  channels.resize (numChannels);
}

std::vector<std::unique_ptr<Channel> > &
MotionEngine::getChannels ()
{
  return channels;
}

}
