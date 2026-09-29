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

/** How hard a channel just hit: 0..1, high on a beat, back to 0 between.
 *
 *  The blob's spikes used to fire whenever the level stood above 0.28, at
 *  random one moment in three -- so a loud channel threw them all the time
 *  and none of them landed on the beat (measured 2026-09-29: the loudest
 *  channel's peak sat above the blob's vuMax 48 % of the time). A hit is a
 *  change, not a level: the peak jumping above where the channel has been.
 *
 *  So the level is followed in dB by a slow envelope, and the punch is how
 *  far the peak stands above it -- nothing for a steady tone however loud,
 *  most for a kick after a quiet bar. It falls away fast, so it reads as a
 *  strike rather than a glow. */
class BlobPunch
{
public:
  /** Takes the channel's linear peak (0..1) of the frame that just passed. */
  float update (float peak, float dtSeconds);
  float value () const { return _punch; }

private:
  float _runningDb = -60.f;
  float _punch = 0.f;
};

}
