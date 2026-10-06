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

/** One channel's meter in stereo: the /vu numbers of its two sides. */
struct StereoMeter
{
  int left{ 0 };
  int right{ 0 };
};

/** Which /vu number feeds which part of Motion.
 *
 *  Motion names the meters it shows by what they measure -- "in1_pre_L",
 *  "main_sub", "main_top1" -- and asks the one truth for their numbers, the
 *  channel map's `vu_meters`. The numbers are the map's business: they moved
 *  from 0..39 to 1..40 once, and REAPER's routing may move them again.
 *
 *  A meter the truth does not have is number 0, which no message carries. */
struct VuRouting
{
  /** Each channel's input meter, left and right: the corona round its blob
   *  and its meter on both mixer pages, which show the louder side. */
  std::array<StereoMeter, 4> channelInputs{};
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
