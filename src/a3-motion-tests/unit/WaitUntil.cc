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

// The helper the timing-flaky tests wait with. Tested itself, because a wait
// that waits wrongly hides exactly what it was brought in to find: one that
// returned true too eagerly would turn a real failure into a green run.

#include "WaitUntil.hh"

#include <gtest/gtest.h>

using namespace a3;

TEST (WaitUntil, AConditionThatIsAlreadyTrueCostsNoWait)
{
  auto const started = juce::Time::getMillisecondCounter ();

  EXPECT_TRUE (waitUntil ([] { return true; }, 4000));

  EXPECT_LT (juce::Time::getMillisecondCounter () - started, 50u)
      << "it slept even though there was nothing to wait for";
}

TEST (WaitUntil, ItWaitsUntilTheConditionTurnsTrue)
{
  auto const turnsTrueAt = juce::Time::getMillisecondCounter () + 120;

  EXPECT_TRUE (waitUntil ([turnsTrueAt] {
    return juce::Time::getMillisecondCounter () >= turnsTrueAt;
  }));
}

// The half that matters: a condition that never comes true has to come back
// false, and within the bound. A helper that hung would turn one broken test
// into a build nobody can finish.
TEST (WaitUntil, ItGivesUpAndSaysSo)
{
  auto const started = juce::Time::getMillisecondCounter ();

  EXPECT_FALSE (waitUntil ([] { return false; }, 100));

  auto const took = juce::Time::getMillisecondCounter () - started;
  EXPECT_GE (took, 100u);
  EXPECT_LT (took, 2000u) << "it waited far past its own timeout";
}

// Asked once more after the deadline rather than assumed false: on a machine
// that stalled for the whole timeout, the honest answer is what the condition
// says now, not what it said before the last sleep.
TEST (WaitUntil, TheLastWordIsTheConditionsAndNotTheClocks)
{
  // Mit einer Zeitgrenze von null wird genau zweimal gefragt: einmal vor dem
  // Warten, einmal hinter der Frist. Wird die Bedingung dazwischen wahr, ist
  // die Antwort wahr.
  int calls = 0;
  auto const ready = [&calls] { return ++calls >= 2; };

  EXPECT_TRUE (waitUntil (ready, 0));
  EXPECT_EQ (2, calls);
}
