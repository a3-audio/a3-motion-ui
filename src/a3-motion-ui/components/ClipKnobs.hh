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

#include <a3-motion-engine/TempoLfo.hh>

#include <a3-motion-ui/components/ClipSettingsCaptions.hh>

namespace a3
{

/** What one of the clip bar's knobs is: the scale it is dragged on, where two
 *  taps put it back, and how its arc reads.
 *
 *  A table of its own rather than the arithmetic scattered through the page's
 *  painting, so the scale a finger drags on and the value the engine is told
 *  are one answer -- and so it can be checked without building the bar, which
 *  drags half the app in with it.
 *
 *  Only the knobs are here. What is tapped rather than turned -- the clip
 *  field, dir, end -- stays a field: a list that opens a menu is a second
 *  gesture where the bar needs one.
 */
struct ClipKnobSpec
{
  double min = 0.0;
  double max = 1.0;
  /** 0 for a continuous value, 1 for whole steps. */
  double interval = 0.0;
  double resetTo = 0.0;
  /** The arc grows from the middle rather than from the start. */
  bool bipolar = false;
  /** A closed ring, for a value that comes round to itself. */
  bool wraps = false;
  /** What is written under it. Here rather than in the painting, so what a
   *  knob *is* stands in one place and the page only wires it up. */
  char const *label = "";
};

/** Which of the bar's sections is which, in the order they stand. */
constexpr int elevationSection = 1;
constexpr int motionSection = 2;

/** The Elevation section: clip-bottom (0), clip-top (1), sway (2). */
constexpr ClipKnobSpec
elevationKnobSpec (int sub)
{
  if (sub == 2)
    return { -lfoMaxStep, lfoMaxStep, 1.0, 0.0, true, false, caption::sway };

  // The two clips are shares of the height, and zero is "do not clip" for
  // both -- which is where two taps put them. Bottom first, left to right:
  // the graphic above them is a room seen from the side, and there the floor
  // is not on the right of the ceiling.
  return { 0.0,   1.0,  0.0,
           0.0,   false, false,
           sub == 0 ? caption::clipBottom : caption::clipTop };
}


/** The Motion section, in reading order: a standing value beside the sweep
 *  that works on it -- rot with its spin, reach with its swell, each squeeze
 *  with its stretch, the fade with the bias.
 *
 *  The sweeps count whole LFO steps either way; the values they work on are
 *  continuous. Two taps are not in this table: what a control goes back to
 *  is the page's own rule (onControlReset), and for reach it depends on where
 *  the figure sits.
 */
constexpr ClipKnobSpec
motionKnobSpec (int sub)
{
  auto const sweep = [] (char const *label) {
    return ClipKnobSpec{ -lfoMaxStep, lfoMaxStep, 1.0, 0.0, true, false, label };
  };

  switch (sub)
    {
    // A turn has no ends, so its scale has none: the ring comes round to
    // itself the way the engine's own value does (Pattern::setRotate wraps).
    case 0: return { 0.0, 1.0, 0.0, 0.0, false, true, caption::rotate };
    case 1: return sweep (caption::spin);
    case 2: return { -1.0, 1.0, 0.0, 0.0, true, false, caption::reach };
    case 3: return sweep (caption::swell);
    case 4: return { -1.0, 1.0, 0.0, 0.0, true, false, caption::squeezeX };
    case 5: return sweep (caption::stretchX);
    case 6: return { -1.0, 1.0, 0.0, 0.0, true, false, caption::squeezeY };
    case 7: return sweep (caption::stretchY);
    case 8: return { 0.0, 1.0, 0.0, 0.0, false, false, caption::fade };
    // Which way a bridge leans, in whole notches either side of the middle.
    case 9: return { -4.0, 4.0, 1.0, 0.0, true, false, caption::bias };
    default: return {};
    }
}

}
