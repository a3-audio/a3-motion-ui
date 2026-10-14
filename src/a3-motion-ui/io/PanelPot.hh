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

#include <cstdlib>
#include <optional>

namespace a3
{

/** One of the V3 panel's absolute pots, read against the last position it
 *  reported.
 *
 *  The first reading is where the pot happens to stand when the panel is
 *  first heard -- at a start, a restart, or a panel plugged in later -- and
 *  is not a hand. It is taken as the reference and reported as nothing, so
 *  the 3d it drives is not set to the panel's idea of it every time Motion
 *  comes up (2026-10-14: the truth comes from Core; Motion sends what a hand
 *  changed). The first move beyond the deadband after it is reported, as
 *  any later one is. */
class PanelPot
{
public:
  /** Raw ADC counts of jitter that are not a move. */
  static constexpr int deadband = 8;
  /** The ADC's top count, which reads as 1. */
  static constexpr float fullScale = 4095.f;

  /** The value to report for this reading, or nothing. */
  std::optional<float>
  read (int raw)
  {
    if (!_reported.has_value ())
      {
        _reported = raw;
        return std::nullopt;
      }

    if (std::abs (raw - *_reported) <= deadband)
      return std::nullopt;

    _reported = raw;
    return static_cast<float> (raw) / fullScale;
  }

private:
  std::optional<int> _reported;
};

}
