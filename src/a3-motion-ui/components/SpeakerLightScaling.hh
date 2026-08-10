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

namespace a3
{

/** Brightness of one speaker beam, 0..1, from its VU rms.
 *
 *  Deliberately rms-only: the peak on this rig carries a crest factor around
 *  12, so a peak-driven beam chases transients and never holds still long
 *  enough to be compared against its neighbours. Peaks are the right input for
 *  motion, not for a steady brightness — see
 *  `.claude/notes/speaker-waveform-visualisation.md`.
 *
 *  `curve` is the perceptual exponent and is the only thing that sets how far
 *  apart two speakers read: scaling both by `vuMax` cancels out of the ratio
 *  ((a/m)^c / (b/m)^c == (a/b)^c), so `vuMax` shifts overall brightness while
 *  `curve` alone controls contrast between speakers. */
float speakerLightLevel (float vuRms, float vuMax, float curve);

/** Half-angle of a beam cone, in degrees, for a given width value.
 *
 *  The shader lights a pixel when `dot(dir, -speakerDir) > 1 - width`, so the
 *  cone reaches `acos(1 - width)` off-axis. With the speakers 90 degrees
 *  apart, a half-angle of 45 degrees is the point where neighbouring cones
 *  just touch. */
float beamHalfAngleDegrees (float width);

}
