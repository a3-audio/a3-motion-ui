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

#include <a3-motion-ui/components/StatusBar.hh>
#include <a3-motion-ui/components/WorkspaceList.hh>
#include <a3-motion-ui/io/Workspaces.hh>

using namespace a3;

namespace
{
// What `i3-msg -t get_workspaces` prints on the rig, cut to the fields read.
juce::String const rigWorkspaces = R"([
  {"num":1,"name":"1:MOTION","visible":false,"focused":false},
  {"num":2,"name":"2:STEMDECK","visible":true,"focused":true},
  {"num":3,"name":"3:REAPER","visible":false,"focused":false},
  {"num":-1,"name":"scratch","visible":false,"focused":false}
])";
}

// The label is the name after the number; i3 is asked by number.
TEST (Workspaces, TheListIsWhatI3Reports)
{
  auto const list = workspaces::parse (rigWorkspaces);

  ASSERT_EQ (list.size (), 3u) << "a workspace without a number is not offered";
  EXPECT_EQ (list[0].number, 1);
  EXPECT_EQ (list[0].label, "MOTION");
  EXPECT_FALSE (list[0].current);
  EXPECT_EQ (list[1].label, "STEMDECK");
  EXPECT_TRUE (list[1].current);
  EXPECT_EQ (list[2].number, 3);
}

TEST (Workspaces, ANameWithoutALabelKeepsItsNumber)
{
  auto const list = workspaces::parse (R"([{"num":7,"name":"7","focused":false}])");

  ASSERT_EQ (list.size (), 1u);
  EXPECT_EQ (list[0].label, "7");
}

// No i3, or i3 said something else: nothing to offer, and no crash.
TEST (Workspaces, AnythingButAListIsNoWorkspaces)
{
  EXPECT_TRUE (workspaces::parse ("").empty ());
  EXPECT_TRUE (workspaces::parse ("{\"error\":\"no socket\"}").empty ());
  EXPECT_TRUE (workspaces::parse ("[[1,2],\"x\"]").empty ());
}

// ── The list on the screen ───────────────────────────────────────────────

namespace
{
struct ShownList
{
  WorkspaceList list;
  std::vector<int> chosen;

  ShownList ()
  {
    list.setBounds (0, 0, 768, 1024);
    list.onChosen = [this] (int number) { chosen.push_back (number); };
    // The two bar keys it opens from, as the device lays them out.
    list.show (workspaces::parse (rigWorkspaces), { 543, 5, 75, 25 });
  }

  void tapAt (juce::Point<int> at)
  {
    auto const source = juce::Desktop::getInstance ().getMainMouseSource ();
    juce::MouseEvent const event (
        source, at.toFloat (), {}, juce::MouseInputSource::defaultPressure,
        0.f, 0.f, 0.f, 0.f, &list, &list, juce::Time::getCurrentTime (),
        at.toFloat (), juce::Time::getCurrentTime (), 1, false);
    list.mouseUp (event);
  }
};
}

// One key per workspace, a column under the keys it opened from, inside the
// window: a juce::PopupMenu is a window of its own and came up black on the
// rig (i3, no compositor; StemDeck, 2026-09-30).
TEST (WorkspaceList, OneKeyPerWorkspaceUnderTheBarKeys)
{
  ShownList s;

  ASSERT_TRUE (s.list.isVisible ());
  ASSERT_EQ (s.list.keyCount (), 3);
  for (int i = 0; i < 3; ++i)
    {
      auto const key = s.list.keyArea (i);
      EXPECT_GE (key.getY (), 5 + 25) << i;
      EXPECT_LE (key.getRight (), 543 + 75) << i;
      EXPECT_GE (key.getHeight (), 25) << "a key a finger can hit";
      EXPECT_TRUE (s.list.getLocalBounds ().contains (key)) << i;
    }
  EXPECT_LE (s.list.keyArea (0).getBottom (), s.list.keyArea (1).getY ());
}

TEST (WorkspaceList, AKeyGoesToItsWorkspaceAndCloses)
{
  ShownList s;

  s.tapAt (s.list.keyArea (2).getCentre ());

  ASSERT_EQ (s.chosen.size (), 1u);
  EXPECT_EQ (s.chosen[0], 3);
  EXPECT_FALSE (s.list.isVisible ());
}

TEST (WorkspaceList, ATapBesideTheKeysOnlyCloses)
{
  ShownList s;

  s.tapAt ({ 100, 800 });

  EXPECT_TRUE (s.chosen.empty ());
  EXPECT_FALSE (s.list.isVisible ());
}

// Nothing to offer is nothing to show: no empty frame over the screen.
TEST (WorkspaceList, NoWorkspacesShowNothing)
{
  WorkspaceList list;
  list.setBounds (0, 0, 768, 1024);
  list.show ({}, { 543, 5, 75, 25 });

  EXPECT_FALSE (list.isVisible ());
}

TEST (WorkspaceList, ItPaints)
{
  ShownList s;
  auto const image = s.list.createComponentSnapshot (s.list.getLocalBounds ());
  EXPECT_EQ (image.getWidth (), 768);
}

// ── The two keys on the bar ──────────────────────────────────────────────

namespace
{
struct Bar
{
  juce::Value bpm{ 120.0 };
  StatusBar bar{ bpm };
  int deck = 0;
  int arrow = 0;

  Bar ()
  {
    bar.setBounds (0, 0, 768, bar.preferredHeight ());
    bar.onDeckKeyTapped = [this] { ++deck; };
    bar.onWorkspacesKeyTapped = [this] { ++arrow; };
  }

  void tapAt (juce::Point<int> at)
  {
    auto const source = juce::Desktop::getInstance ().getMainMouseSource ();
    juce::MouseEvent const event (
        source, at.toFloat (), {}, juce::MouseInputSource::defaultPressure,
        0.f, 0.f, 0.f, 0.f, &bar, &bar, juce::Time::getCurrentTime (),
        at.toFloat (), juce::Time::getCurrentTime (), 1, false);
    bar.mouseUp (event);
  }
};
}

TEST (StatusBarWorkspaceKeys, DeckAndTheArrowEachSayTheyWereHit)
{
  Bar b;
  auto const anchor = b.bar.workspacesAnchor ();
  ASSERT_FALSE (anchor.isEmpty ());

  b.tapAt ({ anchor.getX () + 2, anchor.getCentreY () });
  EXPECT_EQ (b.deck, 1);
  EXPECT_EQ (b.arrow, 0);

  b.tapAt ({ anchor.getRight () - 2, anchor.getCentreY () });
  EXPECT_EQ (b.deck, 1);
  EXPECT_EQ (b.arrow, 1);
}

TEST (StatusBarWorkspaceKeys, TheBarPaintsWithThem)
{
  Bar b;
  auto const image = b.bar.createComponentSnapshot (b.bar.getLocalBounds ());
  EXPECT_EQ (image.getWidth (), 768);
}
