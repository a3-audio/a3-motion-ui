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

#include <JuceHeader.h>

namespace a3
{

/** How many frames a second the sphere is actually managing.
 *
 *  Kept because every question about what the picture may cost is a question
 *  about this number, and twice now it has been answered by building a
 *  throwaway counter, reading it, and deleting it again. The rate is also the
 *  only honest measure here: the process sits at ninety per cent of a core
 *  whether it is drawing thirty frames or twenty, because what it does with
 *  the rest is wait for the vertical blank.
 *
 *  Off unless `A3_FRAME_TRACE` is set in the environment, like the beat and
 *  recording traces.
 */
class FrameRate
{
public:
  explicit FrameRate (double reportEverySeconds = 2.0);

  /** Counts one frame drawn at `nowSeconds`, and returns the line to log
   *  once the window has gone by — empty the rest of the time. */
  juce::String tick (double nowSeconds);

  /** Whether A3_FRAME_TRACE asks for this at all. */
  static bool wanted ();

private:
  double _window;
  double _since = -1.0;
  int _frames = 0;
};

}
