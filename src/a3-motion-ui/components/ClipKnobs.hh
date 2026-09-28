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

#include <optional>

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

/** The Elevation section: clip-bottom (0), clip-top (1), sway (2), elv (3). */
constexpr ClipKnobSpec
elevationKnobSpec (int sub)
{
  if (sub == 2)
    return { -lfoMaxStep, lfoMaxStep, 1.0, 0.0, true, false, caption::sway };

  // Where the middle of the trajectory sits, bottom to top. Two taps are the
  // page's rule (the middle of the clip band), not a number in the table.
  if (sub == 3)
    return { 0.0, 1.0, 0.0, 0.5, false, false, caption::elevation };

  // The two clips are shares of the height, and zero is "do not clip" for
  // both -- which is where two taps put them. Bottom first, left to right:
  // the graphic above them is a room seen from the side, and there the floor
  // is not on the right of the ceiling.
  return { 0.0,   1.0,  0.0,
           0.0,   false, false,
           sub == 0 ? caption::clipBottom : caption::clipTop };
}


/** How many knobs the Motion section has -- motionKnobSpec (0 .. n-1). */
constexpr int numMotionKnobs = 14;

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
    // The figure's plane leant in the room, each lean beside its sweep. A
    // ring like rot, since the sweeps turn the plane round rather than
    // rocking it (2026-09-28): -2..2 quarter turns is the whole turn, and
    // upright stands at the top (knobAngleFraction).
    case 10: return { -2.0, 2.0, 0.0, 0.0, true, true, caption::tilt };
    case 11: return sweep (caption::tiltSweep);
    case 12: return { -2.0, 2.0, 0.0, 0.0, true, true, caption::roll };
    case 13: return sweep (caption::rollSweep);
    default: return {};
    }
}

/** Where a value stands on its knob, in the angle fraction PotKnob draws in:
 *  -1 to 1 across a scale, 0 at the top; round a ring, 0 to 2 for a whole
 *  turn measured from the value zero, so zero is at the top of every ring --
 *  rot's 0 and an upright lean alike (2026-09-28). A ring that ran from its
 *  minimum would put a -2..2 lean's upright at the bottom.
 *
 *  The one mapping the pointer (LookAndFeel_A3::drawRotarySlider) and the
 *  modulation's arc (reachOnKnob) are both drawn with, so the arc starts
 *  exactly at the pointer. */
constexpr float
knobAngleFraction (double min, double max, bool wraps, double value)
{
  if (!(max > min))
    return 0.f;

  if (wraps)
    return static_cast<float> (value / (max - min) * 2.0);

  return static_cast<float> ((value - min) / (max - min) * 2.0 - 1.0);
}

constexpr float
knobAngleFraction (ClipKnobSpec const &spec, double value)
{
  return knobAngleFraction (spec.min, spec.max, spec.wraps, value);
}

/** Where a modulation is holding a knob's value right now, in the angle
 *  fraction PotKnob draws in (knobAngleFraction), or -2 when nothing is
 *  moving it.
 *
 *  When the knobs became sliders on 2026-09-23 the modulation was left
 *  behind with the old painting code and the arcs vanished (a3-motion-ui#35);
 *  this is the one place that says how a held value becomes an arc. */
constexpr float
reachOnKnob (ClipKnobSpec const &spec, std::optional<float> held)
{
  if (!held.has_value () || !(spec.max > spec.min))
    return -2.f;

  return knobAngleFraction (spec, static_cast<double> (*held));
}

}
