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

#include "FpvLayout.hh"

#include <algorithm>

namespace a3
{

std::array<FpvStrip, 4>
fpvStripRow (juce::Rectangle<int> row, int gap)
{
  std::array<FpvStrip, 4> strips{};
  auto const stripWidth = std::max (0, (row.getWidth () - 3 * gap) / 4);
  for (size_t ch = 0; ch < strips.size (); ++ch)
    {
      auto &strip = strips[ch];
      strip.whole = ch == 3 ? row : row.removeFromLeft (stripWidth);
      if (ch < 3)
        row.removeFromLeft (gap);

      auto sections = strip.whole;
      auto const h = static_cast<float> (strip.whole.getHeight ());
      strip.header
          = sections.removeFromTop (juce::roundToInt (h * fpvHeaderOfStrip));
      strip.clip
          = sections.removeFromTop (juce::roundToInt (h * fpvClipOfStrip));
      strip.instruments = sections.removeFromTop (
          juce::roundToInt (h * fpvInstrumentsOfStrip));
      strip.meter = sections;
    }
  return strips;
}

FpvLayout
fpvLayout (juce::Rectangle<int> area, int gap)
{
  FpvLayout out{};
  if (area.isEmpty ())
    return out;

  auto rest = area;
  auto const stripsHeight = juce::roundToInt (
      static_cast<float> (area.getHeight ()) * fpvStripsOfHeight);
  auto const stripsRow = rest.removeFromBottom (stripsHeight);
  rest.removeFromBottom (gap);
  out.sphere = rest;
  out.strips = fpvStripRow (stripsRow, gap);
  return out;
}

}
