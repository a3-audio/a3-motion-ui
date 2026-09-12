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

#include <a3-motion-engine/ClipSettings.hh>

namespace a3
{

/** Which of the clip bar's three sections is being held.
 *
 *  A held section is one nothing writes over: step through clips with the
 *  elevation held and every figure arrives in the room you are already in.
 *  It is what the settings presets used to do -- land values and leave the
 *  figure alone -- except that it is decided in the moment and per section,
 *  rather than baked into which kind of file you happened to save.
 *
 *  **It belongs to the device, not to a clip.** It is a stance you take while
 *  playing and drop again, so it is in neither the clip file nor the set; a
 *  lock that travelled with a clip would be a clip that refuses to be what it
 *  says it is on the next machine.
 *
 *  By section rather than by control. Three locks are three things a hand can
 *  find without reading, and the three questions they answer -- which figure,
 *  which room, which movement -- are the ones a set is actually built out of.
 */
struct ClipLocks
{
  /** The figure and how fast it is played. */
  bool shape = false;
  /** Where in the room it goes: the band, the base, and the sway that
   *  travels it. */
  bool elevation = false;
  /** What the movement is in the plane, and everything that moves it. */
  bool motion = false;

  bool any () const { return shape || elevation || motion; }
};

/** `incoming`, with every field of a held section taken from `current`.
 *
 *  One function because everything that writes over a slot has to ask the
 *  same question, and because *which field belongs to which section* is a
 *  list that drifts the moment there are two of it. A test walks the shared
 *  field list and insists every field is claimed exactly once -- by one of
 *  the three, or deliberately by none.
 *
 *  The nine envelope values and the act mode are claimed by none: they belong
 *  to the ACTION page, which is not one of the three sections, and a fired
 *  action already leaves them to the slot (see actionOver()).
 */
ClipSettings heldOver (ClipSettings const &current, ClipSettings incoming,
                       ClipLocks const &locks);

}
