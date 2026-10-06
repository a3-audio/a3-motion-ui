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

#include <initializer_list>
#include <tuple>

#include <a3-motion-engine/OscAddresses.hh>
#include <a3-motion-engine/OscTruth.hh>

namespace a3
{

/** A truth whose every pattern is made up -- "/t/channel.volume/{ch}" -- so
 *  an address can only have come from the truth, never from a default left
 *  behind in the code. Tests of how Motion *uses* its addresses build their
 *  messages from it rather than from the real vocabulary: the vocabulary is
 *  a3-core's, and OscTruthContract holds Motion against the real file. */
inline OscTruth
madeUpOscTruth (juce::StringArray const &leaveOut = {})
{
  auto *addresses = new juce::DynamicObject ();
  for (auto const &key : oscAddressKeys ())
    if (!leaveOut.contains (key))
      {
        auto *entry = new juce::DynamicObject ();
        auto const perChannel = key.startsWith ("channel.");
        auto const pattern = key == "vu" ? juce::String ("/t/vu/{n}")
                                         : "/t/" + key
                                               + (perChannel ? "/{ch}" : "");
        entry->setProperty ("pattern", pattern);
        addresses->setProperty (juce::Identifier (key), juce::var (entry));
      }

  auto *hosts = new juce::DynamicObject ();
  hosts->setProperty ("local", "127.0.0.9");
  hosts->setProperty ("any", "0.0.0.0");
  hosts->setProperty ("core", "10.9.9.10");

  // Ports nobody uses, and a Core on a host of its own, so an endpoint can
  // only have come from here.
  juce::Array<juce::var> listeners;
  for (auto const &[program, role, host, port] :
       std::initializer_list<std::tuple<char const *, char const *,
                                        char const *, int> >{
           { "core", "osc", "core", 19000 },
           { "beat-analyzer", "clock", "any", 17775 },
           { "motion", "osc", "any", 17771 },
           { "motion", "vu", "any", 17772 },
           { "motion", "energy", "any", 17777 } })
    {
      auto *listener = new juce::DynamicObject ();
      listener->setProperty ("program", program);
      listener->setProperty ("role", role);
      listener->setProperty ("host", host);
      listener->setProperty ("port", port);
      listeners.add (juce::var (listener));
    }

  auto *root = new juce::DynamicObject ();
  root->setProperty ("addresses", juce::var (addresses));
  root->setProperty ("hosts", juce::var (hosts));
  root->setProperty ("listeners", listeners);

  // The meters Motion shows, out of the channel map's order and among ones it
  // does not, so a meter found in the right place was found by its name. The
  // mono in*_pre stay in the map, as in the real one until every consumer
  // reads the stereo pairs appended after them (16..25).
  juce::Array<juce::var> meters;
  for (auto const *name :
       { "free",      "in3_pre",   "main_top1", "in1_pre",   "main_sub",
         "in2_pre",   "main_top2", "main_top3", "main_top4", "in4_pre",
         "main_top5", "main_top6", "main_top7", "main_top8", "main_top9",
         "in2_pre_R", "in1_post_L", "in1_pre_L", "in4_pre_R", "in1_pre_R",
         "in3_pre_L", "in2_pre_L", "in3_pre_R", "in4_pre_L", "in1_post_R" })
    meters.add (name);
  root->setProperty ("vu_meters", meters);
  return parseOscTruth (juce::JSON::toString (juce::var (root)));
}

}
