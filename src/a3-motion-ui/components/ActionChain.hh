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

#include <JuceHeader.h>

#include <array>
#include <optional>

namespace a3
{

/** Each button's "then" (2026-09-28): the button of the same channel that
 *  fires when this one's accent is over, or nothing. Indexed by button. */
using AfterTable = std::array<std::optional<int>, numActionButtons>;

/** One channel's chain of actions, decided on the message thread.
 *
 *  The engine only counts accents that have ended (accentEndCount); what
 *  fires next is worked out here, where the scripts are, from which button
 *  fired the running accent and what that button says comes after it. A
 *  count rather than an event so a chain can never re-enter itself within
 *  one end: each end is answered once. */
class ActionChain
{
public:
  /** `button` fired, by a hand or by the chain. `endCount` is the engine's
   *  count at that moment: an end already counted is not this press's. */
  void pressed (int button, unsigned endCount);

  /** Play|Pause or Stop on the channel: whatever was chained stops here. */
  void broken ();

  /** The engine's count now. Returns the button to fire when an accent has
   *  ended since last asked and the button that ran it has one after it. */
  std::optional<int> accentEnded (unsigned endCount, AfterTable const &after);

private:
  int _running = -1;
  unsigned _seen = 0;
};

/** The after key one step on: --, A1 .. A6 and round. */
std::optional<int> stepAfter (std::optional<int> after, int increment);

/** "A3", or "--" for nothing -- the key's word and the set's. */
juce::String afterName (std::optional<int> after);
std::optional<int> afterFromName (juce::String const &name);

/** Where a press on an action pad came from. */
enum class PadSource
{
  Panel,
  PadsPage,
  /** The global strip's ACT key, which fires the chosen button anyway. */
  TransportKey,
  Scene,
  Chain,
};

/** Whether a press brings up the ACTION page with the pressed button chosen:
 *  a push on the panel or the PADS page does, unless Shift makes it a
 *  preview -- auditioned from wherever the hand is, FILES most of all, which
 *  a page change would take away from under it. */
bool actionPressShowsItsPage (PadSource source, bool shift);

}
