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

#include "ColumnBreak.hh"

namespace a3
{

namespace
{
int
rowsFor (int count, int columns)
{
  return (count + columns - 1) / columns;
}
}

ColumnBreak
breakColumns (juce::Rectangle<int> area, int count, int minimumWidth,
              int minimumHeight)
{
  if (count <= 0 || area.isEmpty ())
    return { juce::jmax (1, count), 1, false };

  // Halved rather than decremented: two rows of two reads as a block, three
  // and one reads as a mistake. The device has four channels and every
  // sensible break of four is a power of two.
  for (auto columns = count; columns > 1; columns /= 2)
    {
      auto const rows = rowsFor (count, columns);
      if (area.getWidth () / columns >= minimumWidth
          && area.getHeight () / rows >= minimumHeight)
        return { columns, rows, true };
    }

  auto const rows = count;
  auto const fits = area.getWidth () >= minimumWidth
                    && area.getHeight () / rows >= minimumHeight;
  return { 1, rows, fits };
}

juce::Rectangle<int>
cellIn (juce::Rectangle<int> area, ColumnBreak broken, int index)
{
  if (area.isEmpty () || broken.columns < 1 || broken.rows < 1 || index < 0
      || index >= broken.columns * broken.rows)
    return {};

  // Integer division leaves a remainder against the right and bottom edges,
  // the same way ClipSettingsLayout's colW does. Measured from the left and
  // top for the same reason it is there: taken from the far edge the cells
  // come out a few pixels off the grid above them, and a row that nearly
  // lines up reads as a mistake where one that lines up exactly reads as
  // structure.
  auto const cellW = area.getWidth () / broken.columns;
  auto const cellH = area.getHeight () / broken.rows;

  return { area.getX () + cellW * (index % broken.columns),
           area.getY () + cellH * (index / broken.columns), cellW, cellH };
}

}
