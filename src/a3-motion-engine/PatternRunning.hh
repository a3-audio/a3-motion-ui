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

#include <a3-motion-engine/Pattern.hh>

namespace a3
{

/** Whether the blob is being moved along this pattern right now.
 *
 *  Not "was it started" and not "will it be": *is something travelling along
 *  it at this moment*. Three statuses answer yes, and the third is the one
 *  worth writing down -- `ScheduledForIdle` is a pattern that has been asked
 *  to stop and has not stopped yet. It goes on playing, sometimes for most of
 *  a phrase, and `MotionComponent` drew nothing for it because the draw loop
 *  counted `Playing` and `Recording` by hand.
 *
 *  The rest of the UI already reads the state this way -- the pad colours, the
 *  blinking key, the "anything moving" check that keeps the settings bar
 *  refreshing. This is that same sentence, said once, where a test can reach
 *  it.
 */
bool patternIsRunning (Pattern::Status status);

}
