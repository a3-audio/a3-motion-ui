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

#include <a3-motion-engine/ActionMotion.hh>

#include <a3-motion-ui/components/ClipKnobs.hh>

namespace a3
{

/** What the ACTION page's MOTION tile is made of, without a window: which of
 *  its values are knobs and which are fields, each knob's scale (the MOTION
 *  page's own), and the arithmetic between where a control stands and the
 *  value it puts on the clip. */

/** Speed, direction and end: fields as on CLIP -- a drag steps the speed, a
 *  tap brings direction and end round. Everything else is a knob. */
bool motionParamIsAField (MotionParam param);

/** A knob's scale and caption, from the MOTION page's own table
 *  (motionKnobSpec, elevationKnobSpec). Empty for a field. */
ClipKnobSpec motionTileKnobSpec (MotionParam param);

/** What the tile writes under it -- the MOTION page's caption for a knob,
 *  CLIP's for a field. */
char const *motionParamCaption (MotionParam param);

/** Where the knob stands for a value as the settings hold it: the value
 *  itself, except elv, which is the base the other way up. */
float motionKnobValue (MotionParam param, float value);

/** The value a knob standing at `knob` puts on the clip: whole steps for a
 *  sweep, and elv held inside the clip band (`clipTop`, `clipBottom`) and
 *  snapped as the MOTION page's is. */
float motionValueForKnob (MotionParam param, double knob, float clipTop,
                          float clipBottom);

/** A field one step on: the speed held at its ends, direction and end round
 *  their lists. A knob comes back as it was. */
float steppedMotionValue (MotionParam param, float value, int increment);

}
