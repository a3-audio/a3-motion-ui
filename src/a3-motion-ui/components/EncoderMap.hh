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

#pragma once

#include <a3-motion-ui/components/ClipSettingsLayout.hh>
#include <a3-motion-ui/components/MixerControls.hh>

#include <array>
#include <utility>
#include <vector>

namespace a3
{

/** What one of the panel's eight encoders turns (2026-09-27).
 *
 *  The encoders stand four by two, a column per channel, and so do the fields
 *  of CLIP, MOTION, REC and CHMIX: without Shift each encoder turns the field
 *  it stands under. With Shift, and on the pages without eight fields, the
 *  column's channel's FREQ (top) and Q (bottom), as before. */
struct EncoderTarget
{
  enum class Kind
  {
    None,
    /** A clip bar control, stepped by the value handler. */
    Control,
    /** A length key: a turn gives it another length, a press chooses it. */
    Speed,
    /** One of the shown channel's mixer pots (CHMIX). */
    Mixer,
    /** One of the shown channel's two keys, PFL or FX (CHMIX): a press
     *  flips it, a turn does nothing. */
    MixerKey,
    /** ACTION: walks the list's highlight; a press assigns it. */
    ActionList,
    /** ACTION: steps the chosen action button, A1..A6. */
    ActionButton,
    /** ACTION: steps the chosen button's mode; a press is EDIT. */
    ActionMode,
    /** ACTION: steps what the chosen button fires after; a press switches
     *  the card between AUDIO and MOTION. */
    ActionAfter,
    /** FREQ or Q of the encoder's own column's channel. */
    ColumnChannelPot,
  };

  Kind kind = Kind::None;
  int section = 0;
  int sub = 0;
  int speed = 0;
  MixerControl mixer = MixerControl::Gain;
  ChannelPot pot = ChannelPot::Freq;
};

/** `row` 0 is the upper encoder of the column, 1 the lower. `clicked` is the
 *  encoder's own click state -- see encoderPressClicks(). */
EncoderTarget encoderTarget (BarPage page, int column, int row, bool clicked,
                             bool shift);

/** Whether a press on this encoder switches what it turns: MOTION's rows and
 *  REC's fade|bias. */
bool encoderPressClicks (BarPage page, int column, int row);

/** Each encoder's click, [column][row]. */
using EncoderClicks = std::array<std::array<bool, 2>, 4>;

/** The controls (section, sub) the encoders turn now where a press switches
 *  between two -- the ones the bar marks, so it is clear which of the pair an
 *  encoder is on. */
std::vector<std::pair<int, int> > encoderMarks (BarPage page,
                                                EncoderClicks const &clicked);

/** One number for a page's clicks, for the settings file: bit column*2+row. */
int encoderClicksMask (EncoderClicks const &clicked);
EncoderClicks encoderClicksFromMask (int mask);

}
