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

  // Three columns for the envelope's three values, and a fourth for the mode
  // on the second row. Even columns across both rows, so atk sits over atk.
  constexpr int columns = 4;
  auto const cellW = (content.getWidth () - (columns - 1) * gap) / columns;

  auto const wantedH = static_cast<int> (headerSize * 4.f);
  auto const roomForTwo = juce::jmax (fingertipSize, content.getHeight () / 3);
  auto const rowH = juce::jlimit (fingertipSize, roomForTwo, wantedH);

  auto const takeRow = [&content, rowH, gap] (bool last) {
    auto row = content.removeFromBottom (juce::jmin (rowH, content.getHeight ()));
    if (!last)
      content.removeFromBottom (gap);
    return row;
  };

  // Taken from the bottom, so the last one out is the topmost: the accent is
  // read first because it is what ACT has always done.
  out.filterRow = takeRow (false);
  out.controlRow = takeRow (true);

  auto const fill = [cellW, gap] (juce::Rectangle<int> row, int count,
                                  juce::Rectangle<int> *into) {
    for (int i = 0; i < count; ++i)
      {
        into[i] = row.removeFromLeft (cellW);
        if (i + 1 < count)
          row.removeFromLeft (gap);
      }
  };

  fill (out.controlRow, 3, out.controls.data ());
  fill (out.filterRow, 4, out.controls.data () + 3);

  // Which envelope each row is, over the row's fourth column -- the accent's
  // is free, and the filter's shares its row with the mode, so it goes above.
  out.accentLabel = { out.controlRow.getX () + 3 * (cellW + gap),
                      out.controlRow.getY (), cellW,
                      out.controlRow.getHeight () / 3 };
  out.filterLabel = { out.controlRow.getX (),
                      out.controlRow.getY () - out.controlRow.getHeight () / 3,
                      cellW, out.controlRow.getHeight () / 3 };

  content.removeFromBottom (gap);
  out.actionField = content;

  // Generous, because the page is: the knob takes the room a whole page can
  // give it rather than the sliver a third of a bar can.
  auto const knobDiam = juce::jmin (
      knobDiameterForFont (bodySize, potSizeScale) * 2,
      juce::jmax (1, juce::jmin (cellW, out.controlRow.getHeight ()) * 2 / 3));

  auto const columnGap = juce::jmax (2, cellW / 20);
  out.metrics = ControlMetrics{
    knobDiam,
    sharedCaptionSize (bodySize, cellW, columnGap,
                       out.controlRow.getHeight ()),
    bodySize,
  };

  return out;
}

}
