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
withChannel (juce::String const &pattern, int channel)
{
  return pattern.replace (channelPlaceholder, juce::String (channel));
}

namespace
{
/** One entry. Absent, not a string, or one JUCE would refuse leaves the
 *  default standing — a bad address must cost a default, never a crash.
 *  Validity is judged on the substituted address: the template carries
 *  `{ch}`, which JUCE reads as a wildcard rather than a path. */
void
readAddress (juce::var const &block, char const *key, juce::String &into)
{
  if (!block.hasProperty (key))
    return;

  auto const value = block[key];
  if (!value.isString ())
    return;

  auto const address = value.toString ();
  if (!isSendableOscAddress (withChannel (address, 0)))
    return;

  into = address;
}

/** A prefix is matched with startsWith rather than sent, so it need not be
 *  a whole address — but an empty one would match every message there is. */
void
readPrefix (juce::var const &block, char const *key, juce::String &into)
{
  if (!block.hasProperty (key))
    return;

  auto const value = block[key];
  if (!value.isString ())
    return;

  auto const prefix = value.toString ();
  if (prefix.isEmpty () || !prefix.startsWithChar ('/'))
    return;

  into = prefix;
}
}

namespace
{
/** What each mixer address is called in config.json, in the same order as
 *  OscAddresses::mixerChannel/mixerMaster/mixerFilter.
 *
 *  Kept here rather than in the ui's MixerControls.hh: a key in a file
 *  somebody has already edited and the default behind it are one piece of
 *  vocabulary, and keeping them together is what lets this engine stay free
 *  of any `a3-motion-ui` include — reading a config file is the engine's
 *  job, not the ui's, and a table of seven words is not a reason to change
 *  that. Named per control, so a config file reads as words rather than as
 *  an index into a table: "mixerGain", not "mixer0". */
constexpr std::array<char const *, numMixerAddresses> mixerAddressKeys{
  "mixerGain", "mixerEqHigh", "mixerEqMid", "mixerEqLow",
  "mixerVolume", "mixerPfl", "mixerFx",
};

constexpr std::array<char const *, numMasterAddresses> masterAddressKeys{
  "masterVolume", "masterBooth", "masterPhonesMix",
  "masterPhonesVolume", "masterReturn",
};

constexpr std::array<char const *, numFilterAddresses> filterAddressKeys{
  "filterMode", "filterFrequency", "filterResonance",
};
}

namespace
{
/** Every key, against whichever block it is being read from. Called once
 *  for the flat block and once per group, so a file written before the
 *  grouping still reads and a grouped one wins over it. */
void
readAll (juce::var const &block, OscAddresses &into)
{
  if (!block.isObject ())
    return;

  readAddress (block, "channelAzimuth", into.channelAzimuth);
  readAddress (block, "channelElevation", into.channelElevation);
  readAddress (block, "channelPot1", into.channelPot1);
  readAddress (block, "channelPot2", into.channelPot2);
  readAddress (block, "channelThreeD", into.channelThreeD);
  // The name it shipped under for a day, while `3d` was still Core's toggle.
  readAddress (block, "channelPot3", into.channelThreeD);
  readAddress (block, "iemAzimuth", into.iemAzimuth);
  readAddress (block, "iemElevation", into.iemElevation);
  readAddress (block, "stateRecall", into.stateRecall);
  readAddress (block, "beat", into.beatOut);
  readAddress (block, "beatOut", into.beatOut);
  readAddress (block, "beatIn", into.beatIn);
  readAddress (block, "tap", into.tap);
  readAddress (block, "clockMode", into.clockMode);
  readPrefix (block, "vuPrefix", into.vuPrefix);
  readAddress (block, "energyRms", into.energyRms);

  // The mixer's keys, tables over the same three orders as the defaults --
  // see mixerAddressKeys/masterAddressKeys/filterAddressKeys above for why
  // they live here rather than in the ui's control table.
  for (std::size_t i = 0; i < numMixerAddresses; ++i)
    readAddress (block, mixerAddressKeys[i], into.mixerChannel[i]);

  for (std::size_t i = 0; i < numMasterAddresses; ++i)
    readAddress (block, masterAddressKeys[i], into.mixerMaster[i]);

  for (std::size_t i = 0; i < numFilterAddresses; ++i)
    readAddress (block, filterAddressKeys[i], into.mixerFilter[i]);
}
}

OscAddresses
loadOscAddresses (juce::var const &config)
{
  OscAddresses addresses;

  auto const block = config["oscAddresses"];
  if (!block.isObject ())
    return addresses;

  // The flat shape first: that is how the block shipped, and a config file
  // on a device does not rewrite itself. The groups then override it — the
  // Network page shows them under headings, and `beat` is in neither `out`
  // nor `in` because it is both, depending on the clock mode.
  readAll (block, addresses);
  readAll (block["beatclock"], addresses); // the shape before out/in carried it
  readAll (block["out"], addresses);
  readAll (block["in"], addresses);

  // `beat` under a group means that group's direction, so the same key can
  // sit in both and mean the right thing in each.
  readAddress (block["out"], "beat", addresses.beatOut);
  readAddress (block["in"], "beat", addresses.beatIn);

  return addresses;
}

}
