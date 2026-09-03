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

#include "SetFile.hh"

namespace a3
{

namespace
{
/** Grown to the shape of the device that is reading it, whatever shape it was
 *  written in. A set from a four-channel device on a six-channel one is four
 *  channels and two empty, not a refusal — hardware outlives file formats. */
void
fitToDevice (SetFile &set, int numChannels, int numSlots)
{
  set.channels.resize (static_cast<size_t> (juce::jmax (0, numChannels)));
  for (auto &channel : set.channels)
    channel.slots.resize (static_cast<size_t> (juce::jmax (0, numSlots)));
}
}

SetFile
loadSet (juce::File const &file, int numChannels, int numSlots)
{
  SetFile set;

  // A missing or unreadable set is an empty one. A device somebody has not
  // brought a set to still has to come up, and a stray file on a stick must
  // not be the reason a gig does not start.
  auto const parsed = file.existsAsFile ()
                          ? juce::JSON::parse (file.loadFileAsString ())
                          : juce::var{};

  if (auto const *channels = parsed["channels"].getArray ())
    {
      for (auto const &entry : *channels)
        {
          SetFile::Channel channel;
          channel.threeD = static_cast<float> (entry["threeD"]);
          channel.freq = static_cast<float> (entry["freq"]);
          channel.q = static_cast<float> (entry["q"]);

          if (auto const *slots = entry["slots"].getArray ())
            for (auto const &slotEntry : *slots)
              {
                SetFile::Slot slot;
                slot.patternName
                    = slotEntry["pattern"].toString ().toStdString ();
                slot.recordLengthLog2
                    = static_cast<int> (slotEntry["recordLengthLog2"]);
                channel.slots.push_back (slot);
              }

          set.channels.push_back (channel);
        }
    }

  fitToDevice (set, numChannels, numSlots);

  return set;
}

bool
saveSet (juce::File const &file, SetFile const &set)
{
  juce::Array<juce::var> channels;

  for (auto const &channel : set.channels)
    {
      auto *entry = new juce::DynamicObject ();
      entry->setProperty ("threeD", channel.threeD);
      entry->setProperty ("freq", channel.freq);
      entry->setProperty ("q", channel.q);

      juce::Array<juce::var> slots;
      for (auto const &slot : channel.slots)
        {
          auto *slotEntry = new juce::DynamicObject ();
          slotEntry->setProperty ("pattern",
                                  juce::String (slot.patternName));
          slotEntry->setProperty ("recordLengthLog2", slot.recordLengthLog2);
          slots.add (juce::var (slotEntry));
        }
      entry->setProperty ("slots", slots);

      channels.add (juce::var (entry));
    }

  auto *root = new juce::DynamicObject ();
  root->setProperty ("channels", channels);

  if (!file.getParentDirectory ().createDirectory ())
    return false;

  return file.replaceWithText (juce::JSON::toString (juce::var (root)));
}

}
