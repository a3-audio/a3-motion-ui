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

namespace a3
{

/** Spread a seed out before the dice see it.
 *
 *  juce::Random takes its seed as its state, so its *first* draw off two
 *  neighbouring seeds is the same draw. Anywhere a seed counts up — one per
 *  gap in a trajectory, one per application of a script — that turns a
 *  scattering of independent decisions into one decision made many times.
 *
 *  Both places here were written that way and both were wrong in the same
 *  visible manner: the fade's bias pot behaved as a switch rather than as a
 *  dial, because at any setting either every gap strayed or none did.
 *
 *  Splitmix64's mixing step, which is exactly what it is for: same seed in,
 *  same seed out — so a random result stays something you can rehearse — and
 *  one apart in, far apart out.
 */
inline juce::int64
spreadSeed (juce::int64 seed)
{
  auto z = static_cast<juce::uint64> (seed) + 0x9e3779b97f4a7c15ull;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;

  return static_cast<juce::int64> (z ^ (z >> 31));
}

}
