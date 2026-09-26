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

#include <JuceHeader.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace a3
{

/** What the key LEDs draw together, estimated the way the controller's
 *  firmware estimates it (a3-motion firmware/include/led_budget.h) -- so the
 *  app can see a picture coming that the firmware would dim (#21).
 *
 *  The datasheet's figures: about 20 mA per colour at full brightness and
 *  about 1 mA for each chip whatever it shows. Kept in step with the firmware
 *  by hand; the two repositories cannot share a header. */
constexpr std::uint32_t ledMaPerChannelAtFull = 20;
constexpr std::uint32_t ledMaIdle = 1;
/** The firmware's LED_BUDGET_MA (a3-motion firmware/include/config.h): over
 *  it, the controller dims every key together. Kept in step by hand. */
constexpr std::uint32_t firmwareLedBudgetMa = 1300;

class LedLoad
{
public:
  explicit LedLoad (std::size_t count);

  /** The colour `led` was last told to show. An id outside the strip is
   *  ignored. */
  void set (std::size_t led, juce::Colour colour);

  /** The estimate for everything as it is now, in mA. */
  std::uint32_t estimateMilliamps () const;

  /** The highest estimate there has been. */
  std::uint32_t peakMilliamps () const;

  /** True once for every new peak at least `step` mA above the last one
   *  taken: what is worth a line in the log. */
  bool takeNewPeak (std::uint32_t step);

  /** True once each time the estimate goes over firmwareLedBudgetMa: the
   *  picture the firmware will dim. */
  bool takeBudgetCrossing ();

private:
  std::vector<std::uint32_t> _channels;
  std::uint64_t _total = 0;
  std::uint32_t _peak = 0;
  std::uint32_t _reported = 0;
  bool _wasOver = false;
};

}
