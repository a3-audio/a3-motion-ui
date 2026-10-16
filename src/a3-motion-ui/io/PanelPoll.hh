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

#include <array>
#include <cstddef>
#include <cstdint>

namespace a3::panel_poll
{

// Opcodes of the panel's binary protocol (firmware/include/protocol.h).
inline constexpr uint8_t cmdPing = 0x01;
inline constexpr uint8_t cmdGetPots = 0x02;
inline constexpr uint8_t cmdGetEncoders = 0x03;
inline constexpr uint8_t cmdGetButtons = 0x04;
inline constexpr uint8_t cmdSetLed = 0x05;
inline constexpr uint8_t cmdSetAllLeds = 0x06;

// Response sizes, including the leading opcode echo.
inline constexpr std::size_t btnFrameSize = 12; // 1 + 11 packed bytes
inline constexpr std::size_t encFrameSize = 25; // 1 + 8x3 bytes
inline constexpr std::size_t potFrameSize = 9;  // 1 + 4xu16

// One poll cycle: the same three questions every time. The pots are in every
// one -- the firmware filters them on its own scan, so asking costs the wire
// 10 bytes (0.87 ms at 115200 baud) and nothing else.
inline constexpr std::array<uint8_t, 3> request
    = { cmdGetButtons, cmdGetEncoders, cmdGetPots };
inline constexpr std::size_t replySize
    = btnFrameSize + encFrameSize + potFrameSize;
inline constexpr std::size_t potOffset = btnFrameSize + encFrameSize;

}
