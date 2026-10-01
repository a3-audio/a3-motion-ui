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

#include <a3-motion-ui/components/EncoderMap.hh>

using namespace a3;

namespace
{
constexpr int top = 0;
constexpr int bottom = 1;

EncoderTarget
turn (BarPage page, int column, int row, bool clicked = false,
      bool shift = false)
{
  return encoderTarget (page, column, row, clicked, shift);
}

bool
isControl (EncoderTarget const &t, int section, int sub)
{
  return t.kind == EncoderTarget::Kind::Control && t.section == section
         && t.sub == sub;
}
}

// The eight encoders stand four by two, a column per channel -- and so do the
// fields of CLIP, MOTION, REC and CHMIX since 2026-09-27. Each turns the
// field it stands under.

TEST (EncoderMap, OnClipEachEncoderTurnsTheFieldAboveIt)
{
  EXPECT_TRUE (isControl (turn (BarPage::Clip, 0, top), 0, 1)) << "clip";
  EXPECT_TRUE (isControl (turn (BarPage::Clip, 1, top), 0, 2)) << "dir";
  EXPECT_TRUE (isControl (turn (BarPage::Clip, 0, bottom), 0, 0)) << "shape";
  EXPECT_TRUE (isControl (turn (BarPage::Clip, 1, bottom), 0, 3)) << "end";

  int index = 0;
  for (int row : { top, bottom })
    for (int column : { 2, 3 })
      {
        auto const t = turn (BarPage::Clip, column, row);
        EXPECT_EQ (t.kind, EncoderTarget::Kind::Speed);
        EXPECT_EQ (t.speed, index++) << "column " << column << " row " << row;
      }
}

TEST (EncoderMap, OnMotionTheTopRowClicksBetweenSweepsAndStandingValues)
{
  // spin swell strX strY, a click: rot reach sqzX sqzY.
  int const sweeps[] = { 1, 3, 5, 7 };
  int const standing[] = { 0, 2, 4, 6 };
  for (int column = 0; column < 4; ++column)
    {
      EXPECT_TRUE (isControl (turn (BarPage::Motion, column, top), 2,
                              sweeps[column]))
          << "column " << column;
      EXPECT_TRUE (isControl (turn (BarPage::Motion, column, top, true), 2,
                              standing[column]))
          << "column " << column;
      EXPECT_TRUE (encoderPressClicks (BarPage::Motion, column, top));
    }
}

TEST (EncoderMap, OnMotionTheBottomRowClicksBetweenSwayAndElv)
{
  // sway clip-top, a click: elv clip-bottom. Elevation: 0 clip-bottom,
  // 1 clip-top, 2 sway, 3 elv.
  EXPECT_TRUE (isControl (turn (BarPage::Motion, 0, bottom), 1, 2));
  EXPECT_TRUE (isControl (turn (BarPage::Motion, 1, bottom), 1, 1));
  EXPECT_TRUE (isControl (turn (BarPage::Motion, 0, bottom, true), 1, 3));
  EXPECT_TRUE (isControl (turn (BarPage::Motion, 1, bottom, true), 1, 0));

  // tswp and rswp at rest, a click: tilt and roll (2026-09-27).
  EXPECT_TRUE (isControl (turn (BarPage::Motion, 2, bottom), 2, 11));
  EXPECT_TRUE (isControl (turn (BarPage::Motion, 2, bottom, true), 2, 10));
  EXPECT_TRUE (isControl (turn (BarPage::Motion, 3, bottom), 2, 13));
  EXPECT_TRUE (isControl (turn (BarPage::Motion, 3, bottom, true), 2, 12));
  EXPECT_TRUE (encoderPressClicks (BarPage::Motion, 2, bottom));
}

TEST (EncoderMap, OnRecTheRecModeAndFadeThenBias)
{
  EXPECT_TRUE (isControl (turn (BarPage::Record, 0, top), 0, 1)) << "clip";
  EXPECT_TRUE (isControl (turn (BarPage::Record, 1, top), 3, 0)) << "recmode";
  EXPECT_TRUE (isControl (turn (BarPage::Record, 0, bottom), 0, 0))
      << "shape";
  EXPECT_TRUE (isControl (turn (BarPage::Record, 1, bottom), 2, 8)) << "fade";
  EXPECT_TRUE (isControl (turn (BarPage::Record, 1, bottom, true), 2, 9))
      << "bias, a click on";
  EXPECT_TRUE (encoderPressClicks (BarPage::Record, 1, bottom));
  EXPECT_FALSE (encoderPressClicks (BarPage::Record, 1, top));

  EXPECT_EQ (turn (BarPage::Record, 3, bottom).kind,
             EncoderTarget::Kind::Speed);
  EXPECT_EQ (turn (BarPage::Record, 3, bottom).speed, 3);
}

TEST (EncoderMap, OnChmixTheShownChannelsEightFields)
{
  MixerControl const eq[] = { MixerControl::Gain, MixerControl::EqHigh,
                              MixerControl::EqMid, MixerControl::EqLow };
  for (int column = 0; column < 4; ++column)
    {
      auto const t = turn (BarPage::Mixer, column, top);
      EXPECT_EQ (t.kind, EncoderTarget::Kind::Mixer);
      EXPECT_EQ (t.mixer, eq[column]);
    }

  // SEND PFL FX VOL since 2026-09-27: 3D, FREQ and Q left CHMIX for the
  // channel row. PFL and FX are keys, a press flips them.
  EXPECT_EQ (turn (BarPage::Mixer, 0, bottom).kind, EncoderTarget::Kind::Mixer);
  EXPECT_EQ (turn (BarPage::Mixer, 0, bottom).mixer, MixerControl::AuxSend);
  EXPECT_EQ (turn (BarPage::Mixer, 1, bottom).kind,
             EncoderTarget::Kind::MixerKey);
  EXPECT_EQ (turn (BarPage::Mixer, 1, bottom).mixer, MixerControl::Pfl);
  EXPECT_EQ (turn (BarPage::Mixer, 2, bottom).kind,
             EncoderTarget::Kind::MixerKey);
  EXPECT_EQ (turn (BarPage::Mixer, 2, bottom).mixer, MixerControl::Fx);
  EXPECT_EQ (turn (BarPage::Mixer, 3, bottom).kind, EncoderTarget::Kind::Mixer);
  EXPECT_EQ (turn (BarPage::Mixer, 3, bottom).mixer, MixerControl::Volume);
}

// FREQ and Q of the column's channel, as before the fields: with Shift held
// on every page, and without it on the pages that have no eight fields.
TEST (EncoderMap, ShiftOrAPageWithoutFieldsTurnsTheColumnsFreqAndQ)
{
  for (auto const page : { BarPage::Clip, BarPage::Motion, BarPage::Record,
                           BarPage::Mixer })
    for (int column = 0; column < 4; ++column)
      {
        auto const t = turn (page, column, top, false, true);
        EXPECT_EQ (t.kind, EncoderTarget::Kind::ColumnChannelPot);
        EXPECT_EQ (t.pot, ChannelPot::Freq);
        EXPECT_EQ (turn (page, column, bottom, false, true).pot,
                   ChannelPot::Q);
      }

}

// ACTION's own encoders are in ActionEncoders.cc.

// A press on a length key's encoder chooses that length, as a tap does.
TEST (EncoderMap, APressOnALengthChoosesIt)
{
  EXPECT_FALSE (encoderPressClicks (BarPage::Clip, 2, top));
  EXPECT_EQ (turn (BarPage::Clip, 2, top).kind, EncoderTarget::Kind::Speed);
}

// ── Which knobs the encoders are on, marked (2026-09-27) ───────────────────

namespace
{
bool
marks (std::vector<std::pair<int, int> > const &marked, int section, int sub)
{
  for (auto const &[s, u] : marked)
    if (s == section && u == sub)
      return true;
  return false;
}
}

// Where an encoder has two things under it, the one it turns now is marked:
// a press moves the mark.
TEST (EncoderMap, OnMotionTheRowsTheEncodersTurnAreMarked)
{
  EncoderClicks clicked{};
  auto const before = encoderMarks (BarPage::Motion, clicked);

  EXPECT_EQ (before.size (), 8u)
      << "four sweeps, sway, clip-top, and the tilt and roll sweeps";
  EXPECT_TRUE (marks (before, 2, 1)) << "spin";
  EXPECT_TRUE (marks (before, 1, 2)) << "sway";
  EXPECT_FALSE (marks (before, 2, 0)) << "rot waits for a click";

  clicked[0][0] = true;
  auto const after = encoderMarks (BarPage::Motion, clicked);
  EXPECT_TRUE (marks (after, 2, 0)) << "rot, after the click";
  EXPECT_FALSE (marks (after, 2, 1)) << "spin let go";
}

TEST (EncoderMap, OnRecFadeOrBiasIsMarked)
{
  EncoderClicks clicked{};
  auto const fade = encoderMarks (BarPage::Record, clicked);
  ASSERT_EQ (fade.size (), 1u);
  EXPECT_TRUE (marks (fade, 2, 8));

  clicked[1][1] = true;
  auto const bias = encoderMarks (BarPage::Record, clicked);
  ASSERT_EQ (bias.size (), 1u);
  EXPECT_TRUE (marks (bias, 2, 9));
}

TEST (EncoderMap, APageWithoutClicksMarksNothing)
{
  EXPECT_TRUE (encoderMarks (BarPage::Clip, {}).empty ());
  EXPECT_TRUE (encoderMarks (BarPage::Mixer, {}).empty ());
}

// Kept in the settings file as one number per page, so they come back after a
// restart.
TEST (EncoderMap, ClicksSurviveAsANumber)
{
  EncoderClicks clicked{};
  clicked[0][0] = clicked[3][1] = clicked[1][1] = true;

  EXPECT_EQ (encoderClicksFromMask (encoderClicksMask (clicked)), clicked);
  EXPECT_EQ (encoderClicksMask ({}), 0);
}
