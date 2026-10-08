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

#pragma once

#include <gtest/gtest.h>

#include <JuceHeader.h>

namespace a3::test
{

/** The body of one function in A3MotionUIComponent.cc, from its signature to
 *  the closing brace at the start of a line.
 *
 *  The component cannot be built in a test (it wants a config, a GL context
 *  and the engine), so what it must call is read from its source -- the way
 *  for wiring that no unit can reach. */
inline juce::String
uiComponentBodyOf (juce::String const &signature)
{
  auto const ui = juce::File (A3_UI_SOURCE_DIR)
                      .getChildFile ("components/A3MotionUIComponent.cc");
  auto const text = ui.loadFileAsString ();
  EXPECT_TRUE (text.isNotEmpty ()) << ui.getFullPathName ();

  return text.fromFirstOccurrenceOf (signature, false, false)
      .upToFirstOccurrenceOf ("\n}\n", false, false);
}

}
