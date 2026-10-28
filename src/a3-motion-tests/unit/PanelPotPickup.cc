// SPDX-FileCopyrightText: 2026 Raphael Eismann
// SPDX-License-Identifier: GPL-3.0-or-later

// P44: after the panel comes back from StemDeck a pot does nothing until it
// crosses the value it would change, as StemDeck's AUX pots do (P23).

#include <gtest/gtest.h>

#include <a3-motion-ui/io/PanelPotPickup.hh>

using a3::PanelPotPickup;

namespace
{
// A pot that has been moved, applied, and reported back like the component does.
std::optional<float>
move (PanelPotPickup &p, int pot, float reading, float &stored)
{
  auto const out = p.feed (pot, reading, stored);
  if (out)
    {
      stored = *out;
      p.wrote (pot, stored);
    }
  return out;
}
}

TEST (PanelPotPickup, WithoutAHandOverEveryReadingActs)
{
  PanelPotPickup p;
  float stored = 0.2f;
  EXPECT_EQ (move (p, 0, 0.9f, stored), 0.9f);
  EXPECT_EQ (move (p, 0, 0.1f, stored), 0.1f);
  // a touch moving the value changes nothing for a pot that never handed over
  stored = 0.7f;
  EXPECT_EQ (move (p, 0, 0.12f, stored), 0.12f);
}

TEST (PanelPotPickup, AfterAGainTheFarSideDoesNothing)
{
  PanelPotPickup p;
  float stored = 0.5f;
  p.panelGained ();
  EXPECT_FALSE (move (p, 0, 0.9f, stored));
  EXPECT_FALSE (move (p, 0, 0.8f, stored));
  EXPECT_FLOAT_EQ (stored, 0.5f);
}

TEST (PanelPotPickup, CrossingTheValueActs)
{
  PanelPotPickup p;
  float stored = 0.5f;
  p.panelGained ();
  EXPECT_FALSE (move (p, 0, 0.9f, stored));
  EXPECT_FALSE (move (p, 0, 0.6f, stored));
  EXPECT_EQ (move (p, 0, 0.4f, stored), 0.4f);
  EXPECT_EQ (move (p, 0, 0.3f, stored), 0.3f);
}

TEST (PanelPotPickup, AStaleReadingFromBeforeTheHandOverIsNotACrossing)
{
  PanelPotPickup p;
  float stored = 0.2f;
  // the pot stood at 0.1 when the other deck took the panel
  EXPECT_EQ (move (p, 0, 0.1f, stored), 0.1f);
  stored = 0.2f;
  p.panelGained ();
  EXPECT_FALSE (move (p, 0, 0.9f, stored));
  EXPECT_FLOAT_EQ (stored, 0.2f);
}

TEST (PanelPotPickup, ComingWithinAHairOfTheValueActsWithoutCrossing)
{
  PanelPotPickup p;
  float stored = 0.5f;
  p.panelGained ();
  EXPECT_EQ (move (p, 0, 0.505f, stored), 0.505f);
}

TEST (PanelPotPickup, AValueMovedAwayFromAPickedUpPotArmsItAgain)
{
  PanelPotPickup p;
  float stored = 0.5f;
  p.panelGained ();
  EXPECT_FALSE (move (p, 0, 0.9f, stored));
  EXPECT_EQ (move (p, 0, 0.4f, stored), 0.4f);

  stored = 0.8f; // a touch or a clip load; the pot stands at 0.4, below it
  EXPECT_FALSE (move (p, 0, 0.3f, stored));
  EXPECT_FALSE (move (p, 0, 0.5f, stored));
  EXPECT_FLOAT_EQ (stored, 0.8f);
  EXPECT_EQ (move (p, 0, 0.9f, stored), 0.9f);
}

TEST (PanelPotPickup, AValueMovedTowardsTheHandStillFollowsTheHandAfterACrossing)
{
  PanelPotPickup p;
  float stored = 0.5f;
  p.panelGained ();
  EXPECT_FALSE (move (p, 0, 0.9f, stored));
  EXPECT_EQ (move (p, 0, 0.4f, stored), 0.4f);
  stored = 0.2f; // moved away from the pot at 0.4 (pot is above it)
  // the pot's next move straddles the new value: it picks up straight away
  EXPECT_EQ (move (p, 0, 0.1f, stored), 0.1f);
}

TEST (PanelPotPickup, AGainAfterAPickUpArmsAgain)
{
  PanelPotPickup p;
  float stored = 0.5f;
  p.panelGained ();
  EXPECT_FALSE (move (p, 0, 0.9f, stored));
  EXPECT_EQ (move (p, 0, 0.4f, stored), 0.4f);
  p.panelGained ();
  EXPECT_FALSE (move (p, 0, 0.9f, stored));
}

TEST (PanelPotPickup, EachPotIsIndependent)
{
  PanelPotPickup p;
  float a = 0.5f, b = 0.5f;
  p.panelGained ();
  EXPECT_FALSE (move (p, 0, 0.9f, a));
  EXPECT_FALSE (move (p, 1, 0.9f, b));
  EXPECT_EQ (move (p, 0, 0.4f, a), 0.4f);
  EXPECT_FALSE (move (p, 1, 0.8f, b));
  a = 0.9f; // re-arms pot 0 only
  EXPECT_FALSE (move (p, 0, 0.3f, a));
  EXPECT_FALSE (move (p, 1, 0.7f, b));
  EXPECT_EQ (move (p, 1, 0.4f, b), 0.4f);
}

TEST (PanelPotPickup, ARangeOutsideThePotsIsIgnored)
{
  PanelPotPickup p;
  EXPECT_EQ (p.feed (-1, 0.3f, 0.f), 0.3f);
  EXPECT_EQ (p.feed (PanelPotPickup::count, 0.3f, 0.f), 0.3f);
}
