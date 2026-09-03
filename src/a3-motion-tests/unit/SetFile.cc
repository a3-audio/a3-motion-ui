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


#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/SetFile.hh>

using namespace a3;

namespace
{
constexpr int numChannels = 4;
constexpr int numSlots = 2;

juce::File
tempSet (juce::String const &name)
{
  return juce::File::getSpecialLocation (juce::File::tempDirectory)
      .getChildFile (name);
}

SetFile
aSet ()
{
  SetFile set;
  set.channels.resize (numChannels);

  for (int ch = 0; ch < numChannels; ++ch)
    {
      auto &channel = set.channels[static_cast<size_t> (ch)];
      channel.threeD = 0.1f * static_cast<float> (ch + 1);
      channel.freq = 0.2f * static_cast<float> (ch + 1);
      channel.q = 0.05f * static_cast<float> (ch + 1);
      channel.slots.resize (numSlots);

      for (int slot = 0; slot < numSlots; ++slot)
        {
          auto &s = channel.slots[static_cast<size_t> (slot)];
          s.patternName
              = "Rec_" + std::to_string (ch) + std::to_string (slot) + ".svg";
          s.recordLengthLog2 = ch - slot;
        }
    }

  return set;
}
}

// A set is a thing you carry to a gig, so it has to come back the same. Every
// field, because the last time settings were half-written nobody noticed until
// a clip sounded different.
TEST (SetFileTest, ASetSurvivesARoundTrip)
{
  auto const file = tempSet ("a3-set-roundtrip.json");
  auto const written = aSet ();

  ASSERT_TRUE (saveSet (file, written));
  auto const read = loadSet (file, numChannels, numSlots);

  ASSERT_EQ (read.channels.size (), written.channels.size ());
  for (size_t ch = 0; ch < read.channels.size (); ++ch)
    {
      auto const &a = written.channels[ch];
      auto const &b = read.channels[ch];

      EXPECT_FLOAT_EQ (b.threeD, a.threeD) << "channel " << ch;
      EXPECT_FLOAT_EQ (b.freq, a.freq) << "channel " << ch;
      EXPECT_FLOAT_EQ (b.q, a.q) << "channel " << ch;

      ASSERT_EQ (b.slots.size (), a.slots.size ());
      for (size_t slot = 0; slot < b.slots.size (); ++slot)
        {
          EXPECT_EQ (b.slots[slot].patternName, a.slots[slot].patternName);
          EXPECT_EQ (b.slots[slot].recordLengthLog2,
                     a.slots[slot].recordLengthLog2);
        }
    }

  file.deleteFile ();
}

// No set is not an error. A device somebody has not brought a set to is a
// device with empty slots, and it has to start rather than refuse to.
TEST (SetFileTest, AMissingSetIsAnEmptySet)
{
  auto const file = tempSet ("a3-set-does-not-exist.json");
  file.deleteFile ();

  auto const read = loadSet (file, numChannels, numSlots);

  ASSERT_EQ (read.channels.size (), static_cast<size_t> (numChannels));
  for (auto const &channel : read.channels)
    {
      ASSERT_EQ (channel.slots.size (), static_cast<size_t> (numSlots));
      for (auto const &slot : channel.slots)
        EXPECT_TRUE (slot.patternName.empty ());
    }
}

// Same for a file that is there but is not a set. A stick with a stray
// set.json on it should not stop the device coming up.
TEST (SetFileTest, RubbishInTheFileIsAnEmptySetToo)
{
  auto const file = tempSet ("a3-set-rubbish.json");
  file.replaceWithText ("this is not json {{{");

  auto const read = loadSet (file, numChannels, numSlots);

  ASSERT_EQ (read.channels.size (), static_cast<size_t> (numChannels));
  for (auto const &channel : read.channels)
    EXPECT_EQ (channel.slots.size (), static_cast<size_t> (numSlots));

  file.deleteFile ();
}

// A set written by a device with fewer channels or slots than this one has
// must still load, filling what it does not mention. Hardware outlives file
// formats.
TEST (SetFileTest, ASmallerSetFillsOutToTheDevicesShape)
{
  auto const file = tempSet ("a3-set-smaller.json");

  SetFile small;
  small.channels.resize (2);
  for (auto &channel : small.channels)
    {
      channel.slots.resize (1);
      channel.slots[0].patternName = "Only.svg";
    }
  ASSERT_TRUE (saveSet (file, small));

  auto const read = loadSet (file, numChannels, numSlots);

  ASSERT_EQ (read.channels.size (), static_cast<size_t> (numChannels));
  for (auto const &channel : read.channels)
    ASSERT_EQ (channel.slots.size (), static_cast<size_t> (numSlots));

  EXPECT_EQ (read.channels[0].slots[0].patternName, "Only.svg");
  EXPECT_TRUE (read.channels[0].slots[1].patternName.empty ());
  EXPECT_TRUE (read.channels[3].slots[0].patternName.empty ());

  file.deleteFile ();
}
