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

#include <a3-motion-ui/components/UnderlayLook.hh>

using namespace a3;

// The underlay -- the take being written over, drawn faintly under the new
// one -- does not change while a take is recorded, and it was drawn afresh
// every frame: a sixth of the render thread (2026-09-25). It is drawn once
// and kept, and these say what makes it stale.

namespace
{
UnderlayLook
aLook ()
{
  UnderlayLook look;
  look.take = reinterpret_cast<void const *> (0x1);
  look.ticks = 512;
  look.camera = { 0.4f, 0.2f };
  look.width = 768;
  look.height = 672;
  look.colour = juce::Colours::red;
  look.opacity = 0.25f;
  look.thickness = 2.f;
  return look;
}
}

TEST (UnderlayLook, TheSameLookIsNotDrawnAgain)
{
  EXPECT_EQ (aLook (), aLook ());
}

TEST (UnderlayLook, AnotherTakeIsDrawnAgain)
{
  auto look = aLook ();
  look.take = reinterpret_cast<void const *> (0x2);
  EXPECT_NE (look, aLook ());
}

TEST (UnderlayLook, AChangedTakeIsDrawnAgain)
{
  auto look = aLook ();
  look.ticks = 1024;
  EXPECT_NE (look, aLook ());
}

TEST (UnderlayLook, TurningTheViewDrawsItAgain)
{
  auto look = aLook ();
  look.camera.turn = 0.5f;
  EXPECT_NE (look, aLook ());
}

TEST (UnderlayLook, AResizedSphereDrawsItAgain)
{
  auto look = aLook ();
  look.width = 1024;
  EXPECT_NE (look, aLook ());
}

// The skin sets how faint it is and how thick; a skin reloaded mid-take is
// seen at once rather than at the next take.
TEST (UnderlayLook, TheSkinsValuesDrawItAgain)
{
  auto a = aLook ();
  a.opacity = 0.5f;
  auto b = aLook ();
  b.thickness = 3.f;
  auto c = aLook ();
  c.colour = juce::Colours::blue;
  EXPECT_NE (a, aLook ());
  EXPECT_NE (b, aLook ());
  EXPECT_NE (c, aLook ());
}
