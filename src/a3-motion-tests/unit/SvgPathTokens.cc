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

#include <a3-motion-engine/SvgPathTokens.hh>

#include <chrono>

using namespace a3;

namespace
{
juce::StringArray
tokens (char const *pathData)
{
  return svgPathTokens (juce::String (pathData));
}

juce::StringArray
list (std::initializer_list<char const *> items)
{
  juce::StringArray out;
  for (auto const *item : items)
    out.add (item);
  return out;
}

/** A path the size of a long recorded take: every tick a cubic segment. */
juce::String
aLongTake (int segments)
{
  juce::String d ("M 0.2573 0.3301");
  for (int i = 0; i < segments; ++i)
    d << " C 0.2573 0.3301 0.2619 0.3288 0.2701 0.3254";
  return d;
}
}

TEST (SvgPathTokens, CommandsAndNumbersComeApart)
{
  EXPECT_EQ (tokens ("M 0 0 L 1 1"), list ({ "M", "0", "0", "L", "1", "1" }));
}

// A closed subpath ends in Z and the next begins with M; written straight
// after each other they must still be two commands (7ca538a).
TEST (SvgPathTokens, ACommandRunIntoTheNextIsTwoCommands)
{
  EXPECT_EQ (tokens ("M 0 0 L 1 1ZM 2 2"),
             list ({ "M", "0", "0", "L", "1", "1", "Z", "M", "2", "2" }));
}

TEST (SvgPathTokens, ALetterRunIntoANumberComesApart)
{
  EXPECT_EQ (tokens ("M0.5,-1L1,1"),
             list ({ "M", "0.5", "-1", "L", "1", "1" }));
}

// An exponent's e is not a command.
TEST (SvgPathTokens, AnExponentStaysInItsNumber)
{
  EXPECT_EQ (tokens ("L 1e-3 2E+2"), list ({ "L", "1e-3", "2E+2" }));
}

TEST (SvgPathTokens, NoEmptyTokens)
{
  EXPECT_EQ (tokens ("  M   0 ,, 0 \n\t Z  "), list ({ "M", "0", "0", "Z" }));
}

TEST (SvgPathTokens, NothingIsNothing)
{
  EXPECT_TRUE (tokens ("").isEmpty ());
}

// A long take's path -- 270 KB on the rig, 2026-09-25 -- was re-split on the
// message thread every time its pad icon was redrawn, and splitting it took
// seconds: a String built for every character, and the empty tokens taken out
// one by one, shifting the whole array each time. The touch screen answered
// twenty seconds late. One pass, and a generous bound so a loaded machine does
// not fail it.
TEST (SvgPathTokens, ALongTakeIsSplitInOnePass)
{
  auto const d = aLongTake (6000);
  ASSERT_GT (d.length (), 250000);

  auto const started = std::chrono::steady_clock::now ();
  auto const split = svgPathTokens (d);
  auto const took = std::chrono::steady_clock::now () - started;

  EXPECT_EQ (split.size (), 3 + 6000 * 7);
  EXPECT_LT (std::chrono::duration_cast<std::chrono::milliseconds> (took)
                 .count (),
             150);
}
