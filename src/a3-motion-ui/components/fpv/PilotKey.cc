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

#include "PilotKey.hh"

namespace a3
{

PilotKeyFace
pilotKeyFace (PilotLevel level, bool fpv)
{
  PilotKeyFace face;
  face.available = fpv;
  switch (level)
    {
    case PilotLevel::Off:
      face.word = "PILOTS";
      break;
    case PilotLevel::Hint:
      face.word = "HINT";
      face.on = fpv;
      break;
    case PilotLevel::Fly:
      face.word = "FLY";
      face.on = fpv;
      face.notice = fpv;
      break;
    }
  return face;
}

PilotLevel
levelAfterTap (PilotLevel level, bool firesAGame, bool shipInAPilotsGame)
{
  if (level == PilotLevel::Fly && (firesAGame || shipInAPilotsGame))
    return PilotLevel::Hint;
  return level;
}

juce::String
pilotLevelReadout (PilotLevel level)
{
  return "-- PILOTS " + pilotLevelWord (level).toUpperCase ();
}

juce::String
pilotGameReadout (int channel, PilotGame game)
{
  return "CH" + juce::String (channel + 1) + " FLY " + pilotWord (game).toUpperCase ();
}

}
