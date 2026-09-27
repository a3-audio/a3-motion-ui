/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include <a3-motion-ui/components/RecArming.hh>

using namespace a3;

// ● from idle arms the shown slot rather than recording at once: REC PAUSE,
// where the take is set up, and ▶ starts it (2026-09-26).
TEST (RecArming, RecFromIdleArms)
{
  EXPECT_EQ (recKeyAction (false, false, false), RecKeyAction::Arm);
}

// ● again while armed takes it back: nothing was written.
TEST (RecArming, RecWhileArmedDisarms)
{
  EXPECT_EQ (recKeyAction (false, false, true), RecKeyAction::Disarm);
}

// While a take runs or waits for its downbeat, ● ends it -- as before.
TEST (RecArming, RecWhileATakeIsUnderwayEndsIt)
{
  EXPECT_EQ (recKeyAction (false, true, false), RecKeyAction::EndTake);
}

// An unsaved take on the shown slot turns ● into SAVE, and SAVE wins.
TEST (RecArming, SaveComesFirst)
{
  EXPECT_EQ (recKeyAction (true, false, false), RecKeyAction::Save);
  EXPECT_EQ (recKeyAction (true, false, true), RecKeyAction::Save);
}

// Armed, ▶ starts the take; otherwise it plays and pauses as it always did.
TEST (RecArming, PlayStartsTheTakeWhileArmed)
{
  EXPECT_EQ (playKeyAction (true), PlayKeyAction::StartTake);
  EXPECT_EQ (playKeyAction (false), PlayKeyAction::PlayPause);
}

// ■ while armed is a way out that writes nothing.
TEST (RecArming, StopWhileArmedDisarms)
{
  EXPECT_TRUE (stopKeyDisarms (true));
  EXPECT_FALSE (stopKeyDisarms (false));
}
