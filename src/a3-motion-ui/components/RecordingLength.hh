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

/** How many beats the next take runs for.
 *
 *  **The length the shown clip is set to play at** -- which is the speed key
 *  that is lit, asked for at the device on 2026-09-23. Those four keys do not
 *  name speeds, they name *lengths*: each shows how many beats the clip runs
 *  for at that rate (`speedKeyName` is `beatsName (playbackLengthBeats (...))`).
 *  So picking a key picks the length, and REC records that long.
 *
 *  It must be the **effective** length and not the pattern's raw one. Reading
 *  `getPlaybackLength()` instead -- which this did for half a day -- takes the
 *  take as it was recorded and ignores the key, so the number never changed
 *  however many keys were pressed, and there was no way left to say "the next
 *  one is shorter". Reported from the device in exactly those words: "ich kann
 *  sie nach der aufnahme aber nicht mehr ändern".
 *
 *  This reverses a decision, and the reversal is the point rather than an
 *  oversight. Recording used to take a length of its own -- eight keys on the
 *  back of the Shape card, "the length set for the next take, not whatever
 *  the slot happens to hold" -- and that card is gone with the REC tab. What
 *  it bought was the ability to record a *different* length over an existing
 *  clip; what it cost was a page, a mode and a number to keep in step. The
 *  first is the rarer wish.
 *
 *  `clipLengthBeats` is what the shown pattern *plays* -- its own length taken
 *  through its speed -- or 0 for a slot with nothing in it. Then, and only then, the slot's last used length decides --
 *  the value the eight keys used to set, which a set still carries, so the
 *  first take into an empty slot is as long as the last one was.
 *
 *  Never zero: a take of no length is a take that ends before the downbeat it
 *  started on.
 */
int recordingLengthBeats (int clipLengthBeats, int storedRecordLengthLog2,
                          int beatsPerBar);

}
