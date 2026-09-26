/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

/** What the transport keys do around a take (2026-09-26).
 *
 *  ● no longer starts a take outright. It arms the shown slot -- REC PAUSE --
 *  where the take is set up on the REC page (length, rec mode, fade, bias),
 *  and ▶ starts it. The clip on that slot keeps playing while it is armed.
 *  The panel's REC + Play|Pause pad still starts a take directly: there the
 *  pad already names the slot, and the setting up happens on the screen. */
enum class RecKeyAction
{
  Save,
  EndTake,
  Disarm,
  Arm,
};

enum class PlayKeyAction
{
  StartTake,
  PlayPause,
};

/** What ● does. SAVE first -- the key wears it while an unsaved take waits --
 *  then ending a take that runs or waits for its downbeat, then disarming,
 *  and from idle, arming. */
constexpr RecKeyAction
recKeyAction (bool saveOffered, bool takeUnderway, bool armed)
{
  if (saveOffered)
    return RecKeyAction::Save;
  if (takeUnderway)
    return RecKeyAction::EndTake;
  return armed ? RecKeyAction::Disarm : RecKeyAction::Arm;
}

/** What ▶ does: armed, it starts the take; otherwise it plays and pauses. */
constexpr PlayKeyAction
playKeyAction (bool armed)
{
  return armed ? PlayKeyAction::StartTake : PlayKeyAction::PlayPause;
}

/** Whether ■ only takes the arming back: a way out that writes nothing. */
constexpr bool
stopKeyDisarms (bool armed)
{
  return armed;
}

}
