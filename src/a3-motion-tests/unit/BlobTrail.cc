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

#include <a3-motion-ui/components/BlobTrail.hh>

#include <cmath>

#include <gtest/gtest.h>

namespace a3
{

namespace
{
constexpr float lag = 0.3f;
constexpr float cut = 0.35f;

float
distance (float ax, float ay, float bx, float by)
{
  return std::hypot (ax - bx, ay - by);
}
}

TEST (BlobTrail, TheFirstFrameLaysTheWholeTailOnTheBlob)
{
  // Otherwise the wake's first frame is a stripe from the origin to wherever
  // the clip happens to start.
  BlobTrail trail;
  advanceBlobTrail (trail, 0.4f, -0.2f, lag, cut);

  for (int k = 0; k < BlobTrail::numLinks; ++k)
    {
      EXPECT_NEAR (trail.x[k], 0.4f, 1e-5f);
      EXPECT_NEAR (trail.y[k], -0.2f, 1e-5f);
    }
  EXPECT_TRUE (trail.primed);
}

TEST (BlobTrail, EachLinkFallsFurtherBehindThanTheOneInFrontOfIt)
{
  BlobTrail trail;
  advanceBlobTrail (trail, 0.f, 0.f, lag, cut);

  // Walk it a short way, in steps small enough not to count as a jump.
  for (int frame = 1; frame <= 20; ++frame)
    advanceBlobTrail (trail, static_cast<float> (frame) * 0.01f, 0.f, lag, cut);

  auto const head = 0.20f;
  auto previous = 0.f;
  for (int k = 0; k < BlobTrail::numLinks; ++k)
    {
      auto const behind = head - trail.x[k];
      EXPECT_GT (behind, previous);
      previous = behind;
    }
}

TEST (BlobTrail, AParkedBlobHasNoTail)
{
  // The shader gates the plume on how far the tail has been left behind, and
  // this is the state that has to reach zero for a still blob to be still.
  BlobTrail trail;
  advanceBlobTrail (trail, 0.f, 0.f, lag, cut);
  for (int frame = 0; frame < 30; ++frame)
    advanceBlobTrail (trail, 0.5f, 0.f, lag, cut);
  for (int frame = 0; frame < 200; ++frame)
    advanceBlobTrail (trail, 0.5f, 0.f, lag, cut);

  for (int k = 0; k < BlobTrail::numLinks; ++k)
    EXPECT_LT (distance (trail.x[k], trail.y[k], 0.5f, 0.f), 1e-3f);
}

TEST (BlobTrail, AJumpCutsTheTailRatherThanDraggingItAcross)
{
  // A clip looping back to its start, or a finger dropping the blob somewhere
  // else. Dragged, the wake draws a line the sound never travelled.
  BlobTrail trail;
  advanceBlobTrail (trail, -0.6f, 0.f, lag, cut);
  for (int frame = 0; frame < 10; ++frame)
    advanceBlobTrail (trail, -0.6f + static_cast<float> (frame) * 0.01f, 0.f,
                      lag, cut);

  advanceBlobTrail (trail, 0.7f, 0.3f, lag, cut);

  for (int k = 0; k < BlobTrail::numLinks; ++k)
    {
      EXPECT_NEAR (trail.x[k], 0.7f, 1e-5f);
      EXPECT_NEAR (trail.y[k], 0.3f, 1e-5f);
    }
}

TEST (BlobTrail, AMovementExactlyAtTheThresholdIsStillAMovement)
{
  // The cut is for what is plainly a jump; a boundary that cut on equality
  // would flicker for anything travelling at exactly that speed.
  BlobTrail trail;
  advanceBlobTrail (trail, 0.f, 0.f, lag, cut);
  advanceBlobTrail (trail, cut, 0.f, lag, cut);

  EXPECT_LT (trail.x[0], cut);
  EXPECT_GT (trail.x[0], 0.f);
}

TEST (BlobTrail, ReleasingItMeansTheNextFrameStartsCleanRatherThanFromHere)
{
  // A channel that stops and starts again elsewhere must not be given a wake
  // joining the two places.
  BlobTrail trail;
  advanceBlobTrail (trail, -0.5f, 0.f, lag, cut);
  releaseBlobTrail (trail);
  EXPECT_FALSE (trail.primed);

  advanceBlobTrail (trail, 0.5f, 0.f, lag, cut);
  for (int k = 0; k < BlobTrail::numLinks; ++k)
    EXPECT_NEAR (trail.x[k], 0.5f, 1e-5f);
}

TEST (BlobTrail, TheTailNeverOvershootsTheBlobItChases)
{
  // A lag of 1 is the fastest anything here is asked to chase; above it a link
  // would swing past what it is following and the plume would fold over itself.
  BlobTrail trail;
  advanceBlobTrail (trail, 0.f, 0.f, 1.f, cut);
  for (int frame = 1; frame <= 10; ++frame)
    advanceBlobTrail (trail, static_cast<float> (frame) * 0.02f, 0.f, 1.f, cut);

  for (int k = 0; k < BlobTrail::numLinks; ++k)
    EXPECT_LE (trail.x[k], 0.2f + 1e-5f);
}

}
