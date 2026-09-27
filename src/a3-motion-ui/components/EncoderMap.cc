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

#include "EncoderMap.hh"

namespace a3
{

namespace
{
EncoderTarget
control (int section, int sub)
{
  EncoderTarget t;
  t.kind = EncoderTarget::Kind::Control;
  t.section = section;
  t.sub = sub;
  return t;
}

/** CLIP and REC share the lengths: the two right columns, two by two. */
EncoderTarget
speedKey (int column, int row)
{
  EncoderTarget t;
  t.kind = EncoderTarget::Kind::Speed;
  t.speed = row * 2 + (column - 2);
  return t;
}

EncoderTarget
columnChannelPot (int row)
{
  EncoderTarget t;
  t.kind = EncoderTarget::Kind::ColumnChannelPot;
  t.pot = row == 0 ? ChannelPot::Freq : ChannelPot::Q;
  return t;
}

// Section and sub indices as the bar numbers them: Shape 0 (0 picture,
// 1 clip field, 2 dir, 3 end); Elevation 1 (0 clip-bottom, 1 clip-top,
// 2 sway, 3 elv); Motion 2 (0 rot, 1 spin, 2 reach, 3 swell, 4 sqzX,
// 5 strX, 6 sqzY, 7 strY, 8 fade, 9 bias); global 3 (0 rec mode).
constexpr int shape = 0;
constexpr int elevation = 1;
constexpr int motion = 2;
constexpr int global = 3;

EncoderTarget
onClip (int column, int row)
{
  if (column >= 2)
    return speedKey (column, row);
  if (column == 0)
    return control (shape, row == 0 ? 1 : 0);
  return control (shape, row == 0 ? 2 : 3);
}

EncoderTarget
onRecord (int column, int row, bool clicked)
{
  if (column >= 2)
    return speedKey (column, row);
  if (column == 0)
    return control (shape, row == 0 ? 1 : 0);
  if (row == 0)
    return control (global, 0);
  return control (motion, clicked ? 9 : 8);
}

EncoderTarget
onMotion (int column, int row, bool clicked)
{
  if (row == 0)
    {
      // spin swell strX strY; a click: rot reach sqzX sqzY.
      int const sweeps[] = { 1, 3, 5, 7 };
      int const standing[] = { 0, 2, 4, 6 };
      return control (motion, clicked ? standing[column] : sweeps[column]);
    }

  // sway clip-top tswp rswp; a click: elv clip-bottom tilt roll.
  if (column == 0)
    return control (elevation, clicked ? 3 : 2);
  if (column == 1)
    return control (elevation, clicked ? 0 : 1);
  if (column == 2)
    return control (motion, clicked ? 10 : 11);
  return control (motion, clicked ? 12 : 13);
}

EncoderTarget
onMixer (int column, int row)
{
  EncoderTarget t;
  if (row == 0)
    {
      MixerControl const eq[] = { MixerControl::Gain, MixerControl::EqHigh,
                                  MixerControl::EqMid, MixerControl::EqLow };
      t.kind = EncoderTarget::Kind::Mixer;
      t.mixer = eq[column];
      return t;
    }
  // SEND PFL FX VOL, as CHMIX's fields stand (2026-09-27).
  MixerControl const bottom[] = { MixerControl::FxSend, MixerControl::Pfl,
                                  MixerControl::Fx, MixerControl::Volume };
  t.mixer = bottom[column];
  t.kind = mixerControlIsAToggle (t.mixer) ? EncoderTarget::Kind::MixerKey
                                           : EncoderTarget::Kind::Mixer;
  return t;
}
}

EncoderTarget
encoderTarget (BarPage page, int column, int row, bool clicked, bool shift)
{
  if (column < 0 || column > 3 || row < 0 || row > 1)
    return {};
  if (shift)
    return columnChannelPot (row);

  switch (page)
    {
    case BarPage::Clip: return onClip (column, row);
    case BarPage::Record: return onRecord (column, row, clicked);
    case BarPage::Motion: return onMotion (column, row, clicked);
    case BarPage::Mixer: return onMixer (column, row);
    default: return columnChannelPot (row);
    }
}

bool
encoderPressClicks (BarPage page, int column, int row)
{
  if (page == BarPage::Motion)
    return true;
  if (page == BarPage::Record)
    return column == 1 && row == 1;
  return false;
}

std::vector<std::pair<int, int> >
encoderMarks (BarPage page, EncoderClicks const &clicked)
{
  std::vector<std::pair<int, int> > marked;
  for (int column = 0; column < 4; ++column)
    for (int row = 0; row < 2; ++row)
      {
        if (!encoderPressClicks (page, column, row))
          continue;
        auto const target = encoderTarget (
            page, column, row,
            clicked[static_cast<std::size_t> (column)]
                   [static_cast<std::size_t> (row)],
            false);
        if (target.kind == EncoderTarget::Kind::Control)
          marked.emplace_back (target.section, target.sub);
      }
  return marked;
}

int
encoderClicksMask (EncoderClicks const &clicked)
{
  int mask = 0;
  for (int column = 0; column < 4; ++column)
    for (int row = 0; row < 2; ++row)
      if (clicked[static_cast<std::size_t> (column)]
                 [static_cast<std::size_t> (row)])
        mask |= 1 << (column * 2 + row);
  return mask;
}

EncoderClicks
encoderClicksFromMask (int mask)
{
  EncoderClicks clicked{};
  for (int column = 0; column < 4; ++column)
    for (int row = 0; row < 2; ++row)
      clicked[static_cast<std::size_t> (column)]
             [static_cast<std::size_t> (row)]
          = (mask >> (column * 2 + row)) & 1;
  return clicked;
}

}
