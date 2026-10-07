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

#pragma once

#include <optional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace a3
{

/** Which way a ship points: along its last movement that was long enough to
 *  read, so a ship standing still keeps its heading instead of spinning. */
class ShipHeading
{
public:
  /** `minimumStep` in screen units; below it the heading is kept. */
  void update (juce::Point<float> at, float minimumStep);
  /** Forget the last point (the ship vanished); the heading is kept. */
  void lose ();
  float radians () const; // 0 = nose up the screen, clockwise

private:
  std::optional<juce::Point<float> > _last;
  float _radians = 0.f;
};

/** The ship's outline: a dart, nose at `length / 2` ahead of `centre`
 *  along `radians`, tail notch behind. */
juce::Path shipPath (juce::Point<float> centre, float radians, float length);

constexpr float shipWidthOfLength = 0.6f;
constexpr float shipNotchOfLength = 0.2f; // how deep the tail is cut in
constexpr float shipStepOfLength = 0.1f;  // minimumStep the sphere passes
constexpr float shipLengthOfBlob = 1.6f;  // ship length / blob diameter

}
