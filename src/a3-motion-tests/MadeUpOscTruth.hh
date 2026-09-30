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

  auto *root = new juce::DynamicObject ();
  root->setProperty ("addresses", juce::var (addresses));
  return parseOscTruth (juce::JSON::toString (juce::var (root)));
}

}
