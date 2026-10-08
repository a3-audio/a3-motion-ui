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

#include <a3-motion-ui/components/fpv/FpvFloor.hh>

using namespace a3;

namespace
{
FlightBodies
floorWith (std::initializer_list<FlightBody> list)
{
  FlightBodies bodies;
  for (auto const &b : list)
    bodies.body[static_cast<size_t> (bodies.count++)] = b;
  return bodies;
}
}

// --- where a finger goes ---------------------------------------------------

TEST (FpvFloor, OnlyTheFloorUnderTheSphereIsFloor)
{
  EXPECT_TRUE (onTheFloor (0.5f, { 0.3f, 0.2f }));
  EXPECT_FALSE (onTheFloor (1.2f, { 0.3f, 0.2f })) << "beside the sphere";
  EXPECT_FALSE (onTheFloor (0.9f, { 1.1f, 0.f })) << "below the horizon";
}

TEST (FpvFloor, AFingerAloneOnTheFloorIsTheGroupGesture)
{
  EXPECT_EQ (fpvFingerDown (true, true, false), FpvFingerDown::Floor);
}

TEST (FpvFloor, OffTheFloorIsTheCameraWithItsDoubleTap)
{
  EXPECT_EQ (fpvFingerDown (true, false, false), FpvFingerDown::Camera);
  EXPECT_EQ (fpvFingerDown (true, false, true), FpvFingerDown::Camera);
}

TEST (FpvFloor, ASecondFingerIsAPinch)
{
  EXPECT_EQ (fpvFingerDown (false, true, false), FpvFingerDown::Camera);
  EXPECT_EQ (fpvFingerDown (false, true, true), FpvFingerDown::Camera);
}

// While Page is held a finger on the floor is the Page's, and never the
// gesture's: a hold on a group would otherwise remove it, not escort it.
TEST (FpvFloor, WhilePageIsHeldTheFloorIsAPageTap)
{
  EXPECT_EQ (fpvFingerDown (true, true, true), FpvFingerDown::PageTap);
}

TEST (FpvFloor, TheNearestBodyUnderTheFingerIsHit)
{
  std::array<BodyOnScreen, maxFlightBodies> bodies{};
  bodies[0] = { 4, { 100.f, 100.f }, 20.f };
  bodies[1] = { 6, { 120.f, 100.f }, 20.f };
  EXPECT_EQ (bodyUnderFinger (bodies, 2, { 115.f, 100.f }), 6);
  EXPECT_EQ (bodyUnderFinger (bodies, 2, { 102.f, 100.f }), 4);
  EXPECT_EQ (bodyUnderFinger (bodies, 2, { 100.f, 150.f }), std::nullopt);
  EXPECT_EQ (bodyUnderFinger (bodies, 1, { 125.f, 100.f }), std::nullopt)
      << "only the first count are bodies";
}

// --- escorts -----------------------------------------------------------------

TEST (FpvFloor, ARemovedBodyLetsItsEscortsGo)
{
  FpvEscorts escorts;
  escorts.set (0, 2);
  escorts.set (1, 2);
  escorts.set (2, 5);
  auto const changed = escorts.forget (2);
  EXPECT_EQ (escorts.of (0), noBodyId);
  EXPECT_EQ (escorts.of (1), noBodyId);
  EXPECT_EQ (escorts.of (2), 5);
  EXPECT_TRUE (changed[0]);
  EXPECT_TRUE (changed[1]);
  EXPECT_FALSE (changed[2]);
  EXPECT_FALSE (changed[3]);
}

TEST (FpvFloor, AChannelOutOfRangeChangesNothing)
{
  FpvEscorts escorts;
  escorts.set (7, 2);
  escorts.set (-1, 2);
  for (int ch = 0; ch < 4; ++ch)
    EXPECT_EQ (escorts.of (ch), noBodyId);
  EXPECT_EQ (escorts.of (9), noBodyId);
}

TEST (FpvFloor, AnEscortIsShownWithItsBodysMass)
{
  auto const floor = floorWith ({ { { 0.f, 0.f }, 1.f, 0 },
                                  { { 0.5f, 0.f }, 3.f, 3 } });
  auto const view = escortView (3, true, floor);
  EXPECT_EQ (view.escort, 3);
  EXPECT_FLOAT_EQ (view.mass, 3.f);
}

TEST (FpvFloor, AClipShipShowsNoEscort)
{
  auto const floor = floorWith ({ { { 0.f, 0.f }, 2.f, 1 } });
  EXPECT_EQ (escortView (1, false, floor).escort, noBodyId);
}

TEST (FpvFloor, AGoneBodyShowsPatrol)
{
  auto const floor = floorWith ({ { { 0.f, 0.f }, 2.f, 1 } });
  EXPECT_EQ (escortView (4, true, floor).escort, noBodyId);
  EXPECT_EQ (escortView (noBodyId, true, floor).escort, noBodyId);
}

// The engine does not escort a dead zone, it patrols; the strip says so,
// also when the escorted group was cycled to a dead zone.
TEST (FpvFloor, ADeadZoneShowsPatrol)
{
  auto const floor = floorWith ({ { { 0.f, 0.f }, -2.f, 1 } });
  EXPECT_EQ (escortView (1, true, floor).escort, noBodyId);
}

// --- the Page pad held -------------------------------------------------------

TEST (FpvPageHold, APlainPressAndReleaseToggles)
{
  FpvPageHold hold;
  hold.press (2);
  EXPECT_TRUE (hold.isHeld ());
  EXPECT_EQ (hold.release (2), PageOutcome::Toggle);
  EXPECT_FALSE (hold.isHeld ());
}

TEST (FpvPageHold, ATapOnABodyWhileHeldEscortsIt)
{
  FpvPageHold hold;
  hold.press (1);
  hold.tap (5);
  EXPECT_EQ (hold.tappedBody (), 5);
  EXPECT_EQ (hold.release (1), PageOutcome::Escort);
}

TEST (FpvPageHold, ATapOnTheFloorWhileHeldPatrols)
{
  FpvPageHold hold;
  hold.press (1);
  hold.tap (std::nullopt);
  EXPECT_EQ (hold.release (1), PageOutcome::Patrol);
}

TEST (FpvPageHold, TheLastTapCounts)
{
  FpvPageHold hold;
  hold.press (0);
  hold.tap (std::nullopt);
  hold.tap (2);
  EXPECT_EQ (hold.release (0), PageOutcome::Escort);
  EXPECT_EQ (hold.tappedBody (), std::nullopt) << "the release forgets";
}

// Ids are reused: a tapped body that went must not hand its escort to the
// next group that takes its number.
TEST (FpvPageHold, ATappedBodyThatWentIsATapOnTheFloor)
{
  FpvPageHold hold;
  hold.press (0);
  hold.tap (2);
  hold.bodyRemoved (3);
  EXPECT_EQ (hold.tappedBody (), 2);
  hold.bodyRemoved (2);
  EXPECT_EQ (hold.release (0), PageOutcome::Patrol);
}

TEST (FpvPageHold, AnotherChannelsReleaseDoesNothing)
{
  FpvPageHold hold;
  hold.press (0);
  EXPECT_EQ (hold.release (1), std::nullopt);
  EXPECT_TRUE (hold.isHeld ());
}

TEST (FpvPageHold, ATapWithNothingHeldIsIgnored)
{
  FpvPageHold hold;
  hold.tap (3);
  hold.press (0);
  EXPECT_EQ (hold.release (0), PageOutcome::Toggle);
}

TEST (FpvPageHold, ClearedItAnswersNothing)
{
  FpvPageHold hold;
  hold.press (0);
  hold.clear ();
  EXPECT_FALSE (hold.isHeld ());
  EXPECT_EQ (hold.release (0), std::nullopt);
}

// --- what the readout says ---------------------------------------------------

TEST (FpvFloor, TheReadoutNamesTheChannelAndWhatItDoesNow)
{
  EXPECT_EQ (fpvPageReadout (0, PageOutcome::Toggle, true, noBodyId),
             "CH1 ORBIT");
  EXPECT_EQ (fpvPageReadout (1, PageOutcome::Toggle, false, noBodyId),
             "CH2 CLIP");
  EXPECT_EQ (fpvPageReadout (1, PageOutcome::Escort, true, 2),
             juce::String::fromUTF8 ("CH2 \xe2\x86\x92 G3"));
  EXPECT_EQ (fpvPageReadout (3, PageOutcome::Patrol, true, noBodyId),
             "CH4 PATROL");
}

// --- readouts that tell the truth --------------------------------------------

TEST (FpvFloor, FullSaysOrbitOnAPlayOrPagePadForAnOrbitChannel)
{
  EXPECT_EQ (padPressReadout (1, "PLAYPAUSE", AppView::Full, true, true),
             "CH2 ORBIT");
  EXPECT_EQ (padPressReadout (1, "PAGE", AppView::Full, true, true),
             "CH2 ORBIT");
}

TEST (FpvFloor, FullKeepsThePadNameOtherwise)
{
  EXPECT_EQ (padPressReadout (0, "PLAYPAUSE", AppView::Full, false, true),
             "CH1 PLAYPAUSE");
  EXPECT_EQ (padPressReadout (0, "A3", AppView::Full, true, false), "CH1 A3");
  EXPECT_EQ (padPressReadout (0, "PAGE", AppView::Fpv, true, true),
             "CH1 PAGE");
}

TEST (FpvFloor, ADeadZoneEscortIsReportedAsPatrol)
{
  auto const floor = floorWith ({ { { 0.f, 0.f }, -2.f, 1 },
                                  { { 0.5f, 0.f }, 2.f, 2 } });
  EXPECT_EQ (flownOutcome (PageOutcome::Escort, 1, floor), PageOutcome::Patrol);
  EXPECT_EQ (flownOutcome (PageOutcome::Escort, 2, floor), PageOutcome::Escort);
  EXPECT_EQ (flownOutcome (PageOutcome::Toggle, 1, floor), PageOutcome::Toggle);
}

TEST (FpvFloor, AnOrbitToggleWithoutAClipSaysSo)
{
  EXPECT_EQ (fpvPageReadout (0, PageOutcome::Toggle, true, noBodyId, false),
             juce::String::fromUTF8 ("CH1 ORBIT \xc2\xb7 no clip"));
  EXPECT_EQ (fpvPageReadout (0, PageOutcome::Toggle, true, noBodyId, true),
             "CH1 ORBIT");
  EXPECT_EQ (fpvPageReadout (0, PageOutcome::Toggle, false, noBodyId, false),
             "CH1 CLIP");
}
