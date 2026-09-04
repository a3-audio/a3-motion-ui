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

#include <juce_core/juce_core.h>

namespace a3
{

/** Which way a clip sets off. Only that: what happens when it gets to the end
 *  is the end action's business, and Bounce turns this round as it goes. There
 *  used to be a third value, "Ping", which meant the same thing as the Bounce
 *  end action -- two controls for one behaviour. */
enum class PlayDirection
{
  Forward,
  Reverse
};

/** What a clip does when it reaches the end of its pass. */
/** What holding the Action key does.
 *
 *  The accent was always a shot: press it and the envelope runs its attack,
 *  holds while the finger is down and decays when it lifts. Hold makes the
 *  clip itself follow the finger too -- the trajectory runs only while the key
 *  is down and stops the instant it is released, which is a stab rather than a
 *  cue. One-shot is the behaviour that was there before. */
enum class ActMode
{
  /** Fire it and let it run to whatever the end action says. */
  OneShot,
  /** Runs while held, stops the moment it is let go. */
  Hold,
};

enum class EndAction
{
  /** Round again, the way it was going. */
  Loop,
  /** End the pass and go back to where the take begins, so the next start is
   *  visibly a start. What a stop does on every deck. */
  Stop,
  /** Stand still where it got to, and start again from there. The caller
   *  takes the clip out of playback, so the channel keeps the last position
   *  it was given.
   *
   *  This is what `Stop` used to do, under the wrong name: standing still
   *  where you happen to land is a pause, and calling it a stop meant there
   *  was no way to ask for the other one. */
  Pause,
  /** Turn round and travel back. Never crosses the loop point, so a take's
   *  closing move is never played in this mode. */
  Bounce,
  /** Carry on somewhere else in the pass. On a drawn trajectory that is the
   *  same figure entered at a different point; on a tapped one the new phase
   *  lands inside a held tap, which makes it a random tap. */
  Random
};

/** Where a clip's playhead stands, and which way it is travelling.
 *
 *  `position` runs 0 to 1 across one pass. `sign` is +1 or -1 and only Bounce
 *  ever changes it. `stopped` says the pass is over and nothing should advance
 *  again until the clip is started afresh. */
struct Playhead
{
  float position;
  float sign;
  bool stopped;
};

float initialSign (PlayDirection direction);

/** The name a file stores, and the action it names. An unknown name is a file
 *  written before the setting existed, or edited by hand: it loops, which is
 *  what every clip did before there was a choice. */
juce::String endActionToName (EndAction action);
EndAction endActionFromName (juce::String const &name);

/** The playhead one tick on.
 *
 *  `delta` is the share of a pass covered in one tick, always positive; the
 *  direction lives in `sign`. `randomPhase` is drawn by the caller rather than
 *  in here, so that this stays a function whose behaviour can be checked. It
 *  is only read when the pass actually ends under EndAction::Random. */
Playhead advancePlayhead (Playhead current, float delta, EndAction endAction,
                          float randomPhase);

}
