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

#include <a3-motion-ui/components/ActionLayout.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>

using namespace a3;

namespace
{
constexpr float headerSize = 18.f;
}

// It has a whole page, so its controls have no excuse for being small. The
// clip bar's sections fight over a third of the bar each; this one fights with
// nobody.
TEST (ActionLayout, EveryControlIsWellOverAFingertip)
{
  for (int width : { 480, 640, 768, 1024 })
    for (int height : { 160, 250, 400 })
      {
        auto const l
            = layOutActionPage ({ 0, 0, width, height }, headerSize, 14.f, 1.f);

        for (size_t i = 0; i < l.controls.size (); ++i)
          {
            EXPECT_GE (l.controls[i].getWidth (), fingertipSize)
                << "control " << i << " at " << width << "x" << height;
            EXPECT_GE (l.controls[i].getHeight (), fingertipSize)
                << "control " << i << " at " << width << "x" << height;
          }
      }
}

// Reading order, left to right, nothing overlapping.
TEST (ActionLayout, TheControlsReadLeftToRight)
{
  auto const l = layOutActionPage ({ 0, 0, 768, 300 }, headerSize, 14.f, 1.f);

  for (size_t i = 1; i < l.controls.size (); ++i)
    EXPECT_GE (l.controls[i].getX (), l.controls[i - 1].getRight ())
        << "control " << i << " overlaps its neighbour";

  EXPECT_LE (l.controls.back ().getRight (), 768);
}

// The picture is above the controls and clear of them: a curve with a knob
// drawn over it is a curve you cannot read.
TEST (ActionLayout, ThePictureSitsClearAboveTheControls)
{
  auto const l = layOutActionPage ({ 0, 0, 768, 300 }, headerSize, 14.f, 1.f);

  ASSERT_FALSE (l.envelopeGraphic.isEmpty ());
  for (auto const &control : l.controls)
    {
      EXPECT_LE (l.envelopeGraphic.getBottom (), control.getY ());
      EXPECT_FALSE (l.envelopeGraphic.intersects (control));
    }
}

// Everything inside the page it was given.
TEST (ActionLayout, NothingEscapesThePage)
{
  auto const page = juce::Rectangle<int>{ 0, 0, 640, 200 };
  auto const l = layOutActionPage (page, headerSize, 14.f, 1.f);

  EXPECT_TRUE (page.contains (l.envelopeGraphic));
  for (auto const &control : l.controls)
    EXPECT_TRUE (page.contains (control));
}
