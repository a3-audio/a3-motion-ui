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

#include <JuceHeader.h>

namespace a3
{

/** Wait until `ready()` answers true, or give up after `timeoutMs`.
 *
 *  For every place a test has to let the engine's own thread get a turn. It
 *  replaces `juce::Thread::sleep (400)` followed by an assertion, which is a
 *  *bet* that the tempo-clock thread had its turn inside 400 ms. On an idle
 *  machine it always does; with six compiler processes on the cores it does
 *  not have to, and `ActionFiring.cc` failed that bet seven times between
 *  2026-09-06 and 2026-09-13 -- three different test names, one file, always
 *  under load, always green again alone.
 *
 *  The timeout is generous on purpose. It is not a deadline the behaviour has
 *  to meet, only a bound on how long a broken build may hang: a test that
 *  waits two seconds on a loaded machine and passes has still proved what it
 *  set out to prove, where one that slept 400 ms and failed proved nothing at
 *  all.
 *
 *  **Only for waiting until something happens.** The other shape -- sleep,
 *  then assert that nothing *has* happened -- cannot be polled: there is no
 *  moment at which "still unchanged" becomes true, and waiting longer only
 *  makes it a stronger statement. Those sleeps stay.
 */
template <typename Ready>
bool
waitUntil (Ready ready, int timeoutMs = 4000, int stepMs = 5)
{
  auto const deadline = juce::Time::getMillisecondCounter ()
                        + static_cast<juce::uint32> (timeoutMs);

  // Asked once before any waiting: a condition that is already true must not
  // cost a step, or every call pays for the slowest machine it might run on.
  while (!ready ())
    {
      if (juce::Time::getMillisecondCounter () >= deadline)
        return ready ();

      juce::Thread::sleep (stepMs);
    }

  return true;
}

}
