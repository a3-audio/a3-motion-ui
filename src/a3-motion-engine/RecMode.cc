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

#include "RecMode.hh"

namespace a3
{

bool
shouldWriteTick (RecMode mode, FingerHistory const &finger)
{
  if (finger.down)
    return true;

  // The lap the finger was lifted in is the one the hold belongs to. Without
  // a length there is no lap to end at, and the old behaviour stands.
  auto const holdIsStillInItsLap
      = finger.lapTicks <= 0
        || (finger.ticksAtLift / finger.lapTicks)
               == (finger.ticksNow / finger.lapTicks);

  switch (mode)
    {
    case RecMode::Touch:
      return false;
    case RecMode::Latch:
      return finger.hasTouched && holdIsStillInItsLap;
    case RecMode::Write:
      return holdIsStillInItsLap;
    }

  return false;
}

bool
showsRecordingUnderlay (RecMode mode, bool recording, bool hasPrevious)
{
  return recording && hasPrevious && mode != RecMode::Write;
}

juce::String
recModeName (RecMode mode)
{
  switch (mode)
    {
    case RecMode::Touch:
      return "Touch";
    case RecMode::Latch:
      return "Latch";
    case RecMode::Write:
      return "Write";
    }

  return "Touch";
}

RecMode
recModeFromName (juce::String const &name)
{
  if (name == "Latch")
    return RecMode::Latch;
  if (name == "Write")
    return RecMode::Write;
  return RecMode::Touch;
}

}
