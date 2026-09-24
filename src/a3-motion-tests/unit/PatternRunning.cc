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

#include <a3-motion-engine/PatternRunning.hh>

using namespace a3;
using Status = Pattern::Status;

// A pattern is running while the blob is being moved along it. Two of the
// three are obvious; the third is the one this exists for.
TEST (PatternRunning, TheObviousTwo)
{
  EXPECT_TRUE (patternIsRunning (Status::Playing));
  EXPECT_TRUE (patternIsRunning (Status::Recording));
}

// **The bug, in one line.** Pressing play on another clip of the same channel
// schedules the new one for the next downbeat and marks the old one
// ScheduledForIdle -- and the old one goes on playing until that downbeat
// arrives. The engine keeps moving its blob; the sphere stopped drawing its
// line the instant the key went down, because the draw loop knew only Playing
// and Recording. A blob running along a line that is not there.
//
// Reported at the device on 2026-09-24: "der blob [faehrt] die trajektorie vom
// vorherigen clip weiter aber die trajektorie [wird] ausgeblendet."
TEST (PatternRunning, AClipWaitingToStopIsStillRunning)
{
  EXPECT_TRUE (patternIsRunning (Status::ScheduledForIdle));
}

// And the other half of the same moment: the clip that was just pressed has
// **not** started. Drawing it here would put the new line on the sphere a
// downbeat early, which is the opposite mistake -- "sichtbar bleiben bis der
// neue clip startet" cuts both ways.
TEST (PatternRunning, AClipWaitingToStartIsNotRunningYet)
{
  EXPECT_FALSE (patternIsRunning (Status::ScheduledForPlaying));
  EXPECT_FALSE (patternIsRunning (Status::ScheduledForRecording));
}

// Nothing is moving along these.
TEST (PatternRunning, TheStillOnes)
{
  EXPECT_FALSE (patternIsRunning (Status::Empty));
  EXPECT_FALSE (patternIsRunning (Status::Idle));
}
