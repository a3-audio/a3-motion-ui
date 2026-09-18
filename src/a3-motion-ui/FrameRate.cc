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

#include "FrameRate.hh"

namespace a3
{

FrameRate::FrameRate (double reportEverySeconds)
    : _window (reportEverySeconds)
{
}

juce::String
FrameRate::tick (double nowSeconds)
{
  ++_frames;

  // The first frame starts the window rather than ending one: taken as a
  // report it would divide by however long the app took to come up.
  if (_since < 0.0)
    {
      _since = nowSeconds;
      _frames = 0;
      return {};
    }

  auto const elapsed = nowSeconds - _since;
  if (elapsed < _window || elapsed <= 0.0)
    return {};

  auto const rate = static_cast<double> (_frames) / elapsed;
  _frames = 0;
  _since = nowSeconds;

  return "frames: " + juce::String (rate, 1) + " a second";
}

bool
FrameRate::wanted ()
{
  return juce::SystemStats::getEnvironmentVariable ("A3_FRAME_TRACE", "")
      .isNotEmpty ();
}

}
