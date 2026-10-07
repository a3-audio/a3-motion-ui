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

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

namespace a3
{

/** One channel's strip in FPV, and the sections it is cut into. */
struct FpvStrip
{
  juce::Rectangle<int> whole;
  juce::Rectangle<int> header;      // channel number and AUTO
  juce::Rectangle<int> clip;        // clip name and its state
  juce::Rectangle<int> instruments; // 3D, FREQ, Q
  juce::Rectangle<int> meter;
};

/** FPV's screen: the sphere above, four channel strips below.
 *
 *  A pure function with a test of its own, the way StatusBarLayout is, so
 *  the component lays out into exactly the rectangles the test checks. */
struct FpvLayout
{
  juce::Rectangle<int> sphere; // the top two thirds, where MotionComponent goes
  std::array<FpvStrip, 4> strips;
};

/** The strips' row, as a share of the area's height. */
constexpr float fpvStripsOfHeight = 1.f / 3.f;

/** A strip's sections, top to bottom, as fractions of its height. The meter
 *  takes what is left. */
constexpr float fpvHeaderOfStrip = 0.2f;
constexpr float fpvClipOfStrip = 0.2f;
constexpr float fpvInstrumentsOfStrip = 0.4f;

/** The four strips across `row` (the strips' row alone), sections included --
 *  the same split fpvLayout makes for its bottom third. The first three are
 *  equally wide; the last takes what the division leaves (0..3 px more). A
 *  row too narrow for the gaps gives zero-width strips, never negative. */
std::array<FpvStrip, 4> fpvStripRow (juce::Rectangle<int> row, int gap);

/** Lays `area` out; `gap` is the air between the sphere and the strips and
 *  between neighbouring strips. The gap comes out of the sphere's share. */
FpvLayout fpvLayout (juce::Rectangle<int> area, int gap);

}
