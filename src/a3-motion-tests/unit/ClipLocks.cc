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

#include <a3-motion-engine/ClipLocks.hh>

#include "ClipSettingsFields.hh"

using namespace a3;

namespace
{
/** The three locks, one at a time, plus none of them. */
std::array<ClipLocks, 3> const
each ()
{
  ClipLocks shape;
  shape.shape = true;
  ClipLocks elevation;
  elevation.elevation = true;
  ClipLocks motion;
  motion.motion = true;

  return { shape, elevation, motion };
}
}

// Nothing held is nothing changed: the clip arrives whole.
TEST (ClipLocks, WithNothingHeldTheWholeClipLands)
{
  ClipSettings current;
  ClipSettings incoming;
  for (auto const &[name, mutate] : clipSettingsFields ())
    {
      juce::ignoreUnused (name);
      mutate (incoming);
    }

  EXPECT_EQ (heldOver (current, incoming, ClipLocks{}), incoming);
}

// Every field belongs to exactly one lock, or deliberately to none.
//
// This is the test the whole arrangement stands on. What a lock holds is a
// list, and a list like that goes wrong by a field being added to ClipSettings
// and nobody thinking about which column of the bar it sits in -- it then
// belongs to no lock, is written over while a section is held, and there is
// nothing to notice it. Walked off the shared list for that reason.
TEST (ClipLocks, EveryFieldIsClaimedByAtMostOneLock)
{
  // The ACTION page's, deliberately claimed by none: it is not one of the
  // three sections, and a fired action already leaves these to the slot.
  std::set<std::string> const theActionPages{
    "envelopeAttack", "envelopeDecay", "envelopeMax", "freqAttack",
    "freqDecay",      "freqMax",       "qAttack",     "qDecay",
    "qMax",           "actMode",
  };

  for (auto const &[name, mutate] : clipSettingsFields ())
    {
      ClipSettings current;
      ClipSettings incoming;
      mutate (incoming);
      ASSERT_NE (current, incoming) << name << " never actually differs";

      auto held = 0;
      for (auto const &locks : each ())
        if (heldOver (current, incoming, locks) == current)
          ++held;

      if (theActionPages.count (name) > 0)
        EXPECT_EQ (held, 0)
            << name << " belongs to the ACTION page and to no lock";
      else
        EXPECT_EQ (held, 1)
            << name << " is held by " << held
            << " of the three locks; every field of the bar belongs to one";
    }
}

// A held section keeps what it has and lets the rest of the clip through. That
// is the whole gesture: step through clips with the elevation held and every
// figure arrives in the room you are already in.
TEST (ClipLocks, AHeldSectionKeepsItsOwnAndLetsTheRestThrough)
{
  ClipSettings current;
  current.reach = 0.9f;
  current.clipTop = 0.3f;
  current.spin = 5;

  ClipSettings incoming;
  incoming.reach = 0.2f;
  incoming.clipTop = 0.0f;
  incoming.spin = -2;

  ClipLocks locks;
  locks.elevation = true;

  auto const landed = heldOver (current, incoming, locks);

  EXPECT_FLOAT_EQ (landed.clipTop, current.clipTop) << "elevation was held";
  EXPECT_EQ (landed.spin, incoming.spin) << "motion was not";

  // reach is drawn in Motion, beside the swell that sweeps it, so it is
  // Motion's to hold -- a lock holds the column it stands over.
  EXPECT_FLOAT_EQ (landed.reach, incoming.reach);
  locks.motion = true;
  EXPECT_FLOAT_EQ (heldOver (current, incoming, locks).reach, current.reach);
}

// All three held is a clip that changes nothing at all -- except the figure,
// which is not a setting and is held by the shape lock at the call site.
TEST (ClipLocks, EverythingHeldLetsNothingOfTheBarThrough)
{
  ClipSettings current;
  ClipSettings incoming;
  for (auto const &[name, mutate] : clipSettingsFields ())
    {
      juce::ignoreUnused (name);
      mutate (incoming);
    }

  ClipLocks all;
  all.shape = all.elevation = all.motion = true;

  auto const landed = heldOver (current, incoming, all);

  // The ACTION page's values still land: they are nobody's lock.
  EXPECT_EQ (landed.envelopeAttack, incoming.envelopeAttack);
  EXPECT_EQ (landed.actMode, incoming.actMode);

  // Everything the bar shows is what it was.
  EXPECT_EQ (landed.speedLog2, current.speedLog2);
  EXPECT_FLOAT_EQ (landed.reach, current.reach);
  EXPECT_FLOAT_EQ (landed.elevationBase, current.elevationBase);
  EXPECT_EQ (landed.endAction, current.endAction);
}

TEST (ClipLocks, AnyIsTrueWhenSomethingIsHeld)
{
  EXPECT_FALSE (ClipLocks{}.any ());
  for (auto const &locks : each ())
    EXPECT_TRUE (locks.any ());
}
