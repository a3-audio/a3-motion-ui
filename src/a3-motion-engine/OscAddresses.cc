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


#include "OscAddresses.hh"

namespace a3
{

namespace
{
// JUCE's own set for OSCAddressPattern; '/' is the separator and so is
// checked per token rather than listed here.
constexpr char const *disallowedInToken = " #";

bool
isPrintableAscii (juce::juce_wchar c)
{
  return c >= 32 && c < 127;
}

/** The placeholder a channel address carries. Substituted before anything
 *  is validated or sent, so it never reaches JUCE. */
constexpr char const *channelPlaceholder = "{ch}";

/** The truth's keys for each mixer table, in the same order as
 *  OscAddresses::mixerChannel/mixerMaster/mixerFilter -- that is, the ui's
 *  mixerControlOrder/masterControlOrder/filterControlOrder. */
constexpr std::array<char const *, numMixerAddresses> mixerChannelKeys{
  "channel.gain",   "channel.eq.high", "channel.eq.mid", "channel.eq.low",
  "channel.volume", "channel.aux-send", "channel.cue",    "channel.filter",
};

constexpr std::array<char const *, numMasterAddresses> mixerMasterKeys{
  "master.volume",        "master.booth",     "master.phones-mix",
  "master.phones-volume", "master.aux-return",
};

constexpr std::array<char const *, numFilterAddresses> mixerFilterKeys{
  "filter.mode",
  "filter.frequency",
  "filter.resonance",
};

/** The rest, one address each. */
constexpr std::array<char const *, 10> singleKeys{
  "channel.azimuth",
  "channel.elevation",
  "channel.filter.frequency",
  "channel.filter.q",
  "channel.3d",
  "state.recall",
  "beat",
  "tap",
  "clockmode",
  "vu",
};
}

bool
isSendableOscAddress (juce::String const &address)
{
  if (address.isEmpty () || !address.startsWithChar ('/'))
    return false;

  juce::StringArray tokens;
  tokens.addTokens (address, "/", juce::StringRef ());
  tokens.removeEmptyStrings (false);

  for (auto const &token : tokens)
    for (auto charPtr = token.getCharPointer (); !charPtr.isEmpty ();)
      {
        auto const c = charPtr.getAndAdvance ();
        if (!isPrintableAscii (c)
            || juce::String (disallowedInToken).containsChar (c))
          return false;
      }

  return true;
}

juce::String
withChannelIndex (juce::String const &pattern, int index)
{
  return pattern.replace (channelPlaceholder, juce::String (index + 1));
}

juce::StringArray
oscAddressKeys ()
{
  juce::StringArray keys;
  for (auto const *key : singleKeys)
    keys.add (key);
  for (auto const *key : mixerChannelKeys)
    keys.add (key);
  for (auto const *key : mixerMasterKeys)
    keys.add (key);
  for (auto const *key : mixerFilterKeys)
    keys.add (key);
  return keys;
}

namespace
{
/** The truth's pattern for `key`, or the mark that names it missing. */
juce::String
addressFor (OscTruth const &truth, juce::String const &key)
{
  auto const pattern = truth.pattern (key);
  return pattern.isNotEmpty () ? pattern : "/a3-osc-missing/" + key;
}

template <std::size_t N>
void
fillTable (OscTruth const &truth, std::array<char const *, N> const &keys,
           std::array<juce::String, N> &into)
{
  for (std::size_t i = 0; i < N; ++i)
    into[i] = addressFor (truth, keys[i]);
}

/** "/vu/{n}" -> "/vu/": the handler matches what comes before the number. */
juce::String
prefixBeforeNumber (juce::String const &pattern)
{
  auto const placeholder = pattern.indexOf ("{n}");
  return placeholder < 0 ? pattern : pattern.substring (0, placeholder);
}
}

OscAddresses
oscAddressesFrom (OscTruth const &truth)
{
  OscAddresses a;
  a.channelAzimuth = addressFor (truth, "channel.azimuth");
  a.channelElevation = addressFor (truth, "channel.elevation");
  a.channelFilterFrequency = addressFor (truth, "channel.filter.frequency");
  a.channelFilterQ = addressFor (truth, "channel.filter.q");
  a.channelThreeD = addressFor (truth, "channel.3d");
  a.stateRecall = addressFor (truth, "state.recall");
  a.beatOut = addressFor (truth, "beat");
  a.beatIn = a.beatOut;
  a.tap = addressFor (truth, "tap");
  a.clockMode = addressFor (truth, "clockmode");
  a.vuPrefix = prefixBeforeNumber (addressFor (truth, "vu"));

  fillTable (truth, mixerChannelKeys, a.mixerChannel);
  fillTable (truth, mixerMasterKeys, a.mixerMaster);
  fillTable (truth, mixerFilterKeys, a.mixerFilter);
  return a;
}

juce::StringArray
missingOscKeys (OscTruth const &truth)
{
  juce::StringArray missing;
  for (auto const &key : oscAddressKeys ())
    if (truth.pattern (key).isEmpty ())
      missing.add (key);
  return missing;
}

}
