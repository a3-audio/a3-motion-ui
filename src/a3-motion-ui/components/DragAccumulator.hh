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

namespace a3
{

/** Turns an ongoing drag into whole increments.
 *
 *  The caller reports how far the finger has moved since it went down —
 *  not since the last report. What comes back is how many steps have not
 *  been emitted yet: more than one when the drag was fast enough that
 *  events got coalesced, negative when the finger goes back.
 *
 *  Counting against what has already been emitted, rather than per event,
 *  is the whole point: per event a fast drag loses steps, and pushing a
 *  value to and fro drifts it.
 */
class DragAccumulator
{
public:
  explicit DragAccumulator (int pixelsPerStep);

  /** A new drag: the next report counts from zero again. */
  void reset ();

  /** The steps not yet emitted for this total movement. */
  int stepsFor (int pixelsMovedSinceStart);

  int emittedSteps () const { return _emittedSteps; }

private:
  int _pixelsPerStep;
  int _emittedSteps = 0;
};

}
