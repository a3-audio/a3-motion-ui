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

/** Where one channel's playhead is drawn across the tick indicator.
 *
 *  A mark per channel rather than one fill for all of them: up to four clips
 *  run at once (one per channel, `channel._patternPlaying` is singular), and a
 *  single fill could only ever show one of them — it showed whichever clip the
 *  settings bar happened to be displaying, which is not the question anybody
 *  is asking mid-set.
 *
 *  `fraction` is how far through its own loop that channel's clip has got.
 *  Negative means the channel is not playing and nothing is drawn: the status
 *  bar is handed one entry per channel whether or not it is running, so "not
 *  playing" has to be a value rather than an absence.
 *
 *  Past the end is clamped, not dropped. The engine runs on its own thread and
 *  can be a tick ahead of the repaint; a playhead that vanished for one frame
 *  at the loop point would read as a clip that had stopped. */
juce::Rectangle<float> playheadBounds (juce::Rectangle<float> tick,
                                       float fraction, float width);

/** The position as the indicator shows it: always sweeping left to right.
 *
 *  The engine plays a clip backwards by counting its position down — `dir`
 *  sets a sign of -1, and a bounce flips it at the ends. Followed literally
 *  the mark would travel right to left, and a bounce would turn it round
 *  mid-run.
 *
 *  Mirroring the position while the sign is negative keeps every pass going
 *  the same way; a bounce then leaves the right edge and comes back in at the
 *  left, which is the picture a loop already makes. What that gives up is
 *  telling forward from backward — the indicator answers how far, not which
 *  way, and a mark that sometimes runs backwards has to be read rather than
 *  caught out of the corner of an eye.
 *
 *  A negative position is the not-playing sentinel and passes through
 *  untouched, or a stopped channel would come out at the right-hand edge. */
float leftToRightPosition (float position, float sign);

}
