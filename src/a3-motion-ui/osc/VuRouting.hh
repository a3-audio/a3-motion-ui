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

#include <array>

#include <JuceHeader.h>

#include <a3-motion-engine/OscTruth.hh>

namespace a3
{

/** How many meters the mixer's master column shows: the main sub and the
 *  nine main tops (decided 2026-09-30). */
constexpr int numMasterColumnMeters = 10;

/** Which /vu number feeds which part of Motion.
 *
 *  Motion names the meters it shows by what they measure -- "in1_pre",
 *  "main_sub", "main_top1" -- and asks the one truth for their numbers, the
 *  channel map's `vu_meters`. The numbers are the map's business: they moved
 *  from 0..39 to 1..40 once, and REAPER's routing may move them again.
 *
 *  A meter the truth does not have is number 0, which no message carries. */
struct VuRouting
{
  /** The four input dots around the blobs: pre-fader, post-FX. */
  std::array<int, 4> channelInputs{};
  /** The sphere's glow. */
  int glow{ 0 };
  /** The four towers' lights. */
  std::array<int, 4> towers{};
  /** The master column, bottom to top of the list: sub, then tops 1..9. */
  std::array<int, numMasterColumnMeters> masterColumn{};
};

/** Every meter name VuRouting asks the truth for. */
juce::StringArray vuMeterNames ();

VuRouting vuRoutingFrom (OscTruth const &truth);

}
