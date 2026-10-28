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

#pragma once

#include <cstddef>
#include <mutex>
#include <vector>

#include <juce_graphics/juce_graphics.h>

namespace a3
{

/** The colour every panel LED should show, and which of them changed.
 *
 *  LEDs are state, not events. They used to travel to the adapter's thread
 *  as messages in a FIFO of 31, and the panel is 41 LEDs: painting it at
 *  start dropped whatever did not fit, a different handful every time, and
 *  nothing ever sent them again. A table cannot overflow -- a slot written
 *  twice before it is taken simply goes out once, as its last colour.
 *
 *  `set()` is called from the message thread, `takeChanged()` from the
 *  adapter's thread; the lock is held only to copy.
 */
class LedState
{
public:
  struct Change
  {
    std::size_t slot;
    juce::Colour colour;
  };

  explicit LedState (std::size_t slotCount);

  /** Ask for this colour. A slot past the end is ignored. */
  void set (std::size_t slot, juce::Colour colour);

  /** Every slot set since the last call, in slot order, with its latest
   *  colour -- and forget them. */
  std::vector<Change> takeChanged ();

  /** Send this slot again with its latest colour: what reached the panel is
   *  not known any more. A slot past the end is ignored. */
  void resend (std::size_t slot);

private:
  std::mutex _lock;
  std::vector<juce::Colour> _colours;
  std::vector<bool> _changed;
};

}
