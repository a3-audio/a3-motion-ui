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
#include <gtest/gtest.h>

#include <limits>

#include <a3-motion-engine/preview/MusicPreview.hh>

using namespace a3;

namespace
{
MusicAhead
buildToDropIn (int bars)
{
  return *musicAheadFrom ("build", "drop", bars, 0.6f);
}
}

TEST (MusicPreview, TheWordsStemDeckSends)
{
  EXPECT_EQ (musicSectionFromWord ("groove"), MusicSection::Groove);
  EXPECT_EQ (musicSectionFromWord ("build"), MusicSection::Build);
  EXPECT_EQ (musicSectionFromWord ("drop"), MusicSection::Drop);
  EXPECT_EQ (musicSectionFromWord ("breakdown"), MusicSection::Breakdown);
  EXPECT_FALSE (musicSectionFromWord ("Drop").has_value ());
  EXPECT_FALSE (musicSectionFromWord ("").has_value ());
  for (auto const section : { MusicSection::Groove, MusicSection::Build,
                              MusicSection::Drop, MusicSection::Breakdown })
    EXPECT_EQ (musicSectionFromWord (wordOf (section)), section);
}

TEST (MusicPreview, APreviewFromItsWords)
{
  auto const ahead = musicAheadFrom ("build", "drop", 4, 0.6f);
  ASSERT_TRUE (ahead.has_value ());
  EXPECT_EQ (ahead->section, MusicSection::Build);
  EXPECT_EQ (ahead->next, MusicSection::Drop);
  EXPECT_EQ (ahead->barsUntilNext, 4);
  EXPECT_FLOAT_EQ (ahead->energy, 0.6f);
}

TEST (MusicPreview, TheSetEndingIsNoNextSection)
{
  auto const ahead = musicAheadFrom ("drop", "end", 8, 1.f);
  ASSERT_TRUE (ahead.has_value ());
  EXPECT_FALSE (ahead->next.has_value ());
}

TEST (MusicPreview, ALoopKeepsItsMinusOne)
{
  EXPECT_EQ (musicAheadFrom ("groove", "groove", -1, 0.5f)->barsUntilNext,
             -1);
}

TEST (MusicPreview, WordsItDoesNotKnowAreNoPreview)
{
  EXPECT_FALSE (musicAheadFrom ("chorus", "drop", 4, 0.5f).has_value ());
  EXPECT_FALSE (musicAheadFrom ("build", "chorus", 4, 0.5f).has_value ());
  EXPECT_FALSE (musicAheadFrom ("none", "none", 0, 0.f).has_value ())
      << "none is asked for first, with isNoMusic";
  EXPECT_TRUE (isNoMusic ("none"));
  EXPECT_FALSE (isNoMusic ("groove"));
}

TEST (MusicPreview, EnergyStaysWithinZeroAndOne)
{
  EXPECT_FLOAT_EQ (musicAheadFrom ("drop", "end", 1, 1.7f)->energy, 1.f);
  EXPECT_FLOAT_EQ (musicAheadFrom ("drop", "end", 1, -0.2f)->energy, 0.f);
}

TEST (MusicPreview, NothingBeforeTheFirst)
{
  MusicPreview preview;
  EXPECT_FALSE (preview.current (10.0, 120.0).has_value ());
}

TEST (MusicPreview, HeldWithItsAgeInBars)
{
  MusicPreview preview;
  preview.receive (buildToDropIn (4), 100.0);

  auto const now = preview.current (103.0, 120.0); // 3 s at 120: 1.5 bars
  ASSERT_TRUE (now.has_value ());
  EXPECT_EQ (now->ahead.section, MusicSection::Build);
  EXPECT_NEAR (now->ageBars, 1.5, 1e-9);
}

// Review Focus 2: StemDeck gone without a word.
TEST (MusicPreview, StaleAfterFourBarsWithoutANewOne)
{
  MusicPreview preview;
  preview.receive (buildToDropIn (4), 100.0);

  EXPECT_TRUE (preview.current (108.0, 120.0).has_value ()) << "4 bars: still";
  EXPECT_FALSE (preview.current (108.1, 120.0).has_value ());
  EXPECT_TRUE (preview.current (115.9, 60.0).has_value ())
      << "at half the tempo a bar lasts twice as long";
}

TEST (MusicPreview, WithoutATempoItCountsAt120)
{
  MusicPreview preview;
  preview.receive (buildToDropIn (4), 100.0);
  EXPECT_TRUE (preview.current (108.0, 0.0).has_value ());
  EXPECT_FALSE (preview.current (108.1, 0.0).has_value ());
}

TEST (MusicPreview, ANewOneReplacesTheOld)
{
  MusicPreview preview;
  preview.receive (buildToDropIn (4), 100.0);
  preview.receive (buildToDropIn (3), 102.0);
  auto const now = preview.current (102.0, 120.0);
  ASSERT_TRUE (now.has_value ());
  EXPECT_EQ (now->ahead.barsUntilNext, 3);
  EXPECT_DOUBLE_EQ (now->ageBars, 0.0);
}

TEST (MusicPreview, NoneClearsIt)
{
  MusicPreview preview;
  preview.receive (buildToDropIn (4), 100.0);
  preview.clear ();
  EXPECT_FALSE (preview.current (100.0, 120.0).has_value ());
}

TEST (MusicPreview, AClockGoingBackIsAgeZero)
{
  MusicPreview preview;
  preview.receive (buildToDropIn (4), 100.0);
  EXPECT_DOUBLE_EQ (preview.current (99.0, 120.0)->ageBars, 0.0);
}

TEST (MusicPreview, ANumberThatIsNotANumberIsNoPreview)
{
  auto const nan = std::numeric_limits<float>::quiet_NaN ();
  auto const inf = std::numeric_limits<float>::infinity ();
  EXPECT_FALSE (musicAheadFrom ("drop", "end", 1, nan).has_value ());
  EXPECT_FALSE (musicAheadFrom ("drop", "end", 1, inf).has_value ());
  EXPECT_FALSE (musicAheadFrom ("drop", "end", 1, -inf).has_value ());
}

TEST (MusicPreview, NothingBelowMinusOneMeansAnything)
{
  EXPECT_FALSE (musicAheadFrom ("drop", "end", -2, 0.5f).has_value ());
  EXPECT_TRUE (musicAheadFrom ("drop", "end", -1, 0.5f).has_value ());
  EXPECT_TRUE (musicAheadFrom ("drop", "end", 0, 0.5f).has_value ());
}
