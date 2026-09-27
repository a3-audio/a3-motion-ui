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

#include <a3-motion-engine/util/Types.hh>

namespace a3
{

/** How a clip travels its take. What it does when that travel is over is the
 *  end action's business (Loop, Stop, Pause).
 *
 *  Bounce and Random were end actions until 2026-09-26, which made them
 *  exclusive with stopping: a bounce could not be asked to go out and back
 *  once and stop. They are ways of travelling, so they are here now, and any
 *  of the four combines with any end. */
enum class PlayDirection
{
  Forward,
  Reverse,
  /** Out to the far end and back. Its travel is the whole round, so the end
   *  action -- and a press asking it to finish -- applies when it is home. */
  Bounce,
  /** Each lap from a random phase, to the end of the pass. On a drawn
   *  trajectory that is the same figure entered at a different point; on a
   *  tapped one the new phase lands inside a held tap. */
  Random
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
  /** Random dropped in at a new point: the blob glides there (Glide.hh). */
  bool jumped = false;
};

float initialSign (PlayDirection direction);

/** Where a clip sets off: the start, the end for Reverse, the drawn phase for
 *  Random. `randomPhase` is drawn by the caller so this stays checkable. */
float initialPosition (PlayDirection direction, float randomPhase);

/** Whether the step from the take's last tick to its first is ever
 *  travelled -- only by a clip running straight on, looping. It decides
 *  whether that step is a gap the fade joins. */
bool travelsTheWrap (PlayDirection direction, EndAction endAction);

/** A clip's direction and end, read from the names a file stores. Files
 *  written while Bounce and Random were end actions name them as the end;
 *  those become the direction, looping -- which is how they played. */
struct PlaybackMode
{
  PlayDirection direction = PlayDirection::Forward;
  EndAction endAction = EndAction::Loop;
};

PlaybackMode playbackModeFromNames (juce::String const &direction,
                                    juce::String const &endAction);

/** The name a file stores, and the action it names. An unknown name is a file
 *  written before the setting existed, or edited by hand: it loops, which is
 *  what every clip did before there was a choice. */
juce::String endActionToName (EndAction action);

/** The other two enums a clip carries, in the one place that spells them.
 *
 *  PatternFile wrote them as inline ternaries and ClipFile would have needed
 *  its own -- two tables for one enum, which agree until the day somebody adds
 *  a value to one of them. */
juce::String playDirectionToName (PlayDirection direction);
PlayDirection playDirectionFromName (juce::String const &name);
juce::String actModeToName (ActMode mode);
ActMode actModeFromName (juce::String const &name);
EndAction endActionFromName (juce::String const &name);

/** The playhead one tick on.
 *
 *  `delta` is the share of a pass covered in one tick, always positive; the
 *  way it is going lives in `sign`. `randomPhase` is drawn by the caller
 *  rather than in here, so that this stays a function whose behaviour can be
 *  checked. It is only read when a Random lap ends and loops.
 *
 *  The end action applies when the travel is over: at the end of the pass,
 *  and for a Bounce when it is home again -- the far end only turns it.
 *
 *  `stopAtEnd` is somebody having pressed play on a running clip: finish this
 *  travel and stop, whatever the end action says. It overrules Loop, which is
 *  what a person wants to get out of at a musical boundary rather than in the
 *  middle of a figure. Where it leaves the playhead is EndAction::Stop's
 *  answer: back at the take's start, because the press was made at a boundary
 *  and the next one should be a start.
 *
 *  It defaults to false, which is what every clip did before there was a way
 *  to ask. */
Playhead advancePlayhead (Playhead current, float delta,
                          PlayDirection direction, EndAction endAction,
                          float randomPhase, bool stopAtEnd = false);

/** The lap one tick on: time through the clip's length, counted in whole
 *  ticks and wrapped at the length, whatever the playhead does. Random starts
 *  every pass at a random point, so the position is not time; this is. A
 *  length of zero keeps it at the start. */
constexpr index_t
nextLapTick (index_t tick, index_t length)
{
  return length > 0 ? (tick + 1) % length : 0;
}

/** How far through its length a lap is, 0 to 1. */
constexpr float
lapProgress (index_t tick, index_t length)
{
  return length > 0 ? static_cast<float> (tick) / static_cast<float> (length)
                    : 0.f;
}

/** Where in the take a play position lands, as a fractional tick index.
 *
 *  Loop spans the full tick count: its last tick is joined to its first, that
 *  seam is part of the loop, and the fade exists to smooth it.
 *
 *  Bounce spans the ticks themselves -- 0 to numTicks-1 -- because it turns
 *  round at the take's own end. Sampled over the full count, the seam would
 *  sit inside the playable range and a bouncing clip would run a little way
 *  along the join back to the beginning before turning: on an open path, a
 *  dart across the sphere and back, always at the same spot. */
double fractionalTickForPlayback (float position, index_t numTicks,
                                  PlayDirection direction);

}
