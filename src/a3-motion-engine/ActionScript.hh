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

namespace a3
{

/** An action, written down.
 *
 *  One assignment per line, in SuperCollider's shape: a name with a tilde, an
 *  equals sign, an expression, a semicolon. Anybody who has typed
 *  `~freq = 400;` into a SuperCollider window already knows how to write one,
 *  and that is the whole reason for the borrowed syntax -- this is not
 *  SuperCollider and does not pretend to be, it just refuses to invent a
 *  third notation for something two already exist for.
 *
 *      // a hard throw, one way, and a resonance that arrives late
 *      ~spin = -6;
 *      ~reach = ~reach * 0.5;
 *      ~end = \bounce;
 *      ~qMax = rrand(0.4, 0.9);
 *      ~freqAttack = [0, 2, 4].choose;
 *
 *  What can be said: numbers, `~name` for what the clip has *now*, the four
 *  arithmetic operators with brackets, `rrand(lo, hi)`, `[a, b, c].choose`,
 *  `true`/`false`, and `\symbol` for the values that are words -- \loop
 *  \stop \pause \bounce \random, \forward \reverse, \oneshot \hold.
 *
 *  What cannot: anything that takes time. There is no sequencing here and no
 *  scheduler behind it; a script is evaluated once, all at once, and what it
 *  produces is a set of values. Movement over time is what the clip and its
 *  envelopes are already for.
 */

/** What came out, and what went wrong.
 *
 *  Both, rather than one or the other: a bad line takes itself down and
 *  leaves the rest standing. On a device whose keyboard appears over the
 *  screen there is no compiler to run first, so a script that refused to do
 *  anything because of one typo would be a script you could not get back to
 *  working.
 */
struct ActionScriptResult
{
  ClipSettings settings;
  /** One line each, naming the line number and what was wrong with it. */
  juce::StringArray errors;
};

/** Read a script and work out what it makes of the settings it is given.
 *
 *  @param seed  what the dice are set to. A roll happens when a script is
 *               applied, so the same seed applied twice lands in the same
 *               place -- which is what makes a random action something you
 *               can rehearse rather than only hope for.
 */
ActionScriptResult runActionScript (juce::String const &source,
                                    ClipSettings const &current,
                                    juce::int64 seed);

/** The other direction: settings written out as a script.
 *
 *  What "save action" produces, so that everything the app writes is
 *  something the app can read and a person can edit. Every field, in the
 *  order the ACTION page reads.
 */
juce::String actionScriptFor (ClipSettings const &settings);

/** Every name a script may assign to, for the editor and for error messages.
 *  Sorted, so a "did you mean" can be built off it later. */
juce::StringArray actionScriptNames ();

}
