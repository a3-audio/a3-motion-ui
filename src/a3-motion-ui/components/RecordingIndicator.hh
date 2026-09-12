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

/** What the status bar's recording mark is doing.
 *
 *  Three states and not two, because pressing REC and the take starting are
 *  not the same moment: a take begins on the next downbeat, which at a slow
 *  tempo is a second away. In that window the hand has already committed and
 *  now has to know *when* to start moving -- too early and the movement is
 *  not in the take, too late and the take opens with nothing in it.
 *
 *  So the window counts. It is the same mark in the same place, in the
 *  channel's colour, blinking on each beat and then filling once the take
 *  runs: one indicator with two states rather than two indicators, so the eye
 *  does not have to move at the moment it can least afford to.
 */
enum class RecordingIndicator
{
  Off,
  CountIn,
  Running
};

/** Which of the three, from what the engine has. Running wins: a blink
 *  carrying on under a running take would be the loudest thing on the screen
 *  saying the least, and this is the state that writes over something that
 *  does not come back. */
RecordingIndicator recordingIndicatorFor (bool scheduled, bool recording);

}
