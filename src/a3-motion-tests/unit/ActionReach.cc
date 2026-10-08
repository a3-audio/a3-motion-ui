/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <a3-motion-ui/components/fpv/ActionReach.hh>

#include <UiSource.hh>

using namespace a3;

// What an action's Pilot section means at a press, by view: the one decision
// the app's three ways of firing an action share.

namespace
{
PilotOrder
asking (std::optional<PilotGame> game)
{
  PilotOrder order;
  order.game = game;
  order.target = PilotTarget{ PilotTargetKind::Group, 1 };
  order.with = PilotRecruit::Nearest;
  return order;
}
}

TEST (ActionReach, FullIgnoresThePilotSection)
{
  EXPECT_FALSE (pilotOrderAtPress (AppView::Full, asking (PilotGame::FakeOut)).has_value ());
  EXPECT_FALSE (pilotOrderAtPress (AppView::Full, asking (PilotGame::None)).has_value ());
}

TEST (ActionReach, FpvHandsTheWholeOrderToThePilot)
{
  auto const order = pilotOrderAtPress (AppView::Fpv, asking (PilotGame::FakeOut));
  ASSERT_TRUE (order.has_value ());
  EXPECT_EQ (*order->game, PilotGame::FakeOut);
  EXPECT_EQ (order->target.groupId, 1);
  EXPECT_EQ (order->with, PilotRecruit::Nearest);
}

TEST (ActionReach, NoGameNamedIsNothingToHandOn)
{
  EXPECT_FALSE (pilotOrderAtPress (AppView::Fpv, asking (std::nullopt)).has_value ());
}

TEST (ActionReach, NoneIsHandedOnToCallAGameOff)
{
  auto const order = pilotOrderAtPress (AppView::Fpv, asking (PilotGame::None));
  ASSERT_TRUE (order.has_value ());
  EXPECT_EQ (*order->game, PilotGame::None);
}

TEST (ActionReach, TheReadoutNamesTheGame)
{
  EXPECT_EQ (pilotReadout (0, "A3", PilotGame::FakeOut), "CH1 A3 GAME FAKEOUT");
  EXPECT_EQ (pilotReadout (3, "A6", PilotGame::CallAndResponse), "CH4 A6 GAME CALLRESPONSE");
  EXPECT_EQ (pilotReadout (1, "A1", PilotGame::None), "CH2 A1 NO GAME");
}

// The component that owns the engine cannot be built in the runner, so the
// one route is read from its source, as ChannelPotTurn does.

namespace
{
int
occurrencesIn (juce::String const &text, juce::String const &word)
{
  int count = 0;
  for (auto at = text.indexOf (word); at >= 0; at = text.indexOf (at + 1, word))
    ++count;
  return count;
}

juce::String
uiComponentSource ()
{
  return juce::File (A3_UI_SOURCE_DIR)
      .getChildFile ("components/A3MotionUIComponent.cc")
      .loadFileAsString ();
}
}

TEST (ActionReach, OnlyTheOneRouteSetsAChannelsAction)
{
  auto const route = a3::test::uiComponentBodyOf (
      "A3MotionUIComponent::sendFiredAction (index_t channel,");
  ASSERT_TRUE (route.isNotEmpty ());

  auto const inRoute = occurrencesIn (route, "_engine.setChannelAction (");
  EXPECT_EQ (inRoute, 2);
  EXPECT_EQ (occurrencesIn (uiComponentSource (), "_engine.setChannelAction ("),
             inRoute);
}

TEST (ActionReach, TheRouteHandsThePilotSectionOnByView)
{
  auto const route = a3::test::uiComponentBodyOf (
      "A3MotionUIComponent::sendFiredAction (index_t channel,");
  ASSERT_TRUE (route.isNotEmpty ());

  EXPECT_TRUE (route.contains ("fired->flight"));
  EXPECT_TRUE (route.contains ("pilotOrderAtPress (_view, fired->pilot)"));
  EXPECT_TRUE (route.contains ("_engine.requestGame (channel, *order)"));
  EXPECT_TRUE (route.contains ("pilotReadout ("));
}

TEST (ActionReach, EveryWayOfFiringGoesThroughTheRoute)
{
  auto const accent = uiComponentSource ()
                          .fromFirstOccurrenceOf ("_clipSettings->onAccentHeld", false, false)
                          .upToFirstOccurrenceOf ("};", false, false);
  EXPECT_TRUE (accent.contains ("sendFiredAction ("));

  EXPECT_TRUE (a3::test::uiComponentBodyOf (
                   "A3MotionUIComponent::handlePadPress (index_t channel, index_t pad)")
                   .contains ("sendFiredAction (channel, fired, name)"));
  EXPECT_TRUE (a3::test::uiComponentBodyOf (
                   "A3MotionUIComponent::fireChainedAction (index_t channel, int button)")
                   .contains ("sendFiredAction ("));
}

TEST (ActionReach, AButtonFiresItsScriptFromOneRun)
{
  auto const body = a3::test::uiComponentBodyOf (
      "A3MotionUIComponent::firedActionOf (index_t channel, int button)");
  ASSERT_TRUE (body.isNotEmpty ());
  EXPECT_TRUE (body.contains ("fireActionAt (action.source"));
}
