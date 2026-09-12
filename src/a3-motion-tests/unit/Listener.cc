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

#include <a3-motion-ui/components/Listener.hh>

using namespace a3;

// The outline of a square with a point in the middle of it is the square: a
// hull that kept the middle one would put a crease across the figure.
TEST (Listener, TheOutlineDropsWhatIsInside)
{
  std::vector<juce::Point<float> > const points{
    { 0.f, 0.f }, { 1.f, 0.f }, { 1.f, 1.f }, { 0.f, 1.f }, { 0.5f, 0.5f }
  };

  EXPECT_EQ (outlineOf (points).size (), 4u);
}

// And it is a ring, not a scribble: no point of the outline is inside the
// figure the others make.
TEST (Listener, TheOutlineIsAConvexRing)
{
  std::vector<juce::Point<float> > points;
  for (int i = 0; i < 40; ++i)
    {
      auto const a = juce::MathConstants<float>::twoPi * i / 40.f;
      points.push_back (
          { std::cos (a) * (i % 2 == 0 ? 1.f : 0.6f), std::sin (a) });
    }

  auto const hull = outlineOf (points);
  ASSERT_GE (hull.size (), 3u);

  for (size_t i = 0; i < hull.size (); ++i)
    {
      auto const &o = hull[i];
      auto const &a = hull[(i + 1) % hull.size ()];
      auto const &b = hull[(i + 2) % hull.size ()];

      auto const turn = (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
      EXPECT_GT (turn, -1e-4f) << "the outline turns back on itself at " << i;
    }
}

// It is a person, and a person is taller than they are wide -- from the side.
TEST (Listener, FromTheHorizonItStands)
{
  auto const from
      = listenerSilhouette ({ juce::MathConstants<float>::halfPi, 0.f }, 100.f);
  auto const box = from.getBounds ();

  EXPECT_GT (box.getHeight (), box.getWidth () * 1.8f)
      << "seen from the horizon a standing person should be tall and narrow";
  EXPECT_NEAR (box.getHeight (), 100.f, 12.f) << "and about the height asked for";
}

// From straight above they are a head and a pair of shoulders, and wider
// across than they are deep -- which is what says they are facing somewhere.
TEST (Listener, FromOverheadItIsAHeadAndShoulders)
{
  auto const above = listenerSilhouette ({}, 100.f);
  auto const box = above.getBounds ();

  EXPECT_LT (box.getHeight (), 40.f) << "from above a person is not tall";
  EXPECT_GT (box.getWidth (), box.getHeight ())
      << "shoulders are wider than a body is deep";
}

// And they face the front of the room: the nose reaches further towards it
// than the back of the head reaches the other way.
TEST (Listener, ItFacesTheFrontOfTheRoom)
{
  auto const above = listenerSilhouette ({}, 100.f);
  auto const box = above.getBounds ();

  // The overhead view puts the room's front up the screen, so "further
  // forward" is further up: a smaller y.
  EXPECT_GT (std::abs (box.getY ()), std::abs (box.getBottom ()) * 1.15f)
      << "it is the same distance either way, so nothing says which way it "
         "is looking";
}

// Nothing to draw is nothing drawn, rather than a figure of no size.
TEST (Listener, NoHeightIsNoFigure)
{
  EXPECT_TRUE (listenerSilhouette ({}, 0.f).isEmpty ());
}
