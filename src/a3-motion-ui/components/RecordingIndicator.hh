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

#include <cstddef>

namespace a3
{

/** What the status bar's recording mark is doing.
 *
 *  Three states and not two, because pressing REC and the take starting are
 *  not the same moment: a take begins on the next downbeat, which at a slow
 *  tempo is a second away. In that window the hand has already committed and
 *  now has to know *when* to start moving -- too early and the movement is
 *  not in the take, too late and the take opens with nothing in it.
 *
 *  So the window counts. It is the same mark in the same place, in the
 *  channel's colour, blinking on each beat and then filling once the take
 *  runs: one indicator with two states rather than two indicators, so the eye
 *  does not have to move at the moment it can least afford to.
 */
enum class RecordingIndicator
{
  Off,
  CountIn,
  Running
};

/** Which of the three, from what the engine has. Running wins: a blink
 *  carrying on under a running take would be the loudest thing on the screen
 *  saying the least, and this is the state that writes over something that
 *  does not come back. */
RecordingIndicator recordingIndicatorFor (bool scheduled, bool recording);

/** The channel whose row holds `take`, or -1 when there is no take or no row
 *  holds it. The bar shows a take wherever it runs, not only when its clip is
 *  the one on screen (#16), and paints it in that channel's colour -- so the
 *  channel has to come from the take, not from what is open. */
template <typename Rows, typename Pointer>
int
channelHoldingTake (Rows const &rows, Pointer const &take)
{
  if (take == nullptr)
    return -1;

  for (auto channel = 0; channel < static_cast<int> (rows.size ()); ++channel)
    for (auto const &slot : rows[static_cast<std::size_t> (channel)])
      if (slot == take)
        return channel;

  return -1;
}

/** What the bar shows of the take: whose it is, and which of the three. */
struct TakeOnTheBar
{
  int channel = -1;
  RecordingIndicator indicator = RecordingIndicator::Off;
};

/** From the running take and the armed one: the running take wins -- it is
 *  the one writing over something -- and with neither, or one no row holds,
 *  the bar shows nothing. The engine keeps its last take after it finished,
 *  so `running` counts only while `recording`. */
template <typename Rows, typename Pointer>
TakeOnTheBar
takeOnTheBar (Rows const &rows, bool recording, Pointer const &last,
              Pointer const &armed)
{
  auto const running = recording ? last : Pointer{};
  auto const channel
      = channelHoldingTake (rows, running != nullptr ? running : armed);
  if (channel < 0)
    return {};
  return { channel,
           recordingIndicatorFor (armed != nullptr, running != nullptr) };
}

/** Whose colour the bar's take mark wears: the take's channel's, or, with
 *  no take, the open one's. */
constexpr int
barColourChannel (int takeChannel, int shownChannel)
{
  return takeChannel < 0 ? shownChannel : takeChannel;
}

}
