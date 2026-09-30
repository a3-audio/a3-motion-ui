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


#include "VuRouting.hh"

namespace a3
{

namespace
{
constexpr std::array<char const *, 4> inputNames{ "in1_pre", "in2_pre",
                                                  "in3_pre", "in4_pre" };
constexpr char const *glowName = "main_sub";
constexpr std::array<char const *, 4> towerNames{ "main_top1", "main_top2",
                                                  "main_top3", "main_top4" };
constexpr std::array<char const *, numMasterColumnMeters> masterNames{
  "main_sub",  "main_top1", "main_top2", "main_top3", "main_top4",
  "main_top5", "main_top6", "main_top7", "main_top8", "main_top9",
};

template <std::size_t N>
void
numberAll (OscTruth const &truth, std::array<char const *, N> const &names,
           std::array<int, N> &into)
{
  for (std::size_t i = 0; i < N; ++i)
    into[i] = truth.vuNumber (names[i]);
}
}

juce::StringArray
vuMeterNames ()
{
  juce::StringArray names;
  for (auto const *name : inputNames)
    names.addIfNotAlreadyThere (name);
  names.addIfNotAlreadyThere (glowName);
  for (auto const *name : towerNames)
    names.addIfNotAlreadyThere (name);
  for (auto const *name : masterNames)
    names.addIfNotAlreadyThere (name);
  return names;
}

VuRouting
vuRoutingFrom (OscTruth const &truth)
{
  VuRouting routing;
  numberAll (truth, inputNames, routing.channelInputs);
  routing.glow = truth.vuNumber (glowName);
  numberAll (truth, towerNames, routing.towers);
  numberAll (truth, masterNames, routing.masterColumn);
  return routing;
}

}
