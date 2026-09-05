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

#include "ActionLayout.hh"

#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>

namespace a3
{

ActionLayout
layOutActionPage (juce::Rectangle<int> bounds, float headerSize,
                  float bodySize, float potSizeScale)
{
  ActionLayout out;

  auto const padding = juce::jmax (4, bounds.getHeight () / 40);
  auto content = bounds.reduced (padding);

  // The rows first, from the bottom, so what is left over goes to the field
  // that names the action. A name can be shrunk; a control below a fingertip
  // is worth nothing at all.
  auto const gap = juce::jmax (4, content.getWidth () / 60);

  // Three columns for an envelope's three values, and a fourth that carries
  // the row's name. Even columns across every row, so atk sits over atk.
  constexpr int columns = 4;
  auto const cellW = (content.getWidth () - (columns - 1) * gap) / columns;

  auto const wantedH = static_cast<int> (headerSize * 4.f);
  auto const roomForRows
      = juce::jmax (fingertipSize, content.getHeight () / (ActionLayout::numRows + 1));
  auto const rowH = juce::jlimit (fingertipSize, roomForRows, wantedH);

  // Taken from the bottom, so the last one out is the topmost.
  for (int row = ActionLayout::numRows - 1; row >= 0; --row)
    {
      out.rows[static_cast<size_t> (row)] = content.removeFromBottom (
          juce::jmin (rowH, content.getHeight ()));
      if (row > 0)
        content.removeFromBottom (gap);
    }

  for (int row = 0; row < ActionLayout::numRows; ++row)
    {
      auto band = out.rows[static_cast<size_t> (row)];
      for (int i = 0; i < 3; ++i)
        {
          out.controls[static_cast<size_t> (row * 3 + i)]
              = band.removeFromLeft (cellW);
          band.removeFromLeft (gap);
        }

      // Whatever is left of the row is its fourth column, which names it.
      out.rowLabels[static_cast<size_t> (row)] = band;
    }

  content.removeFromBottom (gap);

  // The mode takes the name field's last column, so the two readings a glance
  // needs -- what fires and how -- sit on one line.
  out.actionField = content;
  out.actModeField = out.actionField.removeFromRight (cellW);
  out.actionField.removeFromRight (gap);

  // Generous, because the page is: the knob takes the room a whole page can
  // give it rather than the sliver a third of a bar can.
  auto const knobDiam = juce::jmin (
      knobDiameterForFont (bodySize, potSizeScale) * 2,
      juce::jmax (1, juce::jmin (cellW, out.rows[0].getHeight ()) * 2 / 3));

  auto const columnGap = juce::jmax (2, cellW / 20);
  out.metrics = ControlMetrics{
    knobDiam,
    sharedCaptionSize (bodySize, cellW, columnGap, out.rows[0].getHeight ()),
    bodySize,
  };

  return out;
}

}
