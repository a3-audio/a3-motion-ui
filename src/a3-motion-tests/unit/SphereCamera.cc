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

#include <cmath>

#include <JuceHeader.h>

#include <a3-motion-ui/components/SphereProjection.hh>

using namespace a3;

namespace
{
constexpr float quarterTurn = juce::MathConstants<float>::halfPi;

void
expectSame (Pos const &a, Pos const &b, juce::String const &what)
{
  EXPECT_NEAR (a.x (), b.x (), 1e-5f) << what;
  EXPECT_NEAR (a.y (), b.y (), 1e-5f) << what;
  EXPECT_NEAR (a.z (), b.z (), 1e-5f) << what;
}
}

// The default is the view the device has always had, and it has to be the
// identity to the bit: a camera nobody has touched must not move a single
// blob, or every take on every stick lands somewhere new.
TEST (SphereCamera, LookingStraightDownChangesNothing)
{
  SphereCamera overhead;
  EXPECT_TRUE (overhead.isOverhead ());

  Pos const points[] = { Pos::fromCartesian (0.f, 0.f, 1.f),
                         Pos::fromCartesian (1.f, 0.f, 0.f),
                         Pos::fromCartesian (0.f, -1.f, 0.f),
                         Pos::fromCartesian (0.3f, -0.5f, 0.81f) };

  for (auto const &p : points)
    {
      expectSame (asSeenFrom (p, overhead), p, "seen from overhead");
      expectSame (asSeenFromInverse (p, overhead), p, "and back");
    }
}

// Leaning the eye down brings what was overhead towards the top of the
// picture. The screen's vertical is the room's x -- cartesian2DHOA2JUCE puts
// a position at { -y, -x } -- so the zenith arriving at "up" means it arrives
// at +x.
TEST (SphereCamera, LeaningOverBringsTheZenithToTheTopOfThePicture)
{
  SphereCamera fromTheHorizon;
  fromTheHorizon.pitch = quarterTurn;

  auto const zenith = asSeenFrom (Pos::fromCartesian (0.f, 0.f, 1.f),
                                  fromTheHorizon);

  EXPECT_NEAR (zenith.x (), 1.f, 1e-5f);
  EXPECT_NEAR (zenith.z (), 0.f, 1e-5f);

  // And what was in front of the listener has gone under the eye.
  auto const front = asSeenFrom (Pos::fromCartesian (1.f, 0.f, 0.f),
                                 fromTheHorizon);
  EXPECT_NEAR (front.z (), -1.f, 1e-5f);
}

// Walking round the room turns it under the eye, and leaves the zenith where
// it is -- it is the one direction a turn cannot move.
TEST (SphereCamera, WalkingRoundTurnsTheRoomAndLeavesTheZenith)
{
  SphereCamera walked;
  walked.turn = quarterTurn;

  expectSame (asSeenFrom (Pos::fromCartesian (0.f, 0.f, 1.f), walked),
              Pos::fromCartesian (0.f, 0.f, 1.f), "the zenith");

  auto const front
      = asSeenFrom (Pos::fromCartesian (1.f, 0.f, 0.f), walked);
  EXPECT_NEAR (front.x (), 0.f, 1e-5f);
  EXPECT_NEAR (std::abs (front.y ()), 1.f, 1e-5f);
}

// The inverse is the inverse. A finger lands on the view and has to be handed
// back to the room; if these two disagree the blob does not come out under
// the finger, which is the one thing a drag must do.
TEST (SphereCamera, WhatIsSeenCanAlwaysBeGivenBackToTheRoom)
{
  Pos const points[] = { Pos::fromCartesian (0.f, 0.f, 1.f),
                         Pos::fromCartesian (1.f, 0.f, 0.f),
                         Pos::fromCartesian (0.f, 1.f, 0.f),
                         Pos::fromCartesian (0.3f, -0.5f, 0.81f),
                         Pos::fromCartesian (-0.6f, 0.48f, -0.64f) };

  for (float pitch : { 0.f, 0.3f, quarterTurn, 1.2f })
    for (float turn : { 0.f, -0.7f, 2.5f })
      {
        SphereCamera camera;
        camera.pitch = pitch;
        camera.turn = turn;

        for (auto const &p : points)
          expectSame (asSeenFromInverse (asSeenFrom (p, camera), camera), p,
                      "pitch " + juce::String (pitch) + ", turn "
                          + juce::String (turn));
      }
}

// And it is a rotation, so nothing stretches: every direction stays on the
// unit sphere however the eye is placed.
TEST (SphereCamera, TheViewTurnsTheRoomWithoutStretchingIt)
{
  SphereCamera camera;
  camera.pitch = 0.9f;
  camera.turn = -2.1f;

  for (int i = 0; i < 16; ++i)
    {
      auto const a = juce::MathConstants<float>::twoPi * i / 16.f;
      auto const on = Pos::fromCartesian (std::cos (a) * 0.6f,
                                          std::sin (a) * 0.6f, 0.8f);
      auto const seen = asSeenFrom (on, camera);

      EXPECT_NEAR (std::sqrt (seen.x () * seen.x () + seen.y () * seen.y ()
                              + seen.z () * seen.z ()),
                   1.f, 1e-5f);
    }
}
