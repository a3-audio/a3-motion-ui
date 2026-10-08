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

#include <a3-motion-engine/util/Types.hh>

namespace a3
{

/** 3x^2 - 2x^3 on x clamped to 0..1: starts and ends with zero slope, so a
 *  glide driven by it neither kicks off nor lands with a jolt. */
float smoothstep (float x);

/** The direction `weight` (0..1) of the way from `from` to `to`, the short
 *  way round: a normalised lerp of the two, always of unit length.
 *
 *  This is the ORBIT -> CLIP glide. Where there is no sensible blend, `to` is
 *  returned, because the clip is where the channel is going: when `from` is
 *  invalid or has no direction, or the two are (nearly) opposite so the blend
 *  is shorter than 1e-4. No allocation: safe on the clock thread. */
Pos handover (Pos from, Pos to, float weight);

}
